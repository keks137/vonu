#include "pool.h"
#include "chunk.h"
#include "logs.h"
#include "vassert.h"
#include <stddef.h>
#include <string.h>
void pool_reserve(ChunkPool *pool)
{
	pool->blockdata = realloc(pool->blockdata, sizeof(Block) * (pool->cap) * CHUNK_TOTAL_BLOCKS);
	VASSERT_MSG(pool->blockdata != NULL, "Buy more ram bozo");
	Block *start_new_blocks = pool->blockdata + sizeof(Block) * CHUNK_TOTAL_BLOCKS * pool->lvl;
	memset(start_new_blocks, 0, pool->cap * sizeof(Block) * CHUNK_TOTAL_BLOCKS);

	for (size_t i = 0; i < pool->cap; i++) {
		Chunk chunk = { 0 };
		chunk.data = start_new_blocks + i * CHUNK_TOTAL_BLOCKS;
		pool->chunk[pool->lvl + i] = chunk;
	}
}
size_t pool_append(ChunkPool *pool, const ChunkCoord *coord, size_t seed)
{
	VASSERT(pool->lvl < pool->cap);
	chunk_clear_metadata(&pool->chunk[pool->lvl]);
	chunk_load(&pool->chunk[pool->lvl], coord, seed);
	return pool->lvl++;
}

void pool_replace(ChunkPool *pool, const ChunkCoord *coord, size_t index, size_t seed)
{
	VASSERT(pool->lvl > index);
	chunk_clear_metadata(&pool->chunk[index]);
	chunk_load(&pool->chunk[index], coord, seed);
}
size_t pool_append_keep_ogl(ChunkPool *pool, OGLPool *ogl, size_t ogl_index, const ChunkCoord *coord, size_t seed)
{
	VASSERT(pool->lvl < pool->cap);
	Chunk *chunk = &pool->chunk[pool->lvl];
	chunk_clear_metadata(chunk);
	chunk_load(chunk, coord, seed);
	oglpool_reference_chunk(ogl, chunk, ogl_index);
	return pool->lvl++;
}
void pool_replace_keep_ogl(ChunkPool *pool, OGLPool *ogl, size_t ogl_index, const ChunkCoord *coord, size_t index, size_t seed)
{
	VASSERT(pool->lvl > index);
	Chunk *chunk = &pool->chunk[index];
	chunk_clear_metadata(chunk);
	chunk_load(chunk, coord, seed);
	oglpool_reference_chunk(ogl, chunk, ogl_index);
}
bool pool_find_chunk_coord(ChunkPool *pool, const ChunkCoord *coord, size_t *index)
{
	size_t idx;
	if (index == NULL)
		index = &idx;
	for (size_t i = 0; i < pool->lvl; i++) {
		Chunk *pc = &pool->chunk[i];
		if (pc->coord.x == coord->x &&
		    pc->coord.y == coord->y &&
		    pc->coord.z == coord->z) {
			*index = i;
			return true;
		}
	}
	return false;
}

bool pool_get_index(ChunkPool *pool, Chunk *chunk, size_t *index)
{
	for (size_t i = 0; i < pool->lvl; i++) {
		if (
			chunk->coord.x == pool->chunk[i].coord.x &&
			chunk->coord.y == pool->chunk[i].coord.y &&
			chunk->coord.z == pool->chunk[i].coord.z) {
			*index = i;
			return true;
		}
	}
	return false;
}

size_t pool_replaceable_index(ChunkPool *pool)
{
	size_t index = 0;
	time_t oldest = time(NULL);

	for (size_t i = 0; i < pool->lvl; i++) {
		Chunk *pc = &pool->chunk[i];
		if (pc->last_used < oldest) {
			oldest = pc->last_used;
			index = i;
		}
	}

	return index;
}

bool pool_add_keep_ogl(ChunkPool *pool, size_t *index, OGLPool *ogl, size_t ogl_index, const ChunkCoord *coord, size_t seed)
{
	size_t idx;
	if (index == NULL)
		index = &idx;

	if (pool->lvl < pool->cap) {
		*index = pool_append_keep_ogl(pool, ogl, ogl_index, coord, seed);
		return true;
	} else {
		VWARN("pool full");
		*index = pool_replaceable_index(pool);
		pool_replace_keep_ogl(pool, ogl, ogl_index, coord, *index, seed);
		return true;
	}
}
void pool_load_chunk(ChunkPool *pool, OGLPool *ogl, RenderMap *map, const ChunkCoord *pos, Chunk **chunk, size_t seed)
{
	size_t index;
	bool loaded = pool_load(pool, ogl, map, &index, pos, seed);
	VASSERT(loaded);
	*chunk = &pool->chunk[index];
	VASSERT(chunk != NULL);
}
void pool_update_chunk(ChunkPool *pool, OGLPool *ogl, RenderMap *map, const ChunkCoord *pos, Chunk **chunk, size_t seed)
{
	size_t index;
	VASSERT(pool_load(pool, ogl, map, &index, pos, seed));
	*chunk = &pool->chunk[index];
	VASSERT(chunk != NULL);
	(*chunk)->up_to_date = false;
	(*chunk)->modified = true;
	(*chunk)->updates_this_cycle++;
	(*chunk)->cycles_since_update = 0;
}

void pool_read_chunk(ChunkPool *pool, OGLPool *ogl, RenderMap *map, const ChunkCoord *pos, Chunk **chunk, size_t seed)
{
	size_t index;
	VASSERT(pool_load(pool, ogl, map, &index, pos, seed));
	*chunk = &pool->chunk[index];
	VASSERT(chunk != NULL);
}

