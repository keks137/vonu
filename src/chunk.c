#include "chunk.h"
#include "block.h"
#include "logs.h"
#include "vassert.h"
#include <string.h>
Chunk chunk_callocrash()
{
	Chunk chunk = { 0 };
	chunk.data = calloc(CHUNK_TOTAL_BLOCKS, sizeof(Block));
	VASSERT_RELEASE_MSG(chunk.data != NULL, "Buy more ram for more chunks bozo");

	// chunk_init(&chunk);

	return chunk;
}

BlockPos chunk_index_to_blockpos(size_t index)
{
	BlockPos pos = { 0 };
	pos.x = index % CHUNK_TOTAL_X;
	index /= CHUNK_TOTAL_X;
	pos.z = index % CHUNK_TOTAL_Z;
	pos.y = index / CHUNK_TOTAL_Z;
	return pos;
}

ChunkCoord world_coord_to_chunk(const WorldCoord *world)
{
	ChunkCoord chunk;

	chunk.x = (world->x >= 0) ? world->x / CHUNK_TOTAL_X : (world->x + 1) / CHUNK_TOTAL_X - 1;
	chunk.y = (world->y >= 0) ? world->y / CHUNK_TOTAL_Y : (world->y + 1) / CHUNK_TOTAL_Y - 1;
	chunk.z = (world->z >= 0) ? world->z / CHUNK_TOTAL_Z : (world->z + 1) / CHUNK_TOTAL_Z - 1;

	return chunk;
}

void world_cord_to_chunk_and_block(const WorldCoord *world, ChunkCoord *chunk, BlockPos *local)
{
	*chunk = world_coord_to_chunk(world);

	local->x = ((world->x % CHUNK_TOTAL_X) + CHUNK_TOTAL_X) % CHUNK_TOTAL_X;
	local->y = ((world->y % CHUNK_TOTAL_Y) + CHUNK_TOTAL_Y) % CHUNK_TOTAL_Y;
	local->z = ((world->z % CHUNK_TOTAL_Z) + CHUNK_TOTAL_Z) % CHUNK_TOTAL_Z;
}
void blockpos_to_world_coord(const BlockPos *local, const ChunkCoord *chunk, WorldCoord *world)
{
	world->x = chunk->x * CHUNK_TOTAL_X + local->x;
	world->y = chunk->y * CHUNK_TOTAL_Y + local->y;
	world->z = chunk->z * CHUNK_TOTAL_Z + local->z;
}
void print_chunk(Chunk *chunk)
{
	VINFO("Chunk: %p", chunk);
	VINFO("Data: %p", chunk->data);
	VINFO("coords: %i %i %i", chunk->coord.x, chunk->coord.y, chunk->coord.z);
	VINFO("unchanged_render_count: %i", chunk->unchanged_render_count);
	VINFO("up_to_date: %s", chunk->up_to_date ? "true" : "false");
	VINFO("terrain_generated: %s", chunk->terrain_generated ? "true" : "false");
	VINFO("block_count: %i", chunk->block_count);
	VINFO("face_count: %i", chunk->face_count);
	VINFO("cycles_since_update: %i", chunk->cycles_since_update);
	// if (chunk->has_oglpool_reference)
	VINFO("oglpool_index: %i", chunk->oglpool_index);
}
void clear_chunk_verts_scratch(ChunkVertsScratch *tmp_chunk_verts)
{
	// memset(tmp_chunk_verts->data, 0, sizeof(float) * tmp_chunk_verts->fill);
	tmp_chunk_verts->lvl = 0;
}

static uint32_t hash32(uint32_t x)
{
	x = (x ^ 61) ^ (x >> 16);
	x = x + (x << 3);
	x = x ^ (x >> 4);
	x = x * 0x27d4eb2d;
	x = x ^ (x >> 15);
	return x;
}

static float noise2d(int x, int z, size_t seed)
{
	uint32_t n = hash32(seed ^ hash32(x) ^ hash32(z));
	return (float)(n & 0xFFFF) / 65535.0f;
}

static float fbm2d(float x, float z, size_t seed, int octaves, float persistence)
{
	float total = 0.0f;
	float frequency = 1.0f;
	float amplitude = 1.0f;
	float max_value = 0.0f;

	for (int i = 0; i < octaves; i++) {
		int xi = (int)(x * frequency);
		int zi = (int)(z * frequency);
		total += noise2d(xi, zi, seed + i) * amplitude;
		max_value += amplitude;
		amplitude *= persistence;
		frequency *= 2.0f;
	}

	return total / max_value;
}

