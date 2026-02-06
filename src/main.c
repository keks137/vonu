#include <stddef.h>
#include "block.h"
#include "core.h"
#include "disk.h"
#include "game.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "vassert.h"
#include "logs.h"
#include <string.h>
#include <time.h>
#include "loadopengl.h"
#include "../vendor/cglm/cglm.h"
#include "image.h"
#include "chunk.h"
#include "map.h"
#include "pool.h"
#include "oglpool.h"
#include "thread.h"
#include "mesh.h"
#include "profiling.h"

#include "shaders/vert2.c"
#include "shaders/frag2.c"

#define MAX(expr1, expr2) ((expr1) > (expr2) ? (expr1) : (expr2))
const char *BlockTypeString[] = {
#define X(name) #name,
	BLOCKTYPE_NAMES
#undef X
};

typedef struct {
	WorldCoord *coords;
	Block *blocks;
	size_t num_blocks;
	size_t capacity; // might need multi-buffer operations at some point, preferrably not
} BlockBuffer; // might also want a ring buffer that processes these guys
// maybe have logic processing be bound by some kind of power generator that limits the ammount of affected blocks to the buffer size

#ifdef HOT_RELOAD
GameFrameFn game_frame = NULL;
ThreadsFrameFn threads_frame = NULL;
#include <dlfcn.h>
#define NOB_IMPLEMENTATION
#include "../nob.h"
#define SRC_DIR "src/"
bool game_rebuild(Nob_Cmd *cmd)
{
	nob_cmd_append(cmd, "cc");
	nob_cmd_append(cmd, "-shared");
	nob_cmd_append(cmd, "-fPIC");
	nob_cmd_append(cmd, "-Wall");
	nob_cmd_append(cmd, "-Wextra");
	nob_cmd_append(cmd, "-o");
	nob_cmd_append(cmd, "game.so");
	nob_cc_inputs(cmd, SRC_DIR "oglpool.c");
	nob_cc_inputs(cmd, SRC_DIR "disk.c");
	nob_cc_inputs(cmd, SRC_DIR "mesh.c");
	nob_cc_inputs(cmd, SRC_DIR "pool.c");
	nob_cc_inputs(cmd, SRC_DIR "chunk.c");
	nob_cc_inputs(cmd, SRC_DIR "block.c");
	nob_cc_inputs(cmd, SRC_DIR "map.c");
	nob_cmd_append(cmd, SRC_DIR "loadopengl.c");
	nob_cmd_append(cmd, SRC_DIR "game.c");
	return nob_cmd_run(cmd);
}
void get_all_symbols(void *lib)
{
	game_frame = dlsym(lib, "game_frame");
	if (!game_frame) {
		VFATAL("dlsym: %s", dlerror());
		exit(1);
	}
	threads_frame = dlsym(lib, "threads_frame");
	if (!threads_frame) {
		VFATAL("dlsym: %s", dlerror());
		exit(1);
	}
}
#endif //HOT_RELOAD

static size_t get_max_threads()
{
	size_t num_threads = 1;

#ifdef _WIN32
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	num_threads = sysinfo.dwNumberOfProcessors;

#elif defined(_SC_NPROCESSORS_ONLN)
	num_threads = sysconf(_SC_NPROCESSORS_ONLN);
#endif // _WIN32
	return num_threads;
}

// static inline bool is_power_of_two(size_t n)
// {
// 	return n && !(n & (n - 1));
// }
#define NANOS_PER_SEC (1000 * 1000 * 1000)
uint64_t nanos_since_unspecified_epoch(void) // from nob.h
{
#ifdef _WIN32
	LARGE_INTEGER Time;
	QueryPerformanceCounter(&Time);

	static LARGE_INTEGER Frequency = { 0 };
	if (Frequency.QuadPart == 0) {
		QueryPerformanceFrequency(&Frequency);
	}

	uint64_t Secs = Time.QuadPart / Frequency.QuadPart;
	uint64_t Nanos = Time.QuadPart % Frequency.QuadPart * NANOS_PER_SEC / Frequency.QuadPart;
	return NANOS_PER_SEC * Secs + Nanos;
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	return NANOS_PER_SEC * ts.tv_sec + ts.tv_nsec;
#endif // _WIN32
}

