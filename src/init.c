#include "logs.h"
#include "map.h"
#include "pool.h"
#include "loadopengl.h"
#include "vassert.h"
#include <stdbool.h>
#include <string.h>
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

bool rendermap_init(RenderMap *map, size_t table_size, size_t num_buffers)
{
	map->table_size = table_size;
	map->num_buffers = num_buffers;

	map->entry = calloc(num_buffers, sizeof(map->entry[0]));
	if (map->entry == NULL) {
		VERROR("Failed to alloc render map entries");
		abort();
	}

	for (size_t i = 0; i < num_buffers; i++) {
		map->entry[i] = calloc(table_size, sizeof(map->entry[0][0]));
		if (map->entry == NULL) {
			VERROR("Failed to alloc render map entry: %zu", i);
			abort();
		}
	}

	return true;
}

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
