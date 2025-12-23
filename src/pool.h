#ifndef INCLUDE_SRC_POOL_H_
#define INCLUDE_SRC_POOL_H_


#include "chunk.h"
typedef struct {
	Chunk *chunk;
	size_t lvl;
	size_t cap;
} ChunkPoolUnloaded;

typedef struct {
	ChunkPoolUnloaded unloaded;
	Chunk *chunk;
	size_t lvl;
	size_t cap;
} ChunkPool;
bool pool_get_index(ChunkPool *pool, Chunk *chunk, size_t *index);
void print_pool(ChunkPool *pool);

#endif  // INCLUDE_SRC_POOL_H_
