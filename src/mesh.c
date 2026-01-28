#include "mesh.h"
#include "block.h"
#include "logs.h"
#include "profiling.h"
#include "chunk.h"
#include "vassert.h"
#include "oglpool.h"
#include "../vendor/cglm/cglm.h"
#include <string.h>
#include "mesh.h"
typedef enum {
	FACE_BACK = 0,
	FACE_FRONT,
	FACE_LEFT,
	FACE_RIGHT,
	FACE_BOTTOM,
	FACE_TOP
} CubeFace;
typedef struct {
	vec3 positions[VERTICES_PER_QUAD];
	vec2 tex_coords[VERTICES_PER_QUAD];
	uint8_t face_id;
} CubeFaceDef;
const CubeFaceDef cube_faces[FACES_PER_CUBE] = {
	// BACK (z = -0.5)
	{
		.positions = {
			{ 0.5f, 0.5f, -0.5f }, // Vertex 0 (index 0,5)
			{ 0.5f, -0.5f, -0.5f }, // Vertex 1 (index 1)
			{ -0.5f, -0.5f, -0.5f }, // Vertex 2 (index 2,3)
			{ -0.5f, 0.5f, -0.5f } // Vertex 4 (index 4)
		},
		.tex_coords = {
			{ 0.0f, 0.0f }, // From vertices 0,5
			{ 0.0f, 1.0f }, // From vertex 1
			{ 1.0f, 1.0f }, // From vertices 2,3
			{ 1.0f, 0.0f } // From vertex 4
		},
		.face_id = FACE_BACK },
	// FRONT face
	{ .positions = {
		  { -0.5f, -0.5f, 0.5f }, // Vertex 0
		  { 0.5f, -0.5f, 0.5f }, // Vertex 1
		  { 0.5f, 0.5f, 0.5f }, // Vertex 2,3
		  { -0.5f, 0.5f, 0.5f } // Vertex 4
	  },
	  .tex_coords = {
		  { 0.0f, 1.0f }, // From vertex 0
		  { 1.0f, 1.0f }, // From vertex 1
		  { 1.0f, 0.0f }, // From vertices 2,3
		  { 0.0f, 0.0f } // From vertex 4
	  },
	  .face_id = FACE_FRONT },
	// LEFT face
	{ .positions = {
		  { -0.5f, 0.5f, 0.5f }, // Vertex 0,5
		  { -0.5f, 0.5f, -0.5f }, // Vertex 1
		  { -0.5f, -0.5f, -0.5f }, // Vertex 2,3
		  { -0.5f, -0.5f, 0.5f } // Vertex 4
	  },
	  .tex_coords = {
		  { 1.0f, 0.0f }, // From vertices 0,5
		  { 0.0f, 0.0f }, // From vertex 1
		  { 0.0f, 1.0f }, // From vertices 2,3
		  { 1.0f, 1.0f } // From vertex 4
	  },
	  .face_id = FACE_LEFT },
	// RIGHT face
	{ .positions = {
		  { 0.5f, -0.5f, -0.5f }, // Vertex 0,5
		  { 0.5f, 0.5f, -0.5f }, // Vertex 1
		  { 0.5f, 0.5f, 0.5f }, // Vertex 2,3
		  { 0.5f, -0.5f, 0.5f } // Vertex 4
	  },
	  .tex_coords = {
		  { 1.0f, 1.0f }, // From vertices 0,5
		  { 1.0f, 0.0f }, // From vertex 1
		  { 0.0f, 0.0f }, // From vertices 2,3
		  { 0.0f, 1.0f } // From vertex 4
	  },
	  .face_id = FACE_RIGHT },
	// BOTTOM face
	{ .positions = {
		  { -0.5f, -0.5f, -0.5f }, // Vertex 0,5
		  { 0.5f, -0.5f, -0.5f }, // Vertex 1
		  { 0.5f, -0.5f, 0.5f }, // Vertex 2,3
		  { -0.5f, -0.5f, 0.5f } // Vertex 4
	  },
	  .tex_coords = {
		  { 1.0f, 0.0f }, // From vertices 0,5
		  { 0.0f, 0.0f }, // From vertex 1
		  { 0.0f, 1.0f }, // From vertices 2,3
		  { 1.0f, 1.0f } // From vertex 4
	  },
	  .face_id = FACE_BOTTOM },
	// TOP face
	{ .positions = {
		  { 0.5f, 0.5f, 0.5f }, // Vertex 0,5
		  { 0.5f, 0.5f, -0.5f }, // Vertex 1
		  { -0.5f, 0.5f, -0.5f }, // Vertex 2,3
		  { -0.5f, 0.5f, 0.5f } // Vertex 4
	  },
	  .tex_coords = {
		  { 1.0f, 1.0f }, // From vertices 0,5
		  { 1.0f, 0.0f }, // From vertex 1
		  { 0.0f, 0.0f }, // From vertices 2,3
		  { 0.0f, 1.0f } // From vertex 4
	  },
	  .face_id = FACE_TOP }
};
#define TEXTURE_ATLAS_SIZE 256.0f
#define TEXTURE_TILE_SIZE 16.0f