static void print_blocklight(BlockLight light)
{
	VINFO("r: %u", light >> 8 * 3 & 0xFF);
	VINFO("g: %u", light >> 8 * 2 & 0xFF);
	VINFO("b: %u", light >> 8 * 1 & 0xFF);
	VINFO("range: %u", light >> 8 * 0 & 0xFF);
}
#ifdef PROFILING

static uint64_t mul_u64_u32_shr(uint64_t cyc, uint32_t mult, uint32_t shift)
{
	__uint128_t x = cyc;
	x *= mult;
	x >>= shift;
	return x;
}

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags)
{
	return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}
static double get_clock_multiplier(void)
{
	struct perf_event_attr pe = {
		.type = PERF_TYPE_HARDWARE,
		.size = sizeof(struct perf_event_attr),
		.config = PERF_COUNT_HW_INSTRUCTIONS,
		.disabled = 1,
		.exclude_kernel = 1,
		.exclude_hv = 1
	};

	int fd = (int)perf_event_open(&pe, 0, -1, -1, 0);
	if (fd == -1) {
		perror("perf_event_open failed");
		return 1;
	}
	void *addr = mmap(NULL, 4 * 1024, PROT_READ, MAP_SHARED, fd, 0);
	if (!addr) {
		perror("mmap failed");
		return 1;
	}
	struct perf_event_mmap_page *pc = (struct perf_event_mmap_page *)addr;
	if (pc->cap_user_time != 1) {
		fprintf(stderr, "Perf system doesn't support user time\n");
		return 1;
	}
	double nanos = (double)mul_u64_u32_shr(1000000000000000ull, pc->time_mult, pc->time_shift);
	double multiplier = nanos / 1000000000000000.0;
	return multiplier;
}

#define SPALL_BUFFER_SIZE (100 * 1024 * 1024)

SpallProfile spall_ctx;
_Thread_local SpallBuffer spall_buffer;
#endif // PROFILING
/* #define FOR_CHUNKS_IN_RENDERDISTANCE(...)                                             \
// 	for (int z = -RENDER_DISTANCE_Z; z <= RENDER_DISTANCE_Z; z++)                 \
// 		for (int y = -RENDER_DISTANCE_Y; y <= RENDER_DISTANCE_Y; y++)         \
// 			for (int x = -RENDER_DISTANCE_X; x <= RENDER_DISTANCE_X; x++) \
// 	__VA_ARGS__
*/

EXTERN GameState game_state = { 0 };

// static const vec3 face_normals[6] = {
// 	{ 0, 0, -1 }, // BACK
// 	{ 0, 0, 1 }, // FRONT
// 	{ -1, 0, 0 }, // LEFT
// 	{ 1, 0, 0 }, // RIGHT
// 	{ 0, -1, 0 }, // BOTTOM
// 	{ 0, 1, 0 } // TOP
// };

#define ARRAY_LEN(array) (sizeof(array) / sizeof(array[0]))

const char *vertexShaderSource = (char *)verts_vert2;
const char *fragmentShaderSource = (char *)frags_frag2;

bool verify_shader(unsigned int shader, bool frag)
{
	int success;
	char infoLog[512];
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(shader, 512, NULL, infoLog);
		VERROR("%s:%s", frag ? "frag" : "vert", infoLog);
		return false;
	}
	return true;
}
bool verify_program(unsigned int program)
{
	int success;
	char infoLog[512];
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(program, 512, NULL, infoLog);
		VERROR(infoLog);
		return false;
	}
	return true;
}

