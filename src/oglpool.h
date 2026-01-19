#ifndef INCLUDE_SRC_OGLPOOL_H_
#define INCLUDE_SRC_OGLPOOL_H_

#include <stdint.h>

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
	unsigned int fullblock_EBO;
} OGLPool;
// #define VERTICES_PER_FACE 6
#define FACES_PER_CUBE 6
#define VERTICES_PER_QUAD 4
#define INDICES_PER_QUAD 6
#define TRIANGLES_PER_QUAD 2
#define RENDER_DISTANCE_X 8
#define RENDER_DISTANCE_Y 8
#define RENDER_DISTANCE_Z 8
// #define RENDER_DISTANCE_X 2
// #define RENDER_DISTANCE_Y 2
// #define RENDER_DISTANCE_Z 2
#define RENDER_TOTAL_X (RENDER_DISTANCE_X + RENDER_DISTANCE_X + 1)
#define RENDER_TOTAL_Y (RENDER_DISTANCE_Y + RENDER_DISTANCE_Y + 1)
#define RENDER_TOTAL_Z (RENDER_DISTANCE_Z + RENDER_DISTANCE_Z + 1)
#define MAX_VISIBLE_CHUNKS (2 * RENDER_DISTANCE_X + 1) * (2 * RENDER_DISTANCE_Y + 1) * (2 * RENDER_DISTANCE_Z + 1)
#define MAX_FACES_PER_CHUNK (CHUNK_TOTAL_BLOCKS * FACES_PER_CUBE)
#define MAX_VERTICES_PER_CHUNK (MAX_FACES_PER_CHUNK * VERTICES_PER_QUAD)

void oglpool_init(OGLPool *pool, size_t cap);

#endif // INCLUDE_SRC_OGLPOOL_H_
