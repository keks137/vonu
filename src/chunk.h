#ifndef INCLUDE_SRC_CHUNK_H_
#define INCLUDE_SRC_CHUNK_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define BLOCKTYPE_NAMES   \
	X(BlocktypeAir)   \
	X(BlocktypeGrass) \
	X(BlocktypeStone)

typedef enum {
#define X(name) name,
	BLOCKTYPE_NAMES
#undef X
} BLOCKTYPE;

#define CHUNK_TOTAL_X 32
#define CHUNK_TOTAL_Y 32
#define CHUNK_TOTAL_Z 32
#define CHUNK_TOTAL_BLOCKS (CHUNK_TOTAL_X * CHUNK_TOTAL_Y * CHUNK_TOTAL_Z)
extern const char *BlockTypeString[];

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
	bool modified;
	bool has_vbo_data;
	uint64_t updates_this_cycle;
	uint64_t cycles_since_update;
	uint64_t unchanged_render_count;
	time_t last_used;
} Chunk;

void print_chunk(Chunk *chunk);
void chunk_free(Chunk *chunk);

#endif // INCLUDE_SRC_CHUNK_H_