bool create_shader_program(unsigned int *shader_program, const char *vertex_shader_str, const char *fragment_shader_str)
{
	unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &vertex_shader_str, NULL);
	glCompileShader(vertex_shader);
	if (!verify_shader(vertex_shader, false))
		return false;

	unsigned int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &fragment_shader_str, NULL);
	glCompileShader(fragment_shader);
	if (!verify_shader(fragment_shader, true))
		return false;

	*shader_program = glCreateProgram();
	glAttachShader(*shader_program, vertex_shader);
	glAttachShader(*shader_program, fragment_shader);
	glLinkProgram(*shader_program);
	verify_program(*shader_program);
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
	return true;
}

static void print_image_info(Image *image)
{
	VINFO("height: %zu", image->height);
	VINFO("width: %zu", image->width);
	VINFO("n_chan: %zu", image->n_chan);
}

static void vec3_assign(vec3 v, float x, float y, float z)
{
	v[0] = x;
	v[1] = y;
	v[2] = z;
}

// const FaceVertices cube_vertices[] = {
// 	// BACK (normal: 0, 0, -1)
//
// 	{
// 		.vertices = {
// 			{ { 0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f }, FACE_BACK, 0 },
// 			{ { 0.5f, -0.5f, -0.5f }, { 0.0f, 1.0f }, FACE_BACK, 0 },
// 			{ { -0.5f, -0.5f, -0.5f }, { 1.0f, 1.0f }, FACE_BACK, 0 },
// 			{ { -0.5f, -0.5f, -0.5f }, { 1.0f, 1.0f }, FACE_BACK, 0 },
// 			{ { -0.5f, 0.5f, -0.5f }, { 1.0f, 0.0f }, FACE_BACK, 0 },
// 			{ { 0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f }, FACE_BACK, 0 } } },
// 	// FRONT (normal: 0, 0, 1)
// 	{ .vertices = { { { -0.5f, -0.5f, 0.5f }, { 0.0f, 1.0f }, FACE_FRONT, 0 }, { { 0.5f, -0.5f, 0.5f }, { 1.0f, 1.0f }, FACE_FRONT, 0 }, { { 0.5f, 0.5f, 0.5f }, { 1.0f, 0.0f }, FACE_FRONT, 0 }, { { 0.5f, 0.5f, 0.5f }, { 1.0f, 0.0f }, FACE_FRONT, 0 }, { { -0.5f, 0.5f, 0.5f }, { 0.0f, 0.0f }, FACE_FRONT, 0 }, { { -0.5f, -0.5f, 0.5f }, { 0.0f, 1.0f }, FACE_FRONT, 0 } } },
// 	// LEFT (normal: -1, 0, 0)
// 	{ .vertices = { { { -0.5f, 0.5f, 0.5f }, { 1.0f, 0.0f }, FACE_LEFT, 0 }, { { -0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f }, FACE_LEFT, 0 }, { { -0.5f, -0.5f, -0.5f }, { 0.0f, 1.0f }, FACE_LEFT, 0 }, { { -0.5f, -0.5f, -0.5f }, { 0.0f, 1.0f }, FACE_LEFT, 0 }, { { -0.5f, -0.5f, 0.5f }, { 1.0f, 1.0f }, FACE_LEFT, 0 }, { { -0.5f, 0.5f, 0.5f }, { 1.0f, 0.0f }, FACE_LEFT, 0 } } },
// 	// RIGHT (normal: 1, 0, 0)
// 	{ .vertices = { { { 0.5f, -0.5f, -0.5f }, { 1.0f, 1.0f }, FACE_RIGHT, 0 }, { { 0.5f, 0.5f, -0.5f }, { 1.0f, 0.0f }, FACE_RIGHT, 0 }, { { 0.5f, 0.5f, 0.5f }, { 0.0f, 0.0f }, FACE_RIGHT, 0 }, { { 0.5f, 0.5f, 0.5f }, { 0.0f, 0.0f }, FACE_RIGHT, 0 }, { { 0.5f, -0.5f, 0.5f }, { 0.0f, 1.0f }, FACE_RIGHT, 0 }, { { 0.5f, -0.5f, -0.5f }, { 1.0f, 1.0f }, FACE_RIGHT, 0 } } },
// 	// BOTTOM (normal: 0, -1, 0)
// 	{ .vertices = { { { -0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f }, FACE_BOTTOM, 0 }, { { 0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f }, FACE_BOTTOM, 0 }, { { 0.5f, -0.5f, 0.5f }, { 0.0f, 1.0f }, FACE_BOTTOM, 0 }, { { 0.5f, -0.5f, 0.5f }, { 0.0f, 1.0f }, FACE_BOTTOM, 0 }, { { -0.5f, -0.5f, 0.5f }, { 1.0f, 1.0f }, FACE_BOTTOM, 0 }, { { -0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f }, FACE_BOTTOM, 0 } } },
// 	// TOP (normal: 0, 1, 0)
// 	{ .vertices = { { { 0.5f, 0.5f, 0.5f }, { 1.0f, 1.0f }, FACE_TOP, 0 }, { { 0.5f, 0.5f, -0.5f }, { 1.0f, 0.0f }, FACE_TOP, 0 }, { { -0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f }, FACE_TOP, 0 }, { { -0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f }, FACE_TOP, 0 }, { { -0.5f, 0.5f, 0.5f }, { 0.0f, 1.0f }, FACE_TOP, 0 }, { { 0.5f, 0.5f, 0.5f }, { 1.0f, 1.0f }, FACE_TOP, 0 } } }
// };