void print_pool(ChunkPool *pool)
{
	VINFO("Pool: %p", pool);
	VINFO("lvl: %zu", pool->lvl);
	VINFO("cap: %zu", pool->cap);
#if 0
	for (size_t i = 0; i < pool->lvl; i++) {
		VINFO("Chunk index: %zu", i);
		print_chunk(&pool->chunk[i]);
	}
#endif
}
static inline Block chunk_read_block(const Chunk *chunk, const BlockPos *pos)
{
	return chunk->data[CHUNK_INDEX(pos->x, pos->y, pos->z)];
}

// NOTE: don't rawdog this function, it doesn't track chunk->block_count
static inline Block chunk_replace_block(Chunk *chunk, BlockPos *pos, Block block)
{
	Block *chnkblock = &chunk->data[CHUNK_INDEX(pos->x, pos->y, pos->z)];
	Block ret = *chnkblock;
	*chnkblock = block;
	return ret;
}

Block pool_read_block(ChunkPool *pool, OGLPool *ogl, RenderMap *map, const WorldCoord *pos, size_t seed)
{
	ChunkCoord chunk_pos = { 0 };
	BlockPos block_pos = { 0 };
	world_cord_to_chunk_and_block(pos, &chunk_pos, &block_pos);
	Chunk *chunk = NULL;
	pool_load_chunk(pool, ogl, map, &chunk_pos, &chunk, seed);
	VASSERT(chunk != NULL);
	VASSERT(chunk->data != NULL);
	VASSERT(block_pos.x >= 0 && block_pos.x < CHUNK_TOTAL_X &&
		block_pos.z >= 0 && block_pos.z < CHUNK_TOTAL_Z &&
		block_pos.y >= 0 && block_pos.y < CHUNK_TOTAL_Y);
	Block ret = chunk_read_block(chunk, &block_pos);
	return ret;
}
void pool_replace_block(ChunkPool *pool, OGLPool *ogl, RenderMap *map, const WorldCoord *pos, Block block, size_t seed)
{
	ChunkCoord chunk_pos = { 0 };
	BlockPos block_pos = { 0 };
	world_cord_to_chunk_and_block(pos, &chunk_pos, &block_pos);
	Chunk *chunk = NULL;
	pool_load_chunk(pool, ogl, map, &chunk_pos, &chunk, seed);
	VASSERT(chunk != NULL);
	VASSERT(chunk->data != NULL);
	VASSERT(block_pos.x >= 0 && block_pos.x < CHUNK_TOTAL_X &&
		block_pos.z >= 0 && block_pos.z < CHUNK_TOTAL_Z &&
		block_pos.y >= 0 && block_pos.y < CHUNK_TOTAL_Y);

	Block prevblock = chunk_replace_block(chunk, &block_pos, block);
	if (block.type != prevblock.type) {
		chunk->up_to_date = false;
		chunk->modified = true;
		chunk->updates_this_cycle++;
		chunk->cycles_since_update = 0;
		// VINFO("hi");
		if (prevblock.type == BlocktypeAir) {
			// VINFO("superhi");
			chunk->block_count++;
		}
	}
}

bool pool_add(ChunkPool *pool, size_t *index, const ChunkCoord *coord, size_t seed)
{
	size_t idx;
	if (index == NULL)
		index = &idx;

	if (pool->lvl < pool->cap) {
		*index = pool_append(pool, coord, seed);
		return true;
	} else {
		*index = pool_replaceable_index(pool);
		pool_replace(pool, coord, *index, seed);
		return index;
	}
}

void pool_empty(ChunkPool *pool, OGLPool *ogl, size_t index)
{
	VASSERT(pool->lvl > index);
	// TODO: store completely empty chunks somewhere else, since they don't need a VBO anyways
	// VASSERT_MSG(pool->chunk[index].up_to_date || pool->chunk[index].block_count == 0, "There was no reason to unload this");
	Chunk *chunk = &pool->chunk[index];
	if (chunk->has_oglpool_reference)
		oglpool_release_chunk(ogl, chunk);

	pool->lvl--;
	Chunk replacement = pool->chunk[pool->lvl];
	pool->chunk[pool->lvl].data = pool->chunk[index].data;
	pool->chunk[index] = replacement;
	// print_chunk(&pool->chunk[index]);
}

bool pool_load_relaxed(ChunkPool *pool, RenderMap *map, const ChunkCoord *coord, size_t *index, size_t seed)
{
	if (rendermap_find(map, coord, index)) {
		return true;
	}
	if (pool_find_chunk_coord(pool, coord, index)) {
		return true;
	}
	if (pool->lvl < pool->cap) {
		*index = pool_append(pool, coord, seed);

		return true;
	}

	return false;
}

bool pool_load(ChunkPool *pool, OGLPool *ogl, RenderMap *map, size_t *index, const ChunkCoord *coord, size_t seed)
{
	size_t idx;
	if (index == NULL)
		index = &idx;

	if (pool_find_chunk_coord(pool, coord, index)) {
		return index;
	}
	if (rendermap_find(map, coord, index)) {
		Chunk *old_chunk = &map->entry[map->current_buffer][*index].chunk;
		if (old_chunk->has_oglpool_reference) {
			if (pool_add_keep_ogl(pool, index, ogl, old_chunk->oglpool_index, coord, seed))
				return true;
		}
	}

	// TODO: cache disk chunks in hash set

	return pool_add(pool, index, coord, seed);
}
