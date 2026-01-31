#include "oglpool.h"
#include "chunk.h"
#include "loadopengl.h"
#include "logs.h"
#include "vassert.h"
#include <stddef.h>
#include <string.h>



static bool oglpool_claim(OGLPool *pool, uint32_t *index)
{
	if (pool->used >= pool->cap) {
		VERROR("OGLPool full");
		return false;
	}

	pool->free_count--;
	*index = pool->free_stack[pool->free_count];
	VASSERT_MSG(index != 0, "Invalid reference");
	VASSERT(pool->items[*index].references == 0);
	pool->items[*index].references = 1;
	pool->used++;
	return true;
}
static void oglpool_release(OGLPool *pool, uint32_t index)
{
	VASSERT(pool->items[index].references > 0);
	pool->items[index].references--;
	if (pool->items[index].references == 0) {
		VASSERT(pool->used > 1);
		pool->used--;
		pool->free_stack[pool->free_count] = index;
		pool->free_count++;
	}
}
void oglpool_reference(OGLPool *pool, uint32_t index)
{
	VASSERT(pool->items[index].references > 0);
	pool->items[index].references++;
}
bool oglpool_claim_chunk(OGLPool *pool, Chunk *chunk)
{
	VASSERT(chunk->oglpool_index == 0);
	if (oglpool_claim(pool, &chunk->oglpool_index)) {
		return true;
	}
	return false;
}
void oglpool_release_chunk(OGLPool *pool, RenderMapChunk *chunk)
{
	VASSERT(chunk->oglpool_index != 0);
	if (chunk->oglpool_index != 0) {
		oglpool_release(pool, chunk->oglpool_index);
		chunk->oglpool_index = 0;
	}
}
void oglpool_reference_chunk(OGLPool *pool, RenderMapChunk *chunk, size_t index)
{
	chunk->oglpool_index = index;
	oglpool_reference(pool, chunk->oglpool_index);
}