static void world_init(World *world)
{
	world->pool.cap = 128;
	world->pool.chunk = calloc(world->pool.cap, sizeof(Chunk));
	VPANIC_MSG(world->pool.chunk != NULL, "Buy more RAM");

	oglpool_init(&world->ogl_pool, MAX_VISIBLE_CHUNKS * 2);
	// oglpool_init(&world->ogl_pool, MAX_VISIBLE_CHUNKS * 8);
	// rendermap_init(&world->render_map, 2 * 2048, 2);
	rendermap_init(&world->render_map, 1 << 14, 2);
	// meshqueue_init(&world->mesh, 1024, 1024);

	pool_reserve(&world->pool);

	world->seed = time(NULL);

	threadpool_init(&world->thread, 0, &world->pool, &world->ogl_pool, &world->render_map, &world->mesh, world->seed);

	// size_t pl = 0;
	// ChunkIterator it = {
	// 	.coord.x = -RENDER_DISTANCE_X,
	// 	.x_min = -RENDER_DISTANCE_X,
	// 	.x_max = RENDER_DISTANCE_X,
	// 	.coord.y = -RENDER_DISTANCE_Y,
	// 	.y_min = -RENDER_DISTANCE_Y,
	// 	.y_max = RENDER_DISTANCE_Y,
	// 	.coord.z = -RENDER_DISTANCE_Z,
	// 	.z_min = -RENDER_DISTANCE_Z,
	// 	.z_max = RENDER_DISTANCE_Z
	// };
	// do {
	// 	ChunkCoord coord = { it.coord.x, it.coord.y, it.coord.z };
	// 	if (pl == world->pool.lvl)
	// 		goto done;
	// 	chunk_load(&world->pool.chunk[pl], &coord, world->seed);
	// 	pl++;
	// } while (next_chunk(&it));
	// chunk_load(&world->pool.chunk[0], &(ChunkCoord){ 0, 0, 0 }, world->seed);

	// pool_load(&world->pool, (ChunkCoord){ 0, 0, 0 });
	// done:
	// 	return;
}

// void print_meshqueue(MeshQueue *mesh)
// {
// 	VINFO("Amount :%i", (mesh->writei) - (mesh->readi));
// }

typedef struct {
	vec3 origin;
	vec3 direction;
} Ray;

static void print_chunkcoord(const ChunkCoord *pos)
{
	VINFO("ChunkCoord: x: %i y: %i z: %i", pos->x, pos->y, pos->z);
}