typedef struct {
	int tileX;
	int tileY;
} TextureCoords;

static void get_tile_uv(int tileX, int tileY, float local_u, float local_v, float *u, float *v)
{
	const float epsilon = 0.5f / TEXTURE_TILE_SIZE;

	float adjusted_u = local_u * (1.0f - 2 * epsilon) + epsilon;
	float adjusted_v = local_v * (1.0f - 2 * epsilon) + epsilon;

	float pixel_u = tileX * TEXTURE_TILE_SIZE + adjusted_u * TEXTURE_TILE_SIZE;
	float pixel_v = tileY * TEXTURE_TILE_SIZE + adjusted_v * TEXTURE_TILE_SIZE;

	*u = pixel_u / TEXTURE_ATLAS_SIZE;
	*v = pixel_v / TEXTURE_ATLAS_SIZE;
}

static void texture_get_uv_vertex(float local_u, float local_v, BLOCKTYPE type, CubeFace face, float *u, float *v)
{
	static const TextureCoords texture_map[256][FACES_PER_CUBE] = {
		[BlocktypeGrass] = {
			[FACE_BACK] = { 1, 0 },
			[FACE_FRONT] = { 1, 0 },
			[FACE_LEFT] = { 1, 0 },
			[FACE_RIGHT] = { 1, 0 },
			[FACE_BOTTOM] = { 2, 0 },
			[FACE_TOP] = { 0, 0 } },
		[BlocktypeDirt] = { [FACE_BACK] = { 2, 0 }, [FACE_FRONT] = { 2, 0 }, [FACE_LEFT] = { 2, 0 }, [FACE_RIGHT] = { 2, 0 }, [FACE_BOTTOM] = { 2, 0 }, [FACE_TOP] = { 2, 0 } },
		[BlocktypeStone] = { [FACE_BACK] = { 3, 0 }, [FACE_FRONT] = { 3, 0 }, [FACE_LEFT] = { 3, 0 }, [FACE_RIGHT] = { 3, 0 }, [FACE_BOTTOM] = { 3, 0 }, [FACE_TOP] = { 3, 0 } },
	};

	TextureCoords coords = texture_map[type][face];
	get_tile_uv(coords.tileX, coords.tileY, local_u, local_v, u, v);
}
typedef struct {
	Vertex vertices[VERTICES_PER_QUAD];
} FaceVertices;
static inline void const_vec3_copy(const vec3 a, vec3 dest)
{
	dest[0] = a[0];
	dest[1] = a[1];
	dest[2] = a[2];
}
static void mesh_add_face(ChunkVertsScratch *buffer, const BlockPos *pos, BLOCKTYPE type, CubeFace face, BlockLight light)
{
	// FaceVertices face_verts = cube_vertices[face];
	const CubeFaceDef *face_def = &cube_faces[face];

	VASSERT_MSG(buffer->lvl + sizeof(FaceVertices) / sizeof(Vertex) <= buffer->cap, "Vertex buffer overflow!");
	for (size_t i = 0; i < VERTICES_PER_QUAD; i++) {
		Vertex vertex = { 0 };

		const_vec3_copy(face_def->positions[i], vertex.pos);
		// vertex.pos[0] +=  (float)x;
		// vertex.pos[1] +=  (float)y;
		// vertex.pos[2] +=  (float)z;
		vertex.pos[0] += 0.5f + (float)pos->x;
		vertex.pos[1] += 0.5f + (float)pos->y;
		vertex.pos[2] += 0.5f + (float)pos->z;

		texture_get_uv_vertex(face_def->tex_coords[i][0],
				      face_def->tex_coords[i][1],
				      type, face, &vertex.tex[0], &vertex.tex[1]);

		vertex.norm |= face & 0x07;
		vertex.light = light;
		buffer->data[buffer->lvl++] = vertex;
	}
}
static inline Block chunk_blockpos_at(const Chunk *chunk, const BlockPos *pos)
{
	return chunk->data[CHUNK_INDEX(pos->x, pos->y, pos->z)];
}
static inline bool blockpos_within_chunk(BlockPos *block_pos)
{
	return block_pos->x >= 0 && block_pos->x < CHUNK_TOTAL_X && block_pos->z >= 0 && block_pos->z < CHUNK_TOTAL_Z && block_pos->y >= 0 && block_pos->y < CHUNK_TOTAL_Y;
}
static inline Block meshqueue_block_at(MeshQueue *mesh, BlockPos *pos)
{
	int64_t cx = 0;
	int64_t cy = 0;
	int64_t cz = 0;

	if (pos->x < 0) {
		cx = -1;
	} else if (pos->x >= CHUNK_TOTAL_X) {
		cx = 1;
	}

	if (pos->y < 0) {
		cy = -1;
	} else if (pos->y >= CHUNK_TOTAL_Y) {
		cy = 1;
	}

	if (pos->z < 0) {
		cz = -1;
	} else if (pos->z >= CHUNK_TOTAL_Z) {
		cz = 1;
	}

	size_t chunk_index = (cy + 1) * MESH_INVOLVED_STRIDE_Y + (cz + 1) * MESH_INVOLVED_STRIDE_Z + (cx + 1);

	int64_t lx = pos->x - cx * CHUNK_TOTAL_X;
	int64_t ly = pos->y - cy * CHUNK_TOTAL_Y;
	int64_t lz = pos->z - cz * CHUNK_TOTAL_Z;

	VASSERT((lx >= 0 && lx < CHUNK_TOTAL_X &&
		 ly >= 0 && ly < CHUNK_TOTAL_Y &&
		 lz >= 0 && lz < CHUNK_TOTAL_Z));

	size_t block_index_in_chunk = (ly * CHUNK_TOTAL_X + lz) * CHUNK_TOTAL_X + lx;

	size_t involved_index = chunk_index * CHUNK_TOTAL_BLOCKS + block_index_in_chunk;
	return mesh->blockdata[involved_index];
}
static void mesh_add_block(MeshQueue *mesh, const BlockPos *pos, const Chunk *chunk, ChunkVertsScratch *scratch)
{
	static const vec3 face_offsets[FACES_PER_CUBE] = {
		{ 0, 0, -1 },
		{ 0, 0, 1 },
		{ -1, 0, 0 },
		{ 1, 0, 0 },
		{ 0, -1, 0 },
		{ 0, 1, 0 }
	};

	Block block = chunk_blockpos_at(chunk, pos);
	BlockLight light = block.light;
	if (!block.obstructing)
		return;

	for (CubeFace face = FACE_BACK; face <= FACE_TOP; face++) {
		BlockPos neighbour_pos = { 0 };
		neighbour_pos.x = pos->x + face_offsets[face][0];
		neighbour_pos.y = pos->y + face_offsets[face][1];
		neighbour_pos.z = pos->z + face_offsets[face][2];

		Block neighbor = { 0 };
		if (blockpos_within_chunk(&neighbour_pos))
			neighbor = chunk_blockpos_at(chunk, &neighbour_pos);
		else {
			// TODO: rethink this, it's broken
			neighbor = meshqueue_block_at(mesh, &neighbour_pos);
			// if (neighbor.obstructing) {
			// 	print_chunkcoord(&chunk->coord);
			// 	print_blockpos(&neighbour_pos);
			// 	print_block(&neighbor);
			// }
		}
		if (!neighbor.obstructing) {
			mesh_add_face(scratch, pos, block.type, face, light);
		}
	}
}

