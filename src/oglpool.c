#include "oglpool.h"
#include "chunk.h"
#include "loadopengl.h"
#include "logs.h"
#include "vassert.h"
#include <stddef.h>
#include <string.h>

static void vao_attributes(unsigned int VAO)
{
	glBindVertexArray(VAO);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
			      sizeof(Vertex),
			      (void *)offsetof(Vertex, pos));
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
			      sizeof(Vertex),
			      (void *)offsetof(Vertex, tex));
	glEnableVertexAttribArray(1);

	glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT,
			       sizeof(Vertex),
			       (void *)offsetof(Vertex, norm));
	glEnableVertexAttribArray(2);
	glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT,
			       sizeof(Vertex),
			       (void *)offsetof(Vertex, light));
	glEnableVertexAttribArray(3);
}

static void oglitem_init(OGLItem *item)
{
	glGenVertexArrays(1, &item->VAO);
	glGenBuffers(1, &item->VBO);

	glBindVertexArray(item->VAO);
	glBindBuffer(GL_ARRAY_BUFFER, item->VBO);
	vao_attributes(item->VAO);
}
void oglpool_init(OGLPool *pool, size_t cap)
{
	memset(pool, 0, sizeof(*pool));
	pool->cap = cap;
	pool->items = calloc(pool->cap, sizeof(pool->items[0]));
	pool->free_stack = calloc(pool->cap, sizeof(pool->free_stack[0]));
	VASSERT_RELEASE_MSG(pool->items != NULL, "Buy more RAM...");
	for (size_t i = 0; i < pool->cap; i++) {
		oglitem_init(&pool->items[i]);
	}
	while (pool->free_count < pool->cap) {
		pool->free_stack[pool->free_count] = pool->free_count;
		pool->free_count++;
	}
}
static bool oglpool_claim(OGLPool *pool, size_t *index)
{
	if (pool->used >= pool->cap) {
		VERROR("OGLPool full");
		return false;
	}

	pool->free_count--;
	*index = pool->free_stack[pool->free_count];
	VASSERT(pool->items[*index].references == 0);
	pool->items[*index].references = 1;
	pool->used++;
	return true;
}
static void oglpool_release(OGLPool *pool, size_t index)
{
	VASSERT(pool->items[index].references > 0);
	pool->items[index].references--;
	if (pool->items[index].references == 0) {
		VASSERT(pool->used != 0);
		pool->used--;
		pool->free_stack[pool->free_count] = index;
		pool->free_count++;
	}
}
void oglpool_reference(OGLPool *pool, size_t index)
{
	VASSERT(pool->items[index].references > 0);
	pool->items[index].references++;
}
bool oglpool_claim_chunk(OGLPool *pool, Chunk *chunk)
{
	VASSERT(!chunk->has_oglpool_reference);
	if (oglpool_claim(pool, &chunk->oglpool_index)) {
		chunk->has_oglpool_reference = true;
		return true;
	}
	return false;
}
void oglpool_release_chunk(OGLPool *pool, Chunk *chunk)
{
	VASSERT_WARN(chunk->has_oglpool_reference);
	if (chunk->has_oglpool_reference) {
		oglpool_release(pool, chunk->oglpool_index);
		chunk->has_oglpool_reference = false;
	}
}
void oglpool_reference_chunk(OGLPool *pool, Chunk *chunk, size_t index)
{
	chunk->oglpool_index = index;
	chunk->has_oglpool_reference = true;
	oglpool_reference(pool, chunk->oglpool_index);
}