static void print_blockpos(const BlockPos *pos)
{
	VINFO("BlockPos: x: %i y: %i z: %i", pos->x, pos->y, pos->z);
}

// static void meshqueue_chunk_load(ChunkPool *pool, Chunk *chunk, const ChunkCoord *coord, size_t seed)
// {
// 	// TODO: cache
// 	// TODO: pull latest from pool
// 	chunk->coord = *coord;
// 	VASSERT(chunk->data != NULL);
// 	size_t index;
// 	if (pool_get_index(pool, chunk, &index)) {
// 		memcpy(chunk->data, pool->chunk[index].data, sizeof(chunk->data[0]) * CHUNK_TOTAL_BLOCKS);
// 		chunk->block_count = pool->chunk[index].block_count;
// 		// VINFO("copied");
// 		return;
// 	}
// 	chunk_generate_terrain(chunk, seed);
// }

static void shaders_init(ShaderData *shader_data, unsigned int *texture)
{
	VPANIC(create_shader_program(&shader_data->program, vertexShaderSource, fragmentShaderSource));

	glUseProgram(shader_data->program);
	glUniform1i(glGetUniformLocation(shader_data->program, "texture1"), 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, *texture);

	shader_data->view_loc = glGetUniformLocation(shader_data->program, "view");
	shader_data->projection_loc = glGetUniformLocation(shader_data->program, "projection");
	shader_data->model_loc = glGetUniformLocation(shader_data->program, "model");
}

bool workerqueue_is_empty(WorkerQueue *queue)
{
	size_t r = atomic_load_explicit(&queue->readpos, memory_order_relaxed);
	size_t w = atomic_load_explicit(&queue->writepos, memory_order_relaxed);
	// size_t r = queue->readpos;
	// size_t w = queue->writepos;
	return r == w;
}
bool meshs_empty(WorkerSystem *system)
{
	bool ret = true;
	ret &= workerqueue_is_empty(&system->mesh_up);
	ret &= workerqueue_is_empty(&system->mesh_new);
	return ret;
}
bool workerqueue_push(WorkerQueue *queue, WorkerTask task)
{
	size_t current_writepos = atomic_load_explicit(&queue->writepos, memory_order_relaxed);
	size_t next_writepos = (current_writepos + 1) % queue->capacity;
	size_t current_readpos = atomic_load_explicit(&queue->readpos, memory_order_acquire);

	if (next_writepos == current_readpos) {
		return false;
	}

	queue->tasks[current_writepos] = task;
	atomic_thread_fence(memory_order_release);
	atomic_store_explicit(&queue->writepos, next_writepos, memory_order_release);
	return true;
}
void workerqueue_push_override(WorkerQueue *queue, WorkerTask task)
{
	size_t current_writepos = atomic_load_explicit(&queue->writepos, memory_order_relaxed);
	size_t next_writepos = (current_writepos + 1) % queue->capacity;
	size_t current_readpos = atomic_load_explicit(&queue->readpos, memory_order_acquire);

	if (next_writepos == queue->readpos) {
		// size_t new_readpos = (current_writepos - queue->capacity);
		// size_t new_readpos = (current_readpos + 1) % queue->capacity;
		// size_t new_readpos = (current_writepos - queue->capacity / 2);
		size_t new_readpos = (current_readpos + (queue->capacity / 2)) % queue->capacity;
		atomic_store_explicit((atomic_size_t *)&queue->readpos, new_readpos, memory_order_release);
	}

	queue->tasks[current_writepos] = task;
	// VINFO("should hi");

	atomic_thread_fence(memory_order_release);
	atomic_store_explicit((atomic_size_t *)&queue->writepos, next_writepos, memory_order_release);
}