void chunk_generate_terrain(Chunk *chunk, size_t seed)
{
	VASSERT(chunk->data != NULL);
	chunk->block_count = 0;
	// VINFO("Generating terrain: %i %i %i", chunk->coord.x, chunk->coord.y, chunk->coord.z);

	int64_t world_x = chunk->coord.x * CHUNK_TOTAL_X;
	int64_t world_y = chunk->coord.y * CHUNK_TOTAL_Y;
	int64_t world_z = chunk->coord.z * CHUNK_TOTAL_Z;
	const int base_height = 4;
	const int height_range = 50;
	const float noise_scale = 0.05f;
	const int bedrock_depth = 3;

	uint8_t heightmap[CHUNK_TOTAL_X][CHUNK_TOTAL_Z];

	for (size_t z = 0; z < CHUNK_TOTAL_Z; z++) {
		for (size_t x = 0; x < CHUNK_TOTAL_X; x++) {
			float wx = (world_x + x) * noise_scale;
			float wz = (world_z + z) * noise_scale;

			float noise = fbm2d(wx, wz, seed, 4, 0.5f);

			float ridge_noise = fbm2d(wx * 1.5f, wz * 1.5f, seed + 12345, 2, 0.7f);
			noise = noise * 0.7f + fabs(ridge_noise) * 0.3f;

			heightmap[x][z] = base_height + (int)(noise * height_range);
		}
	}

	for (size_t z = 0; z < CHUNK_TOTAL_Z; z++) {
		for (size_t x = 0; x < CHUNK_TOTAL_X; x++) {
			int surface_height = heightmap[x][z];

			for (size_t y = 0; y < CHUNK_TOTAL_Y; y++) {
				int world_block_y = world_y + y;
				Block *block = &chunk->data[CHUNK_INDEX(x, y, z)];

				if (world_block_y <= surface_height) {
					if (world_block_y == surface_height) {
						block->type = BlocktypeGrass;
						block->obstructing = true;
						chunk->block_count++;
					} else if (world_block_y > surface_height - 4) {
						block->type = BlocktypeDirt;
						block->obstructing = true;
						chunk->block_count++;
					} else if (world_block_y < bedrock_depth) {
						block->type = BlocktypeStone;
					} else {
						block->type = BlocktypeStone;
						block->obstructing = true;
						chunk->block_count++;
					}
				}
			}
		}
	}

	// // 3. Optional: Add caves/overhangs
	// if (world_y < cave_start) {
	// 	for (size_t y = 0; y < CHUNK_TOTAL_Y; y++) {
	// 		int world_block_y = world_y + y;
	// 		if (world_block_y < surface_height - 5) {
	// 			for (size_t z = 0; z < CHUNK_TOTAL_Z; z++) {
	// 				for (size_t x = 0; x < CHUNK_TOTAL_X; x++) {
	// 					// Use 3D noise for caves
	// 					float wx = (world_x + x) * 0.1f;
	// 					float wy = world_block_y * 0.1f;
	// 					float wz = (world_z + z) * 0.1f;
	//
	// 					// Simple 3D cave noise
	// 					float cave_noise =
	// 						noise2d((int)wx, (int)wz, seed + 999) * 0.5f +
	// 						noise2d((int)(wy * 2), (int)wz, seed + 1000) * 0.3f +
	// 						noise2d((int)wx, (int)(wy * 2), seed + 1001) * 0.2f;
	//
	// 					// Create caves where noise is high
	// 					if (cave_noise > 0.6f) {
	// 						Block *block = &chunk->data[CHUNK_INDEX(x, y, z)];
	// 						if (block->type != BlocktypeAir) {
	// 							block->type = BlocktypeAir;
	// 							block->obstructing = false;
	// 							chunk->block_count--;
	// 						}
	// 					}
	// 				}
	// 			}
	// 		}
	// 	}
	// }
	//
	chunk->terrain_generated = true;
}

void chunk_clear_metadata(Chunk *chunk)
{
	VASSERT(chunk != NULL);
	Chunk tmp = { 0 };
	tmp.data = chunk->data;

	// NOTE: not sure if oglpool might lead to issues here
	memset(chunk, 0, sizeof(*chunk));
	chunk->data = tmp.data;
}
void chunk_load(Chunk *chunk, const ChunkCoord *coord, size_t seed)
{
	chunk->last_used = time(NULL);
	chunk->coord = *coord;
	VASSERT(chunk->data != NULL);
	memset(chunk->data, 0, sizeof(Block) * CHUNK_TOTAL_BLOCKS);
	chunk_generate_terrain(chunk, seed);
}
