#ifndef INCLUDE_SRC_OGLPOOL_H_
#define INCLUDE_SRC_OGLPOOL_H_

#include <stdbool.h>
#include <stddef.h>
typedef struct {
	unsigned int VAO;
	unsigned int VBO;
	size_t references;
} OGLItem;
typedef struct {
	OGLItem *items;
	size_t *free_stack;
	size_t free_count;
	size_t used;
	size_t cap;
} OGLPool;

void oglpool_init(OGLPool *pool, size_t cap);
bool oglpool_claim(OGLPool *pool, size_t *index);
void oglpool_release(OGLPool *pool, size_t index);
void oglpool_reference(OGLPool *pool, size_t index);

#endif  // INCLUDE_SRC_OGLPOOL_H_