static void meshresourcepool_init(MeshResourcePool *pool, size_t max_users, size_t max_uploads)
{
	pool->max_users = max_users;
	pool->max_uploads = max_uploads;
	pool->taken = calloc(pool->max_users, sizeof(pool->taken[0]));
	VPANIC_MSG(pool->taken != NULL, "Buy more RAM");
	pool->involved = calloc(pool->max_users, sizeof(pool->involved[0]) * MESH_CHUNKS_INVOLVED);
	VPANIC_MSG(pool->involved != NULL, "Buy more RAM");
	pool->blockdata = calloc(pool->max_users, sizeof(pool->blockdata[0]) * CHUNK_TOTAL_BLOCKS * MESH_CHUNKS_INVOLVED);
	VPANIC_MSG(pool->blockdata != NULL, "Buy more RAM");
	pool->upload_status = calloc(pool->max_uploads, sizeof(pool->upload_status[0]));
	VPANIC_MSG(pool->upload_status != NULL, "Buy more RAM");
	pool->upload_data = calloc(pool->max_uploads, sizeof(pool->upload_data[0]));
	VPANIC_MSG(pool->upload_data != NULL, "Buy more RAM");
	for (size_t i = 0; i < pool->max_uploads; i++) {
		pool->upload_data[i].buf.cap = MAX_VERTICES_PER_CHUNK;
		pool->upload_data[i].buf.data = calloc(pool->upload_data[i].buf.cap, sizeof(pool->upload_data[i].buf.data[0]));

		VPANIC_MSG(pool->upload_data[i].buf.data != NULL, "Buy more RAM");
	}
	for (size_t i = 0; i < pool->max_users * MESH_CHUNKS_INVOLVED; i++) {
		pool->involved[i].data = &pool->blockdata[i * CHUNK_TOTAL_BLOCKS];
	}
}

static void light_propagate()
{
}

bool running;

#ifdef _WIN32
DWORD WINAPI threads_main(LPVOID arg)
#else
static void *threads_main(void *arg)
#endif
{
#ifdef PROFILING
	uint8_t *spall_buffer_data = calloc(1, SPALL_BUFFER_SIZE);
	spall_buffer = (SpallBuffer){
		.pid = 0,
		.tid = (uint32_t)(uint64_t)pthread_self(),
		.length = SPALL_BUFFER_SIZE,
		.data = spall_buffer_data,
	};
	memset(spall_buffer.data, 1, spall_buffer.length);
	spall_buffer_init(&spall_ctx, &spall_buffer);
#endif // PROFILING
	Worker *worker = (Worker *)arg;
	WorkerContext *ctx = &worker->context;
	while (running) {
		atomic_bool paused = atomic_load(&ctx->system->paused);
		if (paused) {
			snooze(1);
			continue;
		}

		threads_frame(worker);
	}

#ifdef PROFILING
	spall_buffer_quit(&spall_ctx, &spall_buffer);
#endif // PROFILING

	return 0;
}
bool thread_create(Worker *worker)
{
#ifdef _WIN32
	worker->handle = CreateThread(
		NULL,
		0,
		threads_main,
		worker,
		0,
		NULL);
	if (worker->handle == NULL)
		return false;
	return true;
#else
	int result = pthread_create(
		&worker->handle,
		NULL,
		threads_main,
		worker);
	if (result != 0)
		return false;
	return true;
#endif
}

static void mutex_init(Mutex *mutex)
{
#ifdef _WIN32
	InitializeCriticalSection(mutex);
#else
	pthread_mutex_init(mutex, NULL);
#endif
}
static void mutex_lock(Mutex *mutex)
{
#ifdef _WIN32
	EnterCriticalSection(mutex);
#else
	pthread_mutex_lock(mutex);
#endif
}
static void mutex_unlock(Mutex *mutex)
{
#ifdef _WIN32
	LeaveCriticalSection(mutex);
#else
	pthread_mutex_unlock(mutex);
#endif
}
static void conditional_init(Conditional *cond)
{
#ifdef _WIN32
	InitializeConditionVariable(cond);
#else
	pthread_cond_init(cond, NULL);
#endif
}

