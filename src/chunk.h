#ifndef INCLUDE_SRC_CHUNK_H_
#define INCLUDE_SRC_CHUNK_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
typedef enum {
	BLOCKTYPE_AIR,
	BLOCKTYPE_GRASS,
	BLOCKTYPE_STONE,
} BLOCKTYPE;
typedef struct {
	BLOCKTYPE type;
	bool obstructing;
} Block;
typedef struct {
	uint8_t x;
	uint8_t y;
	uint8_t z;
} BlockPos;

typedef struct {
	int64_t x;
	int64_t y;
	int64_t z;
} WorldCoord;

typedef struct {
	int64_t x;
	int64_t y;
	int64_t z;
} ChunkCoord;

typedef struct {
	Block *data;
	ChunkCoord coord;
	unsigned int VAO;
	unsigned int VBO;
	size_t vertex_count;
	bool contains_blocks;
	bool up_to_date;
	bool initialized;
	bool terrain_generated;
	uint64_t unchanged_render_count;
	bool unloaded;
	time_t last_used;
} Chunk;


void print_chunk(Chunk *chunk);

#endif  // INCLUDE_SRC_CHUNK_H_
