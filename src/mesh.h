#ifndef INCLUDE_SRC_MESH_H_
#define INCLUDE_SRC_MESH_H_

#include "chunk.h"
#include "map.h"
#include "pool.h"
typedef struct {
	ChunkCoord *chunks;
	size_t mask;
	size_t writei;
	size_t readi;
	Block *blockdata;
	Chunk *involved;
} MeshQueue;

void meshqueue_init(MeshQueue *mesh, size_t cap);
void meshqueue_process(MeshQueue *mesh, ChunkPool *pool, RenderMap *map, OGLPool *ogl, ChunkVertsScratch *scratch, size_t seed);


#endif  // INCLUDE_SRC_MESH_H_
