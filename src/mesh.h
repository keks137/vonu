#ifndef INCLUDE_SRC_MESH_H_
#define INCLUDE_SRC_MESH_H_

#include "chunk.h"
#include "map.h"
#include "map.h"
#include "pool.h"
#include "thread.h"
#include <stddef.h>

struct MeshSubQueue {
	ChunkCoord *chunks;
	size_t mask;
	size_t writei;
	size_t readi;
};

#define MESH_CHUNKS_INVOLVED 27
#define MESH_INVOLVED_CENTRE 13
#define MESH_INVOLVED_STRIDE_Y 9
#define MESH_INVOLVED_STRIDE_Z 3

typedef size_t MeshUserIndex;
typedef size_t MeshUploadIndex;

typedef struct {
	MeshUserIndex user;
	MeshUploadIndex up;
} MeshResourceHandle;

void meshworker_process_mesh(Worker *worker, MeshResourcePool *pool, MeshResourceHandle *res, WorkerTask *task);
#endif // INCLUDE_SRC_MESH_H_