static void workerqueue_init(WorkerQueue *queue, size_t capacity)
{
	memset(queue, 0, sizeof(*queue));
	queue->tasks = calloc(capacity, sizeof(WorkerTask));
	VPANIC_MSG(queue->tasks != NULL, "Buy more RAM");
	queue->capacity = capacity;

	// mutex_init(&queue->mutex);
	// conditional_init(&queue->not_empty);
	// conditional_init(&queue->not_full);
}

static void workersystem_init(WorkerSystem *sys, size_t up_cap, size_t new_cap)
{
	workerqueue_init(&sys->mesh_new, new_cap);
	workerqueue_init(&sys->mesh_up, up_cap);
}
void threadpool_init(ThreadPool *thread, size_t num_workers, ChunkPool *pool, OGLPool *ogl_pool, RenderMap *render_map, MeshQueue *mesh_queue, size_t seed)
{
	thread->num = num_workers;
	if (thread->num == 0)
		thread->num = get_max_threads() - 1; // NOTE: not sure if should exclude main and input
	if (thread->num <= 0)
		thread->num = 1; // fallback to keep logic working

	thread->workers = calloc(thread->num, sizeof(thread->workers[0]));
	VPANIC_MSG(thread->workers != NULL, "Buy more RAM");

	// workerqueue_init(&thread->, 1024);
	// meshresourcepool_init(&thread->mesh_resource, 4, 32);
	// meshresourcepool_init(&thread->mesh_resource, 4, 64);
	meshresourcepool_init(&thread->mesh_resource, 4, 256);
	workersystem_init(&thread->system, 256, 256);

	thread->shared_context.pool = pool;
	thread->shared_context.ogl_pool = ogl_pool;
	thread->shared_context.render_map = render_map;
	thread->shared_context.mesh_queue = mesh_queue;
	thread->shared_context.seed = seed;
	thread->shared_context.system = &thread->system;
	thread->shared_context.mesh_resource = &thread->mesh_resource;

	for (size_t i = 0; i < thread->num; i++) {
		Worker *w = &thread->workers[i];
		w->id = i;
		w->context = thread->shared_context;
		VPANIC_MSG(thread_create(w), "Thread Creation failed");
	}
}

typedef struct {
	size_t ogl_index;
	// bool up_to_date;
} RenderRingItem;
typedef struct {
	RenderRingItem *items;
	ChunkCoord *outdated;
	size_t outdated_count;
	size_t outdated_cap;
	size_t cap;
	size_t shift_x;
	size_t shift_z;
	size_t shift_y;
	ChunkCoord base;
} RenderRing;

static void renderring_init(RenderRing *ring, size_t cap, size_t outdated_cap)
{
	ring->cap = cap;
	ring->items = calloc(ring->cap, sizeof(ring->items[0]));
	VPANIC_MSG(ring->items != NULL, "Buy more RAM");
	ring->outdated_cap = outdated_cap;
	ring->outdated = calloc(ring->outdated_cap, sizeof(ring->outdated[0]));
	VPANIC_MSG(ring->outdated != NULL, "Buy more RAM");
}

static void renderring_update(RenderRing *ring, ChunkCoord player)
{
	ChunkCoord new_base = { 0 };
	new_base.x = player.x - RENDER_DISTANCE_X;
	new_base.z = player.z - RENDER_DISTANCE_Z;
	new_base.y = player.y - RENDER_DISTANCE_Y;
	int32_t deltax = new_base.x - ring->base.x;
	int32_t deltaz = new_base.z - ring->base.z;
	int32_t deltay = new_base.y - ring->base.y;
	ring->shift_x = (ring->shift_x + deltax) / RENDER_TOTAL_X;
	ring->shift_z = (ring->shift_z + deltaz) / RENDER_TOTAL_Z;
	ring->shift_y = (ring->shift_y + deltay) / RENDER_TOTAL_Y;
}

