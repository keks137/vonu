#include "block.h"
#include "disk.h"
#include <GL/glcorearb.h>
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
#define STB_IMAGE_IMPLEMENTATION
#include "../vendor/stb_image.h"
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
	unsigned int program;
	unsigned int model_loc;
	unsigned int projection_loc;
	unsigned int view_loc;
} ShaderData;

typedef struct {
	WorldCoord *coords;
	Block *blocks;
	size_t num_blocks;
	size_t capacity; // might need multi-buffer operations at some point, preferrably not
} BlockBuffer; // might also want a ring buffer that processes these guys
// maybe have logic processing be bound by some kind of power generator that limits the ammount of affected blocks to the buffer size

typedef struct {
	vec3 pos;
	vec3 front;
	vec3 up;
	float yaw;
	float pitch;
	float fov;
} Camera;

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

typedef struct {
	Camera camera;
	WorldCoord pos;
	ChunkCoord chunk_pos;
	bool placing;
	bool breaking;
} Player;

typedef struct {
	ChunkPool pool;
	OGLPool ogl_pool;
	ThreadPool thread;
	RenderMap render_map;
	MeshQueue mesh;
	size_t chunk_count;
	Player player;
	size_t seed;
	const char *name;
	const char *uid;
} World;

typedef struct {
	World world;
	float delta_time;
	float last_frame;
	bool paused;
} GameState;

GameState game_state = { 0 };

// static const vec3 face_normals[6] = {
// 	{ 0, 0, -1 }, // BACK
// 	{ 0, 0, 1 }, // FRONT
// 	{ -1, 0, 0 }, // LEFT
// 	{ 1, 0, 0 }, // RIGHT
// 	{ 0, -1, 0 }, // BOTTOM
// 	{ 0, 1, 0 } // TOP
// };

float movement_speed = 5.0f;

#define ARRAY_LEN(array) (sizeof(array) / sizeof(array[0]))

const char *vertexShaderSource = (char *)verts_vert2;
const char *fragmentShaderSource = (char *)frags_frag2;

float last_x = 400;
float last_y = 300;
typedef struct {
	bool reset_mouse;
} MouseState;
MouseState mouse_state;

static void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
	if (game_state.paused)
		return;
	(void)window;
	if (mouse_state.reset_mouse) {
		last_x = xpos;
		last_y = ypos;
		mouse_state.reset_mouse = false;
	}

	float xoffset = xpos - last_x;
	float yoffset = last_y - ypos;
	last_x = xpos;
	last_y = ypos;

	const float sensitivity = 0.1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	Camera *camera = &game_state.world.player.camera;
	camera->yaw += xoffset;
	camera->pitch += yoffset;

	if (camera->pitch > 89.0f)
		camera->pitch = 89.0f;
	if (camera->pitch < -89.0f)
		camera->pitch = -89.0f;
	vec3 direction = { 0 };

	direction[0] = cos(glm_rad(camera->yaw)) * cos(glm_rad(camera->pitch));
	direction[1] = sin(glm_rad(camera->pitch));
	direction[2] = sin(glm_rad(camera->yaw)) * cos(glm_rad(camera->pitch));
	glm_normalize(direction);
	glm_vec3_copy(direction, camera->front);
}
static void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
	(void)window;
	glViewport(0, 0, width, height);
}

