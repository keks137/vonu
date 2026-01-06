#ifndef INCLUDE_SRC_CHUNK_H_
#define INCLUDE_SRC_CHUNK_H_

#include "oglpool.h"
#include "block.h"
#include <stdbool.h>
#include "../vendor/cglm/cglm.h"
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define CHUNK_TOTAL_X 32
#define CHUNK_TOTAL_Y 32
#define CHUNK_TOTAL_Z 32
#define CHUNK_TOTAL_BLOCKS CHUNK_TOTAL_X * CHUNK_TOTAL_Y * CHUNK_TOTAL_Z
#define CHUNK_STRIDE_Z CHUNK_TOTAL_X
#define CHUNK_STRIDE_Y CHUNK_TOTAL_X *CHUNK_TOTAL_Z

#define CHUNK_INDEX(x, y, z) ((x) + (y) * CHUNK_STRIDE_Y + (z) * CHUNK_STRIDE_Z)
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
	vec3 pos;
	vec2 tex;
	uint32_t norm;
	BlockLight light;
} Vertex;

typedef struct {
	Vertex *data;
	size_t lvl;
	size_t cap;
} ChunkVertsScratch;

// typedef enum {
// 	CHUNK_FACE_BACK = 1 << 0,
// 	CHUNK_FACE_FRONT = 1 << 1,
// 	CHUNK_FACE_LEFT = 1 << 2,
// 	CHUNK_FACE_RIGHT = 1 << 3,
// 	CHUNK_FACE_BOTTOM = 1 << 4,
// 	CHUNK_FACE_TOP = 1 << 5,
// } ChunkFace;

typedef struct {
	Block *data;
	size_t *lights;
	size_t light_count;
	ChunkCoord coord;
	size_t oglpool_index;
	bool has_oglpool_reference;
	size_t face_count;
	size_t block_count;
	bool up_to_date;
	bool terrain_generated;
	bool modified;
	bool has_vbo_data;
	uint16_t num_lights;
	uint16_t *lighti;
	uint64_t updates_this_cycle;
	uint64_t cycles_since_update;
	uint64_t cycles_in_pool;
	uint64_t unchanged_render_count;
	time_t last_used;
} Chunk;

void print_chunk(Chunk *chunk);
void chunk_free(Chunk *chunk);
void chunk_load(Chunk *chunk, const ChunkCoord *coord, size_t seed);
void chunk_clear_metadata(Chunk *chunk);
void clear_chunk_verts_scratch(ChunkVertsScratch *tmp_chunk_verts);
ChunkCoord world_coord_to_chunk(const WorldCoord *world);
void world_cord_to_chunk_and_block(const WorldCoord *world, ChunkCoord *chunk, BlockPos *local);
void blockpos_to_world_coord(const BlockPos *local, const ChunkCoord *chunk, WorldCoord *world);
// Block *chunk_blockpos_at(const Chunk *chunk, const BlockPos *pos);
// Block *chunk_xyz_at(const Chunk *chunk, int x, int y, int z);
void chunk_generate_terrain(Chunk *chunk, size_t seed);

// avoid circular
bool oglpool_claim_chunk(OGLPool *pool, Chunk *chunk);
void oglpool_release_chunk(OGLPool *pool, Chunk *chunk);
void oglpool_reference_chunk(OGLPool *pool, Chunk *chunk, size_t index);

#endif // INCLUDE_SRC_CHUNK_H_
