#ifndef INCLUDE_SRC_THREAD_H_
#define INCLUDE_SRC_THREAD_H_

#include <stddef.h>
#include "chunk.h"
#include "pool.h"
#ifdef _WIN32
#include <windows.h>

#else
#include <pthread.h>
#endif //_WIN32

typedef enum {
	WORKER_TASK_MESH_UPD,
	WORKER_TASK_MESH_NEW,
	WORKER_TASK_CHUNK_LOAD,
	WORKER_TASK_CHUNK_SAVE,
	WORKER_TASK_LIGHT_PROPAGATION
} WorkerTaskType;

typedef struct {
	// WorkerTaskType type;
	ChunkCoord coord;
	void *data;
	size_t data_size;
} WorkerTask;

#ifdef _WIN32
typedef CRITICAL_SECTION Mutex;
typedef CONDITION_VARIABLE Conditional;
typedef HANDLE Thread;
#else
typedef pthread_mutex_t Mutex;
typedef pthread_cond_t Conditional;
typedef pthread_t Thread;
#endif

#ifdef __STDC_NO_ATOMICS__
#error need atomics
#else
#include <stdatomic.h>
#endif //__STDC_NO_ATOMICS__

typedef struct {
	WorkerTask *tasks;
	size_t capacity;
	atomic_size_t writepos;
	atomic_size_t readpos;

	// Mutex mutex;
	// Conditional not_empty;
	// Conditional not_full;

} WorkerQueue;

typedef struct {
	WorkerQueue mesh_up;
	WorkerQueue mesh_new;
	// volatile bool shutdown; //needed for saving on exit
	atomic_bool paused;
} WorkerSystem;
typedef struct {
	ChunkCoord coord;
	ChunkVertsScratch buf;
	size_t block_count;
} MeshUploadData;

enum { UPLOAD_STATUS_FREE,
       UPLOAD_STATUS_PROCESSING,
       UPLOAD_STATUS_READY,
};
typedef struct {
	BlockLight *data;
	size_t lvl;
	size_t cap;
} LightScratch;
typedef struct {
	atomic_bool *taken;
	// struct MeshResource *resources;
	size_t max_users;
	Block *blockdata;
	Chunk *involved;
	LightScratch *light_scratch;
	size_t max_uploads;
	atomic_size_t *upload_status;
	MeshUploadData *upload_data;
} MeshResourcePool;

typedef struct {
	// 	struct MeshSubQueue up;
	// 	struct MeshSubQueue new;
	Block *blockdata;
	Chunk *involved;
} MeshQueue;

typedef struct {
	ChunkPool *pool;
	OGLPool *ogl_pool;
	RenderMap *render_map;
	MeshQueue *mesh_queue;
	WorkerSystem *system;
	MeshResourcePool *mesh_resource;
	size_t seed;
} WorkerContext;

typedef struct {
	Thread handle;
	WorkerContext context;
	uint64_t id;
	volatile bool running;
} Worker;

typedef enum {
	INPUT_EVENT_MOUSE_MOVE,
	INPUT_EVENT_MOUSE_BTN,
	INPUT_EVENT_KEY
} InputEventType;

typedef struct {
	InputEventType type;
	union {
		struct {
			int32_t dx, dy;
		} mouse_move;
		struct {
			uint32_t btn;
			bool down;
		} mouse_btn;
		struct {
			uint32_t code;
			bool down;
		} key;
	};
} InputEvent;

typedef struct {
	InputEvent *events;
	size_t cap;
	atomic_size_t writepos;
	atomic_size_t readpos;
} InputRing;
typedef struct {
	Thread handle;
	InputRing *ring;
	volatile bool running;
} InputWorker;
typedef struct {
	WorkerContext shared_context;
	size_t num;
	Worker *workers;
	WorkerSystem system;
	MeshResourcePool mesh_resource;
} ThreadPool;
void threadpool_init(ThreadPool *thread, size_t num_workers, ChunkPool *pool, OGLPool *ogl_pool, RenderMap *render_map, MeshQueue *mesh_queue, size_t seed);
bool workerqueue_push(WorkerQueue *queue, WorkerTask task);
void workerqueue_push_override(WorkerQueue *queue, WorkerTask task);
void meshqueue_process(size_t max, MeshResourcePool *meshp, RenderMap *map, OGLPool *ogl);
bool workerqueue_is_empty(WorkerQueue *queue);
bool meshs_empty(WorkerSystem *system);

#endif // INCLUDE_SRC_THREAD_H_