static void set_paused(GLFWwindow *window, bool val)
{
	game_state.paused = val;
	if (game_state.paused) {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	} else {
		mouse_state.reset_mouse = true;
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
}

static void window_focus_callback(GLFWwindow *window, int focused)
{
	if (focused == GLFW_TRUE) {
	} else {
		set_paused(window, true);
	}
}

static void process_input(GLFWwindow *window, Player *player)
{
	BEGIN_FUNC();
	static bool escapeKeyPressedLastFrame = false;

	bool escapeKeyPressedThisFrame = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;

	if (escapeKeyPressedThisFrame && !escapeKeyPressedLastFrame) {
		set_paused(window, !game_state.paused);
	}

	escapeKeyPressedLastFrame = escapeKeyPressedThisFrame;

	if (game_state.paused) {
		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
			set_paused(window, false);
		}
		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
			set_paused(window, false);
		}

		END_FUNC();
		return;
	}

	Camera *camera = &player->camera;
	const float camera_speed = movement_speed * game_state.delta_time;
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
		camera->pos[1] += camera_speed;
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
		camera->pos[1] -= camera_speed;
	}
	static bool sprinting;
	float forward_speed = camera_speed;
	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
		sprinting = true;
	}
	if (sprinting)
		forward_speed *= 2;

	vec3 horizontal_front;
	glm_vec3_copy(camera->front, horizontal_front);
	horizontal_front[1] = 0.0f;
	glm_normalize(horizontal_front);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		glm_vec3_muladds(horizontal_front, forward_speed, camera->pos);
	} else {
		sprinting = false;
	}

	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		glm_vec3_mulsubs(horizontal_front, camera_speed, camera->pos);
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		vec3 right;
		glm_cross(horizontal_front, camera->up, right);
		glm_normalize(right);
		glm_vec3_mulsubs(right, camera_speed, camera->pos);
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
		vec3 right;
		glm_cross(horizontal_front, camera->up, right);
		glm_normalize(right);
		glm_vec3_muladds(right, camera_speed, camera->pos);
	}
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
		player->breaking = true;
	}
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
		player->placing = true;
	}
	END_FUNC();
}

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

bool load_image(Image *img, const char *path)
{
	int width = 0;
	int height = 0;
	int nrChannels = 0;
	// stbi_set_flip_vertically_on_load(true);
	FILE *file = fopen(path, "rb");
	if (file == NULL) {
		VERROR("Could not open file: %s", path);
		return false;
	}
	unsigned char *data = stbi_load_from_file(file, &width, &height, &nrChannels, 0);
	fclose(file);
	img->width = width;
	img->height = height;
	img->n_chan = nrChannels;
	img->data = data;
	if (data == NULL)
		return false;

	return true;
}

static void print_image_info(Image *image)
{
	VINFO("height: %zu", image->height);
	VINFO("width: %zu", image->width);
	VINFO("n_chan: %zu", image->n_chan);
}

#ifdef NDEBUG
#define GL_CHECK(stmt) \
	do {           \
		stmt;  \
	} while (0)
