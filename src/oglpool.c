#include "oglpool.h"
#include "loadopengl.h"
#include "logs.h"
#include "vassert.h"
#include <string.h>
static void vao_attributes_no_color(unsigned int VAO)
{
	glBindVertexArray(VAO);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
}
static void oglitem_init(OGLItem *item)
{
	glGenVertexArrays(1, &item->VAO);
	glGenBuffers(1, &item->VBO);

	glBindVertexArray(item->VAO);
	glBindBuffer(GL_ARRAY_BUFFER, item->VBO);
	vao_attributes_no_color(item->VAO);
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
bool oglpool_claim(OGLPool *pool, size_t *index)
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
void oglpool_release(OGLPool *pool, size_t index)
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
