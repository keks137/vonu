#ifndef INCLUDE_SRC_MESH_H_
#define INCLUDE_SRC_MESH_H_

#include "chunk.h"
#include "map.h"
#include "pool.h"
#include <stddef.h>

struct MeshSubQueue {
	ChunkCoord *chunks;
	size_t mask;
	size_t writei;
	size_t readi;
};
typedef struct {
	struct MeshSubQueue up;
	struct MeshSubQueue new;
	Block *blockdata;
	Chunk *involved;
} MeshQueue;

void meshqueue_init(MeshQueue *mesh, size_t up_cap, size_t new_cap);
void meshqueue_process(size_t max, MeshQueue *mesh, ChunkPool *pool, RenderMap *map, OGLPool *ogl, ChunkVertsScratch *scratch, size_t seed);

#endif // INCLUDE_SRC_MESH_H_