void threadpool_cleanup(ThreadPool *thread)
{
	for (size_t i = 0; i < thread->num; i++) {
#ifdef _WIN32
		WaitForSingleObject(thread->workers[i].handle, INFINITE);
		CloseHandle(thread->workers[i].handle);
#else
		pthread_join(thread->workers[i].handle, NULL);
#endif
	}

	// MeshResourcePool *meshres = &thread->mesh_resource;
	// for (size_t i = 0; i < meshres->max_uploads; i++)
	// 	free(meshres->upload_data[i].buf.data);
	// free(meshres->upload_data);
	// free(meshres->upload_status);
	// free(meshres->blockdata);
	// free(meshres->involved);
	// free(meshres->taken);
	//
	// free(thread->workers);
	// free(thread->system.mesh_up.tasks);
	// free(thread->system.mesh_new.tasks);
}

int main(int argc, char *argv[])
{
#ifdef HOT_RELOAD
	Nob_Cmd cmd = { 0 };
	VPANIC(game_rebuild(&cmd));
	void *game_lib = dlopen("./game.so", RTLD_NOW);
	if (!game_lib) {
		VFATAL("dlopen: %s", dlerror());
		exit(1);
	}
	get_all_symbols(game_lib);

	int h_was_down = 0;
#endif
#ifdef PROFILING
	spall_init_file("spall_sample.spall", get_clock_multiplier(), &spall_ctx);

	static uint8_t spall_buffer_data[SPALL_BUFFER_SIZE];
	spall_buffer = (SpallBuffer){
		.pid = 0,
		.tid = (uint32_t)(uint64_t)pthread_self(),
		.length = SPALL_BUFFER_SIZE,
		.data = spall_buffer_data,
	};
	memset(spall_buffer.data, 1, spall_buffer.length);
	spall_buffer_init(&spall_ctx, &spall_buffer);

#endif // PROFILING
	running = true;
	// VINFO("%zu", sizeof(BLOCKTYPE));
	WindowData window = { 0 };
	platform_init(&window, 1600, 900);
	// input_system_init(&window); // TODO: consider if wanted
	// glfw_init(&window, 1920, 1080);
	unsigned int texture;
	ogl_init(&texture);

	Image image_data = { 0 };
	assets_init(&image_data);

	ShaderData shader_data = { 0 };
	shaders_init(&shader_data, &texture);

	game_state.world.player.camera = (Camera){
		.fov = 90.0f,
		.yaw = -00.0f,
		.pitch = 0.0f,
		.pos = { 0.0f, 10.0f, 000000000.0f },
		.up = { 0.0f, 1.0f, 0.0f },
		.front = { 0.0f, 0.0f, -1.0f },
	};

	World *world = &game_state.world;
	world_init(world);

	{
		static char *uid = "hi";
		world->uid = uid;
	}
	//disk_init(world->uid);

	// BlockLight light = color_and_range_to_blocklight((Color){ 255, 255, 0, 1 }, 15);
	// print_blocklight(light);
	VINFO("%i", get_max_threads());

	Player *player = &world->player;
	player->movement_speed = 5.0f;

	while (!window.should_close) {
#ifdef HOT_RELOAD
		// TODO: add the other threads aswell
		int h_down = glfwGetKey(window.glfw, GLFW_KEY_H) == GLFW_PRESS;
		if (h_down && !h_was_down) {
			threadpool_pause(&world->thread);
			if (!game_rebuild(&cmd)) {
				h_was_down = h_down;
				threadpool_resume(&world->thread);
				continue;
			}
			dlclose(game_lib);

			game_lib = dlopen("./game.so", RTLD_NOW);
			if (!game_lib) {
				VFATAL("dlopen: %s", dlerror());
				exit(1);
			}
			get_all_symbols(game_lib);
			threadpool_resume(&world->thread);
		}
		h_was_down = h_down;
#endif // HOT_RELOAD
		game_frame(&window, &shader_data);
	}

	running = false;
	threadpool_cleanup(&world->thread);

#ifdef PROFILING
	spall_buffer_quit(&spall_ctx, &spall_buffer);
	spall_quit(&spall_ctx);
#endif //PROFILING

	platform_destroy(&window);
}
