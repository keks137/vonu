#ifndef INCLUDE_SRC_GAME_H_
#define INCLUDE_SRC_GAME_H_

#include "core.h"
#include "pool.h"
#include "thread.h"

#if defined(_MSC_VER)
#define EXTERN __declspec(dllexport)
#define IMPORT __declspec(dllimport)
#else
#define EXTERN __attribute__((visibility("default")))
#define IMPORT
#endif

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

typedef struct {
	unsigned int program;
	unsigned int model_loc;
	unsigned int projection_loc;
	unsigned int view_loc;
} ShaderData;

typedef struct {
	ChunkCoord coord;
	int64_t x_min, x_max, y_min, y_max, z_min, z_max;
} ChunkIterator;

static inline void threadpool_pause(ThreadPool *pool)
{
	atomic_store(&pool->system.paused, true);
}
static inline void threadpool_resume(ThreadPool *pool)
{
	atomic_store(&pool->system.paused, false);
}

#ifdef HOT_RELOAD
typedef void (*GameFrameFn)(WindowData *window, ShaderData *shader);
typedef void (*ThreadsFrameFn)(Worker *worker);
#else
void game_frame(WindowData *window, ShaderData *shader);
void threads_frame(Worker *worker);
#endif // HOT_RELOAD

extern GameState game_state;

static inline void snooze(uint64_t ms)
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
#endif // INCLUDE_SRC_GAME_H_
