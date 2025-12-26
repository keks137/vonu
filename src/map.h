#ifndef INCLUDE_SRC_MAP_H_
#define INCLUDE_SRC_MAP_H_

#include "chunk.h"
#include <stddef.h>
typedef enum {
	HashMapFlagOccupied = 1 << 0,
	HashMapFlagCarry = 1 << 1,
	HashMapFlagDeleted = 1 << 2,
} HashMapFlags;

typedef struct {
	Chunk chunk;
	uint8_t flags;
	uint8_t off_by;
} RenderMapEntry;

typedef struct {
	RenderMapEntry **entry;
	size_t num_buffers;
	size_t current_buffer;
	size_t count;
	size_t count_next;
	size_t table_size;
	size_t deleted_count;
	// uint16_t cap;
} RenderMap;

bool rendermap_init(RenderMap *map, size_t table_size, size_t num_buffers);
bool rendermap_get_chunk(RenderMap *map, Chunk *chunk, ChunkCoord *key);
bool rendermap_add_next_buffer(RenderMap *map, Chunk *chunk);
void rendermap_advance_buffer(RenderMap *map);
bool rendermap_add(RenderMap *map, Chunk *chunk);
bool rendermap_find(const RenderMap *map, ChunkCoord *key, size_t *index);
bool rendermap_remove(RenderMap *map, ChunkCoord *key);

#endif // INCLUDE_SRC_MAP_H_
