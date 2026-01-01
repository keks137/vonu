#ifndef INCLUDE_SRC_POOL_H_
#define INCLUDE_SRC_POOL_H_


#include "chunk.h"
#include "map.h"
#include "oglpool.h"
typedef struct {
	Chunk *chunk;
	size_t lvl;
	size_t cap;
} ChunkPoolUnloaded;

typedef struct {
	Chunk *chunk;
	Block *blockdata;
	size_t lvl;
	size_t cap;
} ChunkPool;
bool pool_get_index(ChunkPool *pool, Chunk *chunk, size_t *index);
void print_pool(ChunkPool *pool);
void pool_reserve(ChunkPool *pool);
void pool_empty(ChunkPool *pool, OGLPool *ogl, size_t index);
bool pool_load_relaxed(ChunkPool *pool, RenderMap *map, const ChunkCoord *coord, size_t *index, size_t seed);
bool pool_load(ChunkPool *pool, OGLPool *ogl, RenderMap *map, size_t *index, const ChunkCoord *coord, size_t seed);
void pool_update_chunk(ChunkPool *pool, OGLPool *ogl, RenderMap *map, const ChunkCoord *pos, Chunk **chunk, size_t seed);
void pool_replace_block(ChunkPool *pool, OGLPool *ogl, RenderMap *map, const WorldCoord *pos, Block block, size_t seed);
Block pool_read_block(ChunkPool *pool, OGLPool *ogl, RenderMap *map, const WorldCoord *pos, size_t seed);

void print_pool(ChunkPool *pool);

#endif  // INCLUDE_SRC_POOL_H_
