#ifndef INCLUDE_SRC_OGLPOOL_H_
#define INCLUDE_SRC_OGLPOOL_H_

#define VERTICES_PER_FACE 6
#define FACES_PER_CUBE 6

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

#endif  // INCLUDE_SRC_OGLPOOL_H_
