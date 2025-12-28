#ifndef INCLUDE_SRC_CHUNK_H_
#define INCLUDE_SRC_CHUNK_H_

#include "oglpool.h"
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
#define CHUNK_TOTAL_BLOCKS CHUNK_TOTAL_X * CHUNK_TOTAL_Y * CHUNK_TOTAL_Z
#define CHUNK_STRIDE_Y CHUNK_TOTAL_X
#define CHUNK_STRIDE_Z CHUNK_TOTAL_X * CHUNK_TOTAL_Y

#define CHUNK_INDEX(x, y, z) ((x) + (y) * CHUNK_STRIDE_Y + (z) * CHUNK_STRIDE_Z)
#define CHUNK_TOTAL_VERTICES (CHUNK_TOTAL_BLOCKS * FACES_PER_CUBE * FLOATS_PER_VERTEX * VERTICES_PER_FACE)
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
	float *data;
	size_t fill;
} ChunkVertsScratch;
typedef struct {
	Block *data;
	ChunkCoord coord;
	// unsigned int VAO;
	// unsigned int VBO;
	size_t oglpool_index;
	bool has_oglpool_reference;
	size_t vertex_count;
	bool contains_blocks;
	bool up_to_date;
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
void chunk_load(Chunk *chunk, const ChunkCoord *coord, size_t seed);
void chunk_generate_mesh(OGLPool *pool, Chunk *chunk, ChunkVertsScratch *tmp_chunk_verts);
void chunk_clear_metadata(Chunk *chunk);
void clear_chunk_verts_scratch(ChunkVertsScratch *tmp_chunk_verts);
ChunkCoord world_coord_to_chunk(const WorldCoord *world);
void world_cord_to_chunk_and_block(const WorldCoord *world, ChunkCoord *chunk, BlockPos *local);
Block *chunk_blockpos_at(const Chunk *chunk, const BlockPos *pos);
Block *chunk_xyz_at(const Chunk *chunk, int x, int y, int z);


// avoid circular
bool oglpool_claim_chunk(OGLPool *pool, Chunk *chunk);
void oglpool_release_chunk(OGLPool *pool, Chunk *chunk);
void oglpool_reference_chunk(OGLPool *pool, Chunk *chunk, size_t index);

#endif // INCLUDE_SRC_CHUNK_H_
