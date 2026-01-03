#ifndef INCLUDE_SRC_MESH_H_
#define INCLUDE_SRC_MESH_H_

#include "chunk.h"
#include "map.h"
#include "map.h"
#include "pool.h"
#include <stddef.h>

struct MeshSubQueue {
	ChunkCoord *chunks;
	size_t mask;
	size_t writei;
	size_t readi;
};

#define MESH_CHUNKS_INVOLVED 27
#define MESH_INVOLVED_CENTRE 14
#define MESH_INVOLVED_STRIDE_Y 9
#define MESH_INVOLVED_STRIDE_Z 3

typedef struct {
	// 	struct MeshSubQueue up;
	// 	struct MeshSubQueue new;
	Block *blockdata;
	Chunk *involved;
} MeshQueue;

void meshqueue_init(MeshQueue *mesh, size_t up_cap, size_t new_cap);

#endif // INCLUDE_SRC_MESH_H_
