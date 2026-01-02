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
	VINFO("has_oglpool_reference: %s", chunk->has_oglpool_reference ? "true" : "false");
	// if (chunk->has_oglpool_reference)
	VINFO("oglpool_index: %i", chunk->oglpool_index);
}
void clear_chunk_verts_scratch(ChunkVertsScratch *tmp_chunk_verts)
{
	// memset(tmp_chunk_verts->data, 0, sizeof(float) * tmp_chunk_verts->fill);
	tmp_chunk_verts->lvl = 0;
}
void chunk_generate_terrain(Chunk *chunk, size_t seed)
{
	VASSERT(chunk->data != NULL);
	// VINFO("Generating terrain: %i %i %i", chunk->coord.x, chunk->coord.y, chunk->coord.z);

	// srand(seed);
	if (chunk->coord.y == 0)
		for (size_t y = 0; y < CHUNK_TOTAL_Y; y++) {
			for (size_t z = 0; z < CHUNK_TOTAL_Z; z++) {
				// size_t y_lim = 1 + rand() % 20;
				for (size_t x = 0; x < CHUNK_TOTAL_X; x++) {
					if (y < 6) {
						Block *block = &chunk->data[CHUNK_INDEX(x, y, z)];
						chunk->block_count++;
						block->type = BlocktypeGrass;
						block->obstructing = true;
						// block_make_light(block, (Color){ 255, 255, 255, 0 }, 15); // TODO:make proper light propagation and remove
					}
				}
			}
		}

	// VINFO("Generated:");
	// print_block(&chunk->data[0]);

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