#else
#define GL_CHECK(stmt)                                                                                            \
	do {                                                                                                      \
		stmt;                                                                                             \
		GLenum err = glGetError();                                                                        \
		if (err != GL_NO_ERROR) {                                                                         \
			VERROR("OpenGL error 0x%04X at %s:%d: %s: %s", err, __FILE__, __LINE__, __func__, #stmt); \
		}                                                                                                 \
	} while (0)
#endif // NDEBUG

static void chunk_render(OGLPool *pool, Chunk *chunk, ShaderData shader_data)
{
#if 0
	if (!chunk->up_to_date || !chunk->has_oglpool_reference)
		return;
#else
	if (chunk->oglpool_index == 0)
		return;
#endif
	if (chunk->face_count == 0) {
		// VERROR("This guy again:");
		return;
	}

	BEGIN_FUNC();
	VASSERT_MSG(chunk->block_count != 0, "How did it get here?");
	VASSERT_MSG(pool->items[chunk->oglpool_index].references > 0, "How did it get here?");
	VASSERT_MSG(chunk->face_count > 0, "How did it get here?");

	chunk->unchanged_render_count++;

	GL_CHECK(glUseProgram(shader_data.program));
	GL_CHECK(glBindVertexArray(pool->items[chunk->oglpool_index].VAO));

	mat4 model = { 0 };
	glm_mat4_identity(model);
	vec3 pos = { 0 };
	pos[0] = chunk->coord.x * CHUNK_TOTAL_X;
	pos[1] = chunk->coord.y * CHUNK_TOTAL_Y;
	pos[2] = chunk->coord.z * CHUNK_TOTAL_Z;
	GL_CHECK(glm_translate(model, pos));

	GL_CHECK(glUniformMatrix4fv(shader_data.model_loc, 1, GL_FALSE, (const float *)model));

	size_t index_count = chunk->face_count * INDICES_PER_QUAD;

	GL_CHECK(glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_SHORT, 0));
	// GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, chunk->vertex_count));
	END_FUNC();
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

static void print_block(Block *block)
{
	VINFO("Block: %p", block);
	VINFO("BlockType: %s", BlockTypeString[block->type]);
	VINFO("Obstructing: %s", block->obstructing ? "true" : "false");
}

typedef struct {
	ChunkCoord coord;
	int64_t x_min, x_max, y_min, y_max, z_min, z_max;
} ChunkIterator;

bool next_chunk(ChunkIterator *it)
{
	if (++it->coord.x > it->x_max) {
		it->coord.x = it->x_min;
		if (++it->coord.y > it->y_max) {
			it->coord.y = it->y_min;
			if (++it->coord.z > it->z_max) {
				return false;
			}
		}
	}
	return true;
}

static void world_init(World *world)
{
	world->pool.cap = 128;
	world->pool.chunk = calloc(world->pool.cap, sizeof(Chunk));
	VASSERT_RELEASE_MSG(world->pool.chunk != NULL, "Buy more RAM");

	oglpool_init(&world->ogl_pool, MAX_VISIBLE_CHUNKS * 2);
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

static void rendermap_keep_only_in_range(RenderMap *map, OGLPool *pool)
{
	ChunkIterator it = {
		.coord.x = -RENDER_DISTANCE_X,
		.x_min = -RENDER_DISTANCE_X,
		.x_max = RENDER_DISTANCE_X,
		.coord.y = -RENDER_DISTANCE_Y,
		.y_min = -RENDER_DISTANCE_Y,
		.y_max = RENDER_DISTANCE_Y,
		.coord.z = -RENDER_DISTANCE_Z,
		.z_min = -RENDER_DISTANCE_Z,
		.z_max = RENDER_DISTANCE_Z
	};

	do {
		Chunk chunk = { 0 };
		if (rendermap_get_chunk(map, &chunk, &(ChunkCoord){ it.coord.x, it.coord.y, it.coord.z })) {
			rendermap_add_next_buffer(map, pool, &chunk);
		}
	} while (next_chunk(&it));
	rendermap_advance_buffer(map, pool);
}

static void rendermap_clean_maybe(RenderMap *map, OGLPool *pool)
{
	BEGIN_FUNC();
	// if (map->count > 1.5f * MAX_VISIBLE_CHUNKS) {
	// 	rendermap_keep_only_in_range(map, pool);
	// }
	if (map->count > 0.75f * map->table_size) {
		rendermap_keep_only_in_range(map, pool);
	}
	END_FUNC();
}

static void player_update(Player *player)
{
	player->pos.x = floorf(player->camera.pos[0]);
	player->pos.y = floorf(player->camera.pos[1]) - 1;
	player->pos.z = floorf(player->camera.pos[2]);
	ChunkCoord new_chunk = world_coord_to_chunk(&player->pos);
	player->chunk_pos = new_chunk;
}

typedef struct {
	GLFWwindow *glfw;
	int width;
	int height;
} WindowData;

// void print_meshqueue(MeshQueue *mesh)
// {
// 	VINFO("Amount :%i", (mesh->writei) - (mesh->readi));
// }

static void world_render(World *world, ShaderData shader_data)
{
	BEGIN_FUNC();
	// time_t current_time = time(NULL);
	// if ((world->mesh.writei - world->mesh.readi) != 0)
	// 	print_meshqueue(&world->mesh);
	// print_pool(&world->pool);
	meshqueue_process(1024, &world->thread.mesh_resource, &world->render_map, &world->ogl_pool);

	for (int y = -RENDER_DISTANCE_Y; y <= RENDER_DISTANCE_Y; y++) {
		for (int z = -RENDER_DISTANCE_Z; z <= RENDER_DISTANCE_Z; z++) {
			for (int x = -RENDER_DISTANCE_X; x <= RENDER_DISTANCE_X; x++) {
				// BEGIN_SECT("render one chunk");
				ChunkCoord coord = {
					world->player.chunk_pos.x + x,
					world->player.chunk_pos.y + y,
					world->player.chunk_pos.z + z
				};
				Chunk chunk = { 0 };
				if (rendermap_get_chunk(&world->render_map, &chunk, &coord)) {
					// VINFO("hi");
					chunk_render(&world->ogl_pool, &chunk, shader_data);
				}
				// END_SECT("render one chunk");
			}
		}
	}

	// ChunkCoord player_chunk = world_coord_to_chunk(world->player.pos);
	// VINFO("Player coords: %i %i %i", player_chunk.x, player_chunk.y, player_chunk.z);
	END_FUNC();
}

typedef struct {
	vec3 origin;
	vec3 direction;
} Ray;

Block blocktype_to_block(BLOCKTYPE type)
{
	Block block = { 0 };
	switch (type) {
	case BlocktypeGrass:
		block.obstructing = true;
		block.type = BlocktypeGrass;
		break;
	case BlocktypeStone:
		block.obstructing = true;
		block.type = BlocktypeStone;
		block_make_light(&block, (Color){ rand() % 255, rand() % 255, rand() % 255, 1 });
		break;
	default:
		break;
	}
	return block;
}

static void player_print_block(Player *player, OGLPool *ogl, RenderMap *map, ChunkPool *pool, size_t seed)
{
	Block block = pool_read_block(pool, ogl, map, &player->pos, seed);
	VINFO("Pos: x: %i, y: %i, z: %i", player->pos.x, player->pos.y, player->pos.z)
	print_block(&block);
}
static void player_break_block(ChunkPool *pool, OGLPool *ogl, RenderMap *map, const WorldCoord *pos, size_t seed)
{
	// TODO: check if player is actually allowed to
	Block block = { 0 };
	pool_replace_block(pool, ogl, map, pos, block, seed);
}
static void player_place_block(Player *player, BLOCKTYPE blocktype, ChunkPool *pool, OGLPool *ogl, RenderMap *map, const WorldCoord *pos, size_t seed)
{
	(void)player;
	// TODO: check if player is actually allowed to
	Block block = blocktype_to_block(blocktype);

	pool_replace_block(pool, ogl, map, pos, block, seed);
	// pool_replace_block(pool, ogl, map, &(WorldCoord){ 0, -10, 0 }, block, seed);
	// player_print_block(player, ogl, map, pool, seed);
}
static void workers_add_update(ThreadPool *thread, const ChunkCoord coord)
{
	// VINFO("%i", thread->system.mesh_up.writepos - thread->system.mesh_up.readpos);
	workerqueue_push_override(&thread->system.mesh_up, (WorkerTask){ coord, NULL, 0 });
}
static void workers_add_new(ThreadPool *thread, const ChunkCoord coord)
{
	workerqueue_push_override(&thread->system.mesh_new, (WorkerTask){ coord, NULL, 0 });
}
static void mesh_request(RenderMap *map, ThreadPool *thread, const ChunkCoord *coord)
{
	BEGIN_FUNC();
	Chunk chunk = { 0 };
	// TODO: make this scream into the void again
	if (rendermap_get_chunk(map, &chunk, coord)) {
		// print_chunk(&chunk);
		if (chunk.up_to_date) {
			END_FUNC();
			return;
		}
		workers_add_update(thread, *coord);
		END_FUNC();

		return;
	}
	workers_add_new(thread, *coord);
	END_FUNC();
}

static void print_blockpos(const BlockPos *pos)
{
	VINFO("BlockPos: x: %i y: %i z: %i", pos->x, pos->y, pos->z);
}
static void print_chunkcoord(const ChunkCoord *pos)
{
	VINFO("ChunkCoord: x: %i y: %i z: %i", pos->x, pos->y, pos->z);
}

static void meshqueue_chunk_load(ChunkPool *pool, Chunk *chunk, const ChunkCoord *coord, size_t seed)
{
	// TODO: cache
	// TODO: pull latest from pool
	chunk->last_used = time(NULL);
	chunk->coord = *coord;
	VASSERT(chunk->data != NULL);
	size_t index;
	if (pool_get_index(pool, chunk, &index)) {
		memcpy(chunk->data, pool->chunk[index].data, sizeof(chunk->data[0]) * CHUNK_TOTAL_BLOCKS);
		chunk->block_count = pool->chunk[index].block_count;
		// VINFO("copied");
		return;
	}
	chunk_generate_terrain(chunk, seed);
}

static void mesh_upload_chunk(OGLPool *ogl, ChunkVertsScratch *scratch, Chunk *chunk)
{
	chunk->face_count = scratch->lvl / VERTICES_PER_QUAD;

	VASSERT_WARN(chunk->face_count > 0);
	if (chunk->face_count == 0) {
		return;
	}

	if (chunk->oglpool_index == 0) {
		if (!oglpool_claim_chunk(ogl, chunk))
			return;
	}
	VASSERT(chunk->oglpool_index != 0);
	OGLItem *item = &ogl->items[chunk->oglpool_index];
	VASSERT_MSG(item->references > 0, "How did it get here?");

	glBindVertexArray(item->VAO);

	glBindBuffer(GL_ARRAY_BUFFER, item->VBO);
	glBufferData(GL_ARRAY_BUFFER,
		     scratch->lvl * sizeof(scratch->data[0]),
		     scratch->data,
		     chunk->updates_this_cycle > 2 ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
	chunk->has_vbo_data = true;
}
static inline void meshqueue_process_element(MeshUploadData *upd, RenderMap *map, OGLPool *ogl)
{
	Chunk chunk = { 0 };
	chunk.up_to_date = true;
	chunk.block_count = upd->block_count;
	// print_chunk(&chunk);

	chunk.coord = upd->coord;
	if (upd->buf.lvl > 0) {
		mesh_upload_chunk(ogl, &upd->buf, &chunk);
		rendermap_add(map, ogl, &chunk);
	} else // TODO: empty map
		rendermap_add(map, ogl, &chunk);

	if (chunk.oglpool_index != 0)
		oglpool_release_chunk(ogl, &chunk);
}
void meshqueue_process(size_t max, MeshResourcePool *meshp, RenderMap *map, OGLPool *ogl)
{
	BEGIN_FUNC();
	size_t num_proc = 0;
	for (size_t i = 0; i < meshp->max_uploads && num_proc < max; i++) {
		if (meshp->upload_status[i] == UPLOAD_STATUS_READY) {
			MeshUploadData *upd = &meshp->upload_data[i];
			// VINFO("i: %zu bc: %zu x: %i y: %i z: %i", i, upd->block_count, upd->coord.x, upd->coord.y, upd->coord.z);
			meshqueue_process_element(upd, map, ogl);
			// VINFO("uploaded");
			meshp->upload_status[i] = UPLOAD_STATUS_FREE;
			num_proc++;
		}
		// VINFO("num_proc: %zu",num_proc);
	}
	END_FUNC();
}

static void world_update(World *world)
{
	BEGIN_FUNC();
	rendermap_clean_maybe(&world->render_map, &world->ogl_pool);
	// VINFO("c: %i", world->render_map.count);
	// VINFO("d: %i", world->render_map.deleted_count);

	BEGIN_SECT("chunk pool");
	for (size_t i = 0; i < world->pool.lvl; i++) {
		Chunk *chunk = &world->pool.chunk[i];
		// if (world->pool.chunk[i].up_to_date) {
		if (chunk->updates_this_cycle > 0) {
			// VINFO("bye");
			// TODO: also outdate appropriate neighbours
			rendermap_outdate(&world->render_map, &chunk->coord);
			chunk->cycles_since_update++;
			chunk->updates_this_cycle = 0;
			chunk->cycles_in_pool = 0;
		}

		if (chunk->cycles_in_pool > 400) {
			if (chunk->modified)
				disk_save(chunk, world->uid);
			pool_empty(&world->pool, &world->ogl_pool, i);
		}
		chunk->cycles_in_pool++;
	}
	END_SECT("chunk pool");

	BEGIN_SECT("render req");
	for (int y = RENDER_DISTANCE_Y; y > -RENDER_DISTANCE_Y; y--) {
		for (int z = -RENDER_DISTANCE_Z; z <= RENDER_DISTANCE_Z; z++) {
			for (int x = -RENDER_DISTANCE_X; x <= RENDER_DISTANCE_X; x++) {
				ChunkCoord coord = {
					world->player.chunk_pos.x + x,
					world->player.chunk_pos.y + y,
					world->player.chunk_pos.z + z
				};

				mesh_request(&world->render_map, &world->thread, &coord);
			}
		}
	}
	END_SECT("render req");
	END_FUNC();
}
static void render(GameState *game_state, WindowData window, ShaderData shader_data)
{
	// BEGIN_FUNC();
	BEGIN_SECT("Buffer Swap");
	glfwSwapBuffers(window.glfw);
	END_SECT("Buffer Swap");
	BEGIN_SECT("Other Stuff");
	mat4 view;
	vec3 camera_pos_plus_front;
	Camera *camera = &game_state->world.player.camera;

	glm_vec3_add(camera->pos, camera->front, camera_pos_plus_front);
	glm_lookat(camera->pos, camera_pos_plus_front, camera->up, view);

	mat4 projection = { 0 };
	float aspect = (float)window.width / (float)window.height;
	glm_perspective(glm_rad(camera->fov), aspect, 0.1f, 500.0f, projection);

	glClearColor(0.39f, 0.58f, 0.92f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(shader_data.program);

	glUniformMatrix4fv(shader_data.view_loc, 1, GL_FALSE, (const float *)view);
	glUniformMatrix4fv(shader_data.projection_loc, 1, GL_FALSE, (const float *)projection);

	END_SECT("Other Stuff");
	world_render(&game_state->world, shader_data);

	BEGIN_SECT("Poll Events");
	glfwPollEvents();
	END_SECT("Poll Events");
	GL_CHECK((void)0);
	// END_FUNC();
}

static void glfw_init(WindowData *window, int width, int height)
{
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
	glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);

	if (!glfwInit()) {
		exit(1);
	}
	window->width = width;
	window->height = height;
	window->glfw = glfwCreateWindow(window->width, window->height, "Vonu", NULL, NULL);

	if (!window->glfw) {
		glfwTerminate();
		exit(1);
	}

	glfwMakeContextCurrent(window->glfw);
	glfwSetCursorPosCallback(window->glfw, mouse_callback);
	glfwSetFramebufferSizeCallback(window->glfw, framebuffer_size_callback);
	glfwSetInputMode(window->glfw, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetWindowFocusCallback(window->glfw, window_focus_callback);
	glfwSwapInterval(0); // no vsync
}

static void ogl_init(unsigned int *texture)
{
	if (!load_gl_functions())
		exit(1);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_DEPTH_CLAMP);

	glGenTextures(1, texture);
	glBindTexture(GL_TEXTURE_2D, *texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	float borderColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	// glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); //wireframe
}

static void assets_init(Image *image_data)
{
	if (!load_image(image_data, "assets/atlas.png"))
		exit(1);
	GLenum format;
	if (image_data->n_chan == 3) {
		format = GL_RGB;
	} else if (image_data->n_chan == 4) {
		format = GL_RGBA;
	} else {
		format = GL_RGB;
	}

	glTexImage2D(GL_TEXTURE_2D, 0, format, image_data->width, image_data->height, 0, format, GL_UNSIGNED_BYTE, image_data->data);
	glGenerateMipmap(GL_TEXTURE_2D);
	stbi_image_free(image_data->data);
}

static void shaders_init(ShaderData *shader_data, unsigned int *texture)
{
	if (!create_shader_program(&shader_data->program, vertexShaderSource, fragmentShaderSource))
		exit(1);
	glUseProgram(shader_data->program);
	glUniform1i(glGetUniformLocation(shader_data->program, "texture1"), 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, *texture);

	shader_data->view_loc = glGetUniformLocation(shader_data->program, "view");
	shader_data->projection_loc = glGetUniformLocation(shader_data->program, "projection");
	shader_data->model_loc = glGetUniformLocation(shader_data->program, "model");
}

static void snooze(uint64_t ms)
{
#ifdef _WIN32
	Sleep(ms);
#else
	struct timespec ts = {
		.tv_sec = ms / 1000,
		.tv_nsec = (ms % 1000) * 1000000
	};
	nanosleep(&ts, NULL);
#endif
}
static void threadpool_pause(ThreadPool *pool)
{
	pool->system.paused = true;
}

static void threadpool_resume(ThreadPool *pool)
{
	pool->system.paused = false;
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
bool workerqueue_pop(WorkerQueue *queue, WorkerTask *task)
{
	size_t current_readpos = atomic_load_explicit(&queue->readpos, memory_order_acquire);

	size_t current_writepos = atomic_load_explicit(&queue->writepos, memory_order_acquire);
	// VINFO("r: %zu w:%zu", current_readpos, current_writepos);
	if (current_readpos == current_writepos) {
		return false;
	}

	size_t next_readpos = (current_readpos + 1) % queue->capacity;

	if (atomic_compare_exchange_weak_explicit((atomic_size_t *)&queue->readpos, &current_readpos, next_readpos, memory_order_release, memory_order_relaxed)) {
		*task = queue->tasks[current_readpos];
		return true;
	}
	return false;
}

static void meshresourcepool_init(MeshResourcePool *pool, size_t max_users, size_t max_uploads)
{
	pool->max_users = max_users;
	pool->max_uploads = max_uploads;
	pool->taken = calloc(pool->max_users, sizeof(pool->taken[0]));
	VASSERT_RELEASE_MSG(pool->taken != NULL, "Buy more RAM");
	pool->involved = calloc(pool->max_users, sizeof(pool->involved[0]) * MESH_CHUNKS_INVOLVED);
	VASSERT_RELEASE_MSG(pool->involved != NULL, "Buy more RAM");
	pool->blockdata = calloc(pool->max_users, sizeof(pool->blockdata[0]) * CHUNK_TOTAL_BLOCKS * MESH_CHUNKS_INVOLVED);
	VASSERT_RELEASE_MSG(pool->blockdata != NULL, "Buy more RAM");
	pool->upload_status = calloc(pool->max_uploads, sizeof(pool->upload_status[0]));
	VASSERT_RELEASE_MSG(pool->upload_status != NULL, "Buy more RAM");
	pool->upload_data = calloc(pool->max_uploads, sizeof(pool->upload_data[0]));
	VASSERT_RELEASE_MSG(pool->upload_data != NULL, "Buy more RAM");
	for (size_t i = 0; i < pool->max_uploads; i++) {
		pool->upload_data[i].buf.cap = MAX_VERTICES_PER_CHUNK;
		pool->upload_data[i].buf.data = calloc(pool->upload_data[i].buf.cap, sizeof(pool->upload_data[i].buf.data[0]));

		VASSERT_RELEASE_MSG(pool->upload_data[i].buf.data != NULL, "Buy more RAM");
	}
	for (size_t i = 0; i < pool->max_users * MESH_CHUNKS_INVOLVED; i++) {
		pool->involved[i].data = &pool->blockdata[i * CHUNK_TOTAL_BLOCKS];
	}
}

bool meshuser_try_acquire(MeshResourcePool *pool, MeshUserIndex *index)
{
	for (MeshUserIndex i = 0; i < pool->max_users; i++) {
		bool expected = false;
		if (atomic_compare_exchange_weak(&pool->taken[i], &expected, true)) {
			*index = i;
			return true;
		}
	}
	return false;
}

bool meshupload_try_acquire(MeshResourcePool *pool, MeshUploadIndex *index)
{
	for (MeshUploadIndex i = 0; i < pool->max_uploads; i++) {
		size_t expected = UPLOAD_STATUS_FREE;
		if (atomic_compare_exchange_weak(&pool->upload_status[i], &expected, UPLOAD_STATUS_PROCESSING)) {
			*index = i;
			return true;
		}
	}
	return false;
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
	MeshResourcePool *meshres = ctx->mesh_resource;
	WorkerTask task = { 0 };
	MeshResourceHandle res = { 0 };
	while (running) {
		if (ctx->system->paused) {
			snooze(1);
			continue;
		}
		if (!meshs_empty(ctx->system)) {
			// VINFO("hi");
			if (meshuser_try_acquire(meshres, &res.user)) {
				// VINFO("%i", test++);
				while (meshupload_try_acquire(meshres, &res.up)) {
					if (workerqueue_pop(&ctx->system->mesh_up, &task)) {
						// VINFO("hi");
						meshworker_process_mesh(worker, meshres, &res, &task);
					} else if (workerqueue_pop(&ctx->system->mesh_new, &task)) {
						meshworker_process_mesh(worker, meshres, &res, &task);
					} else {
						// VINFO("bye");
						break;
					}
					// snooze(1);
				}
				meshres->taken[res.user] = false;
			}
			snooze(1);
		} else
			snooze(1);
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
	VASSERT_RELEASE_MSG(queue->tasks != NULL, "Buy more RAM");
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
		thread->num = get_max_threads() - 1;
	thread->workers = calloc(thread->num, sizeof(thread->workers[0]));
	VASSERT_RELEASE_MSG(thread->workers != NULL, "Buy more RAM");

	// workerqueue_init(&thread->, 1024);
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
		VASSERT_RELEASE_MSG(thread_create(w), "Thread Creation failed");
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
	VASSERT_RELEASE_MSG(ring->items != NULL, "Buy more RAM");
	ring->outdated_cap = outdated_cap;
	ring->outdated = calloc(ring->outdated_cap, sizeof(ring->outdated[0]));
	VASSERT_RELEASE_MSG(ring->outdated != NULL, "Buy more RAM");
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

int main()
{
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
	VINFO("%zu", sizeof(BLOCKTYPE));
	WindowData window = { 0 };
	glfw_init(&window, 1600, 900);
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
		.pos = { 0.0f, 30.0f, 000000000.0f },
		.up = { 0.0f, 1.0f, 0.0f },
		.front = { 0.0f, 0.0f, -1.0f },
	};

	World *world = &game_state.world;
	world_init(world);

	{
		static char *uid = "hi";
		world->uid = uid;
	}
	disk_init(world->uid);

	// BlockLight light = color_and_range_to_blocklight((Color){ 255, 255, 0, 1 }, 15);
	// print_blocklight(light);
	// VINFO("%i", get_max_threads());

	Player *player = &world->player;
	while (!glfwWindowShouldClose(window.glfw)) {
		BEGIN_SECT("main loop");
		float current_frame = glfwGetTime();
		game_state.delta_time = current_frame - game_state.last_frame;
		game_state.last_frame = current_frame;

		// VINFO("FPS: %f",1/game_state.delta_time);

		process_input(window.glfw, player);

		if (game_state.paused) {
			glfwWaitEvents();
			threadpool_pause(&world->thread);
			END_SECT("main loop");
			continue;
		}
		threadpool_resume(&world->thread);

		BEGIN_SECT("Bin search");
		player_update(player);

		if (player->breaking) {
			player_break_block(&game_state.world.pool, &game_state.world.ogl_pool, &game_state.world.render_map, &player->pos, world->seed);
			player->breaking = false;
		}
		if (player->placing) {
			player_place_block(player, BlocktypeStone, &game_state.world.pool, &game_state.world.ogl_pool, &game_state.world.render_map, &player->pos, world->seed);
			player->placing = false;
		}
		END_SECT("Bin search");
		world_update(world);

		render(&game_state, window, shader_data);

		END_SECT("main loop");
#ifdef PROFILING
		spall_buffer_flush(&spall_ctx, &spall_buffer);
#endif // PROFILING
	}
	running = false;
	threadpool_cleanup(&world->thread);

#ifdef PROFILING
	spall_buffer_quit(&spall_ctx, &spall_buffer);
	spall_quit(&spall_ctx);
#endif //PROFILING

	glfwDestroyWindow(window.glfw);
	glfwTerminate();
}