static void meshworker_chunk_load(ChunkPool *pool, Chunk *chunk, const ChunkCoord *coord, size_t seed)
{
	// TODO: cache
	// TODO: pull latest from pool
	// chunk->last_used = time(NULL);
	chunk->coord = *coord;
	VASSERT(chunk->data != NULL);
	size_t index;
	if (pool_get_index(pool, chunk, &index)) {
		memcpy(chunk->data, pool->chunk[index].data, sizeof(chunk->data[0]) * CHUNK_TOTAL_BLOCKS);
		chunk->block_count = pool->chunk[index].block_count;
		// VINFO("count: %zu", chunk->block_count);
		return;
	}
	chunk_generate_terrain(chunk, seed);
}

static void mesh_compute(MeshQueue *mesh, ChunkPool *pool, ChunkVertsScratch *scratch, const ChunkCoord *coord, size_t seed)
{
	Chunk *wanted = &mesh->involved[MESH_INVOLVED_CENTRE];
	VASSERT(wanted->block_count > 0);
	size_t i = 0;
	for (int64_t y = coord->y - 1; y <= coord->y + 1; y++)
		for (int64_t z = coord->z - 1; z <= coord->z + 1; z++)
			for (int64_t x = coord->x - 1; x <= coord->x + 1; x++) {
				if (i == MESH_INVOLVED_CENTRE) { // already loaded
					i++;
					continue;
				}
				memset(mesh->involved[i].data, 0, sizeof(Block) * CHUNK_TOTAL_BLOCKS);
				chunk_clear_metadata(&mesh->involved[i]);
				meshworker_chunk_load(pool, &mesh->involved[i], coord, seed);
				i++;
			}

	for (size_t y = 0; y < CHUNK_TOTAL_Y; y++) {
		for (size_t z = 0; z < CHUNK_TOTAL_Z; z++) {
			for (size_t x = 0; x < CHUNK_TOTAL_X; x++) {
				Block *block = &wanted->data[CHUNK_INDEX(x, y, z)];
				// chunk->data[CHUNK_INDEX(x, y, z)];
				if (!block->obstructing)
					continue;
				BlockPos pos = { .x = x, .y = y, .z = z };

				mesh_add_block(mesh, &pos, wanted, scratch);
			}
		}
	}
}
 void meshworker_process_mesh(Worker *worker, MeshResourcePool *pool, MeshResourceHandle *res, WorkerTask *task)
{
	BEGIN_FUNC();
	// VINFO("%i %i %i", task->coord.x, task->coord.y, task->coord.z);
	MeshUploadData *upd = &pool->upload_data[res->up];
	clear_chunk_verts_scratch(&upd->buf);
	MeshQueue mesh = { 0 };
	mesh.involved = &pool->involved[res->user * MESH_CHUNKS_INVOLVED];
	mesh.blockdata = &pool->blockdata[res->user * MESH_CHUNKS_INVOLVED * CHUNK_TOTAL_BLOCKS];
	// VINFO("involved: %p", pool->involved);

	Chunk *wanted = &mesh.involved[MESH_INVOLVED_CENTRE];
	// VINFO("wanted: %p", wanted);
	// VINFO("data: %p", wanted->data);
	memset(wanted->data, 0, sizeof(Block) * CHUNK_TOTAL_BLOCKS);
	chunk_clear_metadata(wanted);
	meshworker_chunk_load(worker->context.pool, wanted, &task->coord, worker->context.seed);
	wanted->up_to_date = true;
	upd->coord = wanted->coord;
	upd->block_count = wanted->block_count;
	if (wanted->block_count > 0) {
		mesh_compute(&mesh, worker->context.pool, &upd->buf, &task->coord, worker->context.seed);
		if (upd->buf.lvl == 0)
			VWARN("huh? bc: %zu x: %i y: %i z: %i", wanted->block_count, task->coord.x, task->coord.y, task->coord.z);
	}
	pool->upload_status[res->up] = UPLOAD_STATUS_READY;
	END_FUNC();
}
