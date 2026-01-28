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
	GL_CHECK((void)0);
}

static void oglitem_init(OGLPool *pool, OGLItem *item)
{
	glGenVertexArrays(1, &item->VAO);
	glGenBuffers(1, &item->VBO);

	glBindVertexArray(item->VAO);
	glBindBuffer(GL_ARRAY_BUFFER, item->VBO);
	vao_attributes(item->VAO);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pool->fullblock_EBO);
	GL_CHECK((void)0);
}
static void oglpool_upload_indeces(OGLPool *pool)
{
	size_t max_indices = MAX_FACES_PER_CHUNK * INDICES_PER_QUAD;
	uint16_t *all_indices = malloc(max_indices * sizeof(uint16_t));
	VPANIC_MSG(all_indices != NULL, "Buy more RAM...");

	for (size_t i = 0; i < MAX_FACES_PER_CHUNK; i++) {
		uint16_t base = i * VERTICES_PER_QUAD;
		all_indices[i * FACES_PER_CUBE + 0] = base + 0;
		all_indices[i * FACES_PER_CUBE + 1] = base + 1;
		all_indices[i * FACES_PER_CUBE + 2] = base + 2;
		all_indices[i * FACES_PER_CUBE + 3] = base + 0;
		all_indices[i * FACES_PER_CUBE + 4] = base + 2;
		all_indices[i * FACES_PER_CUBE + 5] = base + 3;
	}

	glGenBuffers(1, &pool->fullblock_EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pool->fullblock_EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		     max_indices * sizeof(uint16_t),
		     all_indices, GL_STATIC_DRAW);

	free(all_indices);
}
void oglpool_init(OGLPool *pool, size_t cap)
{
	memset(pool, 0, sizeof(*pool));
	pool->cap = cap;
	pool->items = calloc(pool->cap, sizeof(pool->items[0]));
	pool->free_stack = calloc(pool->cap, sizeof(pool->free_stack[0]));
	VPANIC_MSG(pool->items != NULL, "Buy more RAM...");

	oglpool_upload_indeces(pool);
	for (size_t i = 0; i < pool->cap; i++) {
		oglitem_init(pool, &pool->items[i]);
	}

	pool->free_count++; // ignore first
	while (pool->free_count < pool->cap) {
		pool->free_stack[pool->free_count] = pool->free_count;
		pool->free_count++;
	}
	pool->free_count--; // ignore first
	VASSERT(pool->free_count < pool->cap);
}
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
