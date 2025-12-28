#include "map.h"
#include "chunk.h"
#include "logs.h"
#include "oglpool.h"
#include "vassert.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static inline uint32_t triple_hash(const RenderMap *map, const ChunkCoord *key)
{
	uint64_t h = key->x;
	h ^= key->y + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
	h ^= key->z + 0xbf58476d1ce4e5b9ULL + (h << 6) + (h >> 2);

	h ^= h >> 33;
	h *= 0xff51afd7ed558ccdULL;
	h ^= h >> 33;

	return (uint32_t)(h & (map->table_size - 1));
}

static inline bool chunkcoord_match(const ChunkCoord *a, const ChunkCoord *b)
{
	return (a->x == b->x) && (a->y == b->y) && (a->z == b->z);
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

bool rendermap_get_chunk(RenderMap *map, Chunk *chunk, ChunkCoord *key)
{
	size_t index;
	if (rendermap_find(map, key, &index)) {
		// (void)chunk; // shut up clangd
		*chunk = map->entry[map->current_buffer][index].chunk;
		return true;
	}
	return false;
}

static bool rendermap_add_opt(RenderMap *map, OGLPool *pool, Chunk *chunk, size_t buffer, uint32_t *index)
{
	uint32_t idx;
	if (index == NULL)
		index = &idx;
	*index = triple_hash(map, &chunk->coord);
	uint32_t start_index = *index;
	uint8_t off_count = 0;

	do {
		RenderMapEntry *entry = &map->entry[buffer][*index];

		if (entry->flags & HashMapFlagDeleted) {
			memset(entry, 0, sizeof(*entry));
			entry->chunk = *chunk;
			if (chunk->has_oglpool_reference)
				oglpool_reference(pool, chunk->oglpool_index);
			entry->flags |= HashMapFlagOccupied;
			entry->flags &= ~HashMapFlagDeleted;
			entry->off_by = off_count;
			map->deleted_count--;
		} else {
			if (entry->flags & HashMapFlagOccupied) {
				if (entry->off_by == off_count)
					entry->flags |= HashMapFlagCarry;

				if (chunkcoord_match(&entry->chunk.coord, &chunk->coord)) {
					// TODO: maybe reuse it instead

					// VASSERT(entry->chunk.oglpool_index == chunk->oglpool_index);
					if (entry->chunk.oglpool_index != chunk->oglpool_index) {
						VWARN("Replacing map's reference instead of reusing");
						oglpool_release(pool, entry->chunk.oglpool_index);
					}
					entry->chunk = *chunk;

					return true;
				}
			} else {
				entry->chunk = *chunk;
				if (chunk->has_oglpool_reference)
					oglpool_reference(pool, chunk->oglpool_index);
				entry->flags |= HashMapFlagOccupied;
				entry->off_by = off_count;
				map->count++;
				return true;
			}
		}

		off_count++;

		*index = (*index + 1) & (map->table_size - 1);
		if (*index == start_index)
			break;

	} while (true);

	VERROR("Hashmap full failed cycle");
	return false;
}
bool rendermap_add(RenderMap *map, OGLPool *pool, Chunk *chunk)
{
	return rendermap_add_opt(map, pool, chunk, map->current_buffer, NULL);
}

bool rendermap_find(const RenderMap *map, const ChunkCoord *key, size_t *index)
{
	size_t idx;
	if (index == NULL)
		index = &idx;
	*index = triple_hash(map, key);
	size_t start_index = *index;
	uint8_t off_count = 0;

	do {
		const RenderMapEntry *entry = &map->entry[map->current_buffer][*index];

		if ((entry->flags & (HashMapFlagOccupied | HashMapFlagDeleted)) == 0)
			break;
		if (!(entry->flags & HashMapFlagOccupied))
			goto next_loop;
		if (entry->flags & HashMapFlagDeleted)
			goto next_loop;

		if (off_count == entry->off_by) {
			if (chunkcoord_match(key, &entry->chunk.coord)) {
				return true;
			}
			if (!(entry->flags & HashMapFlagCarry))
				break;
		}

next_loop:
		*index = (*index + 1) & (map->table_size - 1);

		if (*index == start_index)
			break;
		off_count++;
		continue;
	} while (true);

	return false;
}

static inline bool rendermap_contains(const RenderMap *map, ChunkCoord *key)
{
	return rendermap_find(map, key, NULL);
}

size_t rendermap_next_buffer(const RenderMap *map)
{
	return (map->current_buffer + 1) % map->num_buffers;
}
bool rendermap_add_next_buffer(RenderMap *map, OGLPool *pool, Chunk *chunk)
{
	size_t start_count = map->count;
	size_t next_buffer = rendermap_next_buffer(map);
	map->count_next += map->count - start_count;

	uint32_t index;
	bool ret = rendermap_add_opt(map, pool, chunk, next_buffer, &index);
	if (ret)
		map->entry[next_buffer][index].flags |= HashMapFlagStaysNext;
	return ret;
}

void rendermap_advance_buffer(RenderMap *map, OGLPool *pool)
{
	RenderMap tmp = *map;

	for (size_t i = 0; i < map->table_size; i++) {
		RenderMapEntry *entry = &map->entry[map->current_buffer][i];
		if (entry->chunk.has_oglpool_reference != 0 && !(entry->flags & HashMapFlagStaysNext)) {
			oglpool_release(pool, entry->chunk.oglpool_index);
		}
	}

	memset(map, 0, sizeof(*map));
	map->entry = tmp.entry;
	map->table_size = tmp.table_size;
	map->num_buffers = tmp.num_buffers;
	map->current_buffer = tmp.current_buffer;
	map->count = tmp.count_next;
	memset(map->entry[map->current_buffer], 0, sizeof(map->entry[0][0]) * map->table_size);
	map->current_buffer = rendermap_next_buffer(map);
}

bool rendermap_remove(RenderMap *map, OGLPool *pool, ChunkCoord *key)
{
	size_t index;
	index = triple_hash(map, key);
	size_t start_index = index;
	uint8_t off_count = 0;
	size_t last_carry = 0;
	bool found_carry = false;

	do {
		RenderMapEntry *entry = &map->entry[map->current_buffer][index];

		if ((entry->flags & (HashMapFlagOccupied | HashMapFlagDeleted)) == 0)
			break;
		if (!(entry->flags & HashMapFlagOccupied))
			goto next_loop;
		if (entry->flags & HashMapFlagDeleted)
			goto next_loop;

		if (off_count == entry->off_by) {
			if (chunkcoord_match(key, &entry->chunk.coord)) {
				if (!(entry->flags & HashMapFlagCarry) && found_carry) {
					map->entry[map->current_buffer][last_carry].flags &= ~HashMapFlagCarry;
				}
				entry->flags |= HashMapFlagDeleted;
				if (entry->chunk.has_oglpool_reference)
					oglpool_release(pool, entry->chunk.oglpool_index);
				return true;
			}
			if (entry->flags & HashMapFlagCarry) {
				found_carry = true;
				last_carry = index;
			} else
				break;
		}

next_loop:
		index = (index + 1) & (map->table_size - 1);

		if (index == start_index)
			break;
		off_count++;
		continue;
	} while (true);

	return false;
}
