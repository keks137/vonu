#include "game.h"
#include "core.h"
#include "types.h"
#include "logs.h"
#include "mesh.h"
#include "player.h"
#include "profiling.h"
#include "vassert.h"
#include <stddef.h>

static void mesh_upload_chunk(OGLPool *ogl, ChunkVertsScratch *scratch, Chunk *chunk)
{
	chunk->face_count = scratch->lvl / VERTICES_PER_QUAD;
	VASSERT_WARN(chunk->face_count > 0);
	if (chunk->face_count == 0) {
		return;
	}
	if (chunk->oglpool_index == 0) {
		if (!oglpool_claim_chunk(ogl, chunk))
			return;
	}
	VASSERT(chunk->oglpool_index != 0);
	OGLItem *item = &ogl->items[chunk->oglpool_index];
	// VASSERT_MSG(item->references > 0, "How did it get here?");
	glBindVertexArray(item->VAO);

	glBindBuffer(GL_ARRAY_BUFFER, item->VBO);
	glBufferData(GL_ARRAY_BUFFER, scratch->lvl * sizeof(scratch->data[0]), scratch->data, chunk->updates_this_cycle > 2 ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
	chunk->has_vbo_data = true;
	GL_CHECK((void)0);
	// print_chunkcoord(&chunk->coord);
}
static inline void meshqueue_process_element(MeshUploadData *upd, RenderMap *map, OGLPool *ogl)
{
	Chunk chunk = { 0 };
	chunk.up_to_date = true;
	chunk.block_count = upd->block_count;
	// print_chunk(&chunk);

	chunk.coord = upd->coord;
	if (upd->buf.lvl > 0) {
		mesh_upload_chunk(ogl, &upd->buf, &chunk);
		rendermap_add(map, ogl, &chunk);
	} else // TODO: empty map
		rendermap_add(map, ogl, &chunk);

	if (chunk.oglpool_index != 0) {
		RenderMapChunk rmchunk = rendermapchunk_from_chunk(&chunk);
		oglpool_release_chunk(ogl, &rmchunk);
	}
}
void meshqueue_process(size_t max, MeshResourcePool *meshp, RenderMap *map, OGLPool *ogl)
{
	BEGIN_FUNC();
	size_t num_proc = 0;
	for (size_t i = 0; i < meshp->max_uploads && num_proc < max; i++) {
		if (meshp->upload_status[i] == UPLOAD_STATUS_READY) {
			MeshUploadData *upd = &meshp->upload_data[i];
			// VINFO("i: %zu bc: %zu x: %i y: %i z: %i", i, upd->block_count, upd->coord.x, upd->coord.y, upd->coord.z);
			meshqueue_process_element(upd, map, ogl);
			// VINFO("uploaded");
			meshp->upload_status[i] = UPLOAD_STATUS_FREE;
			num_proc++;
		}
		// VINFO("num_proc: %zu",num_proc);
	}
	END_FUNC();
}
static void workers_add_new(ThreadPool *thread, const ChunkCoord coord)
{
	workerqueue_push_override(&thread->system.mesh_new, (WorkerTask){ coord, NULL, 0 });
}
static void workers_add_update(ThreadPool *thread, const ChunkCoord coord)
{
	// VINFO("%i", thread->system.mesh_up.writepos - thread->system.mesh_up.readpos);
	workerqueue_push_override(&thread->system.mesh_up, (WorkerTask){ coord, NULL, 0 });
}
static void chunk_render(OGLPool *pool, Chunk *chunk, ShaderData *shader_data)
{
#if 0
	if (!chunk->up_to_date || !chunk->has_oglpool_reference)
		return;
#else
	if (chunk->oglpool_index == 0)
		return;
#endif
	if (chunk->face_count == 0) {
		// VERROR("This guy again:");
		return;
	}
	// VINFO("drawing");

	BEGIN_FUNC();
	VASSERT_MSG(chunk->block_count != 0, "How did it get here?");
	VASSERT_MSG(pool->items[chunk->oglpool_index].references > 0, "How did it get here?");
	VASSERT_MSG(chunk->face_count > 0, "How did it get here?");

	// chunk->unchanged_render_count++;

	GL_CHECK(glUseProgram(shader_data->program));
	GL_CHECK(glBindVertexArray(pool->items[chunk->oglpool_index].VAO));

	mat4 model = { 0 };
	glm_mat4_identity(model);
	vec3 pos = { 0 };
	pos[0] = chunk->coord.x * CHUNK_TOTAL_X;
	pos[1] = chunk->coord.y * CHUNK_TOTAL_Y;
	pos[2] = chunk->coord.z * CHUNK_TOTAL_Z;
	GL_CHECK(glm_translate(model, pos));

	GL_CHECK(glUniformMatrix4fv(shader_data->model_loc, 1, GL_FALSE, (const float *)model));

	size_t index_count = chunk->face_count * INDICES_PER_QUAD;

	GL_CHECK(glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_SHORT, 0));
	// GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, chunk->vertex_count));
	END_FUNC();
}
static void world_render(World *world, ShaderData *shader_data)
{
	BEGIN_FUNC();
	// time_t current_time = time(NULL);
	// if ((world->mesh.writei - world->mesh.readi) != 0)
	// 	print_meshqueue(&world->mesh);
	// print_pool(&world->pool);
	meshqueue_process(1024, &world->thread.mesh_resource, &world->render_map, &world->ogl_pool);

	for (int y = -RENDER_DISTANCE_Y; y <= RENDER_DISTANCE_Y; y++) {
		for (int z = -RENDER_DISTANCE_Z; z <= RENDER_DISTANCE_Z; z++) {
			for (int x = -RENDER_DISTANCE_X; x <= RENDER_DISTANCE_X; x++) {
				// BEGIN_SECT("render one chunk");
				ChunkCoord coord = {
					world->player.chunk_pos.x + x,
					world->player.chunk_pos.y + y,
					world->player.chunk_pos.z + z
				};
				Chunk chunk = { 0 };
				if (rendermap_get_chunk(&world->render_map, &chunk, &coord)) {
					// VINFO("hi");
					chunk_render(&world->ogl_pool, &chunk, shader_data);
				}
				// END_SECT("render one chunk");
			}
		}
	}

	// ChunkCoord player_chunk = world_coord_to_chunk(world->player.pos);
	// VINFO("Player coords: %i %i %i", player_chunk.x, player_chunk.y, player_chunk.z);
	END_FUNC();
}
static void render(GameState *game_state, WindowData *window, ShaderData *shader_data)
{
	// BEGIN_FUNC();
	BEGIN_SECT("Buffer Swap");
	platform_swap_buffers(window);
	END_SECT("Buffer Swap");
	BEGIN_SECT("Other Stuff");
	mat4 view;
	vec3 camera_pos_plus_front;
	Camera *camera = &game_state->world.player.camera;

	glm_vec3_add(camera->pos, camera->front, camera_pos_plus_front);
	glm_lookat(camera->pos, camera_pos_plus_front, camera->up, view);

	mat4 projection = { 0 };
	float aspect = (float)window->width / (float)window->height;
	glm_perspective(glm_rad(camera->fov), aspect, 0.1f, 500.0f, projection);

	glClearColor(0.39f, 0.58f, 0.92f, 1.0f);
	GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

	glUseProgram(shader_data->program);

	glUniformMatrix4fv(shader_data->view_loc, 1, GL_FALSE, (const float *)view);
	glUniformMatrix4fv(shader_data->projection_loc, 1, GL_FALSE, (const float *)projection);

	END_SECT("Other Stuff");
	world_render(&game_state->world, shader_data);

	GL_CHECK((void)0);
	// END_FUNC();
}
static void mesh_request(RenderMap *map, ThreadPool *thread, const ChunkCoord *coord)
{
	BEGIN_FUNC();
	// print_chunkcoord(coord);
	Chunk chunk = { 0 };
	// TODO: make this scream into the void again
	if (rendermap_get_chunk(map, &chunk, coord)) {
		// print_chunk(&chunk);
		if (chunk.up_to_date) {
			END_FUNC();
			return;
		}
		workers_add_update(thread, *coord);
		END_FUNC();

		return;
	}
	workers_add_new(thread, *coord);
	END_FUNC();
}
bool next_chunk(ChunkIterator *it)
{
	if (++it->coord.x > it->x_max) {
		it->coord.x = it->x_min;
		if (++it->coord.y > it->y_max) {
			it->coord.y = it->y_min;
			if (++it->coord.z > it->z_max) {
				return false;
			}
		}
	}
	return true;
}
static void rendermap_keep_only_in_range(RenderMap *map, OGLPool *pool)
{
	ChunkIterator it = {
		.coord.x = -RENDER_DISTANCE_X,
		.x_min = -RENDER_DISTANCE_X,
		.x_max = RENDER_DISTANCE_X,
		.coord.y = -RENDER_DISTANCE_Y,
		.y_min = -RENDER_DISTANCE_Y,
		.y_max = RENDER_DISTANCE_Y,
		.coord.z = -RENDER_DISTANCE_Z,
		.z_min = -RENDER_DISTANCE_Z,
		.z_max = RENDER_DISTANCE_Z
	};

	do {
		Chunk chunk = { 0 };
		if (rendermap_get_chunk(map, &chunk, &(ChunkCoord){ it.coord.x, it.coord.y, it.coord.z })) {
			rendermap_add_next_buffer(map, pool, &chunk);
		}
	} while (next_chunk(&it));
	rendermap_advance_buffer(map, pool);
}
static void rendermap_clean_maybe(RenderMap *map, OGLPool *pool)
{
	BEGIN_FUNC();
	// if (map->count > 1.5f * MAX_VISIBLE_CHUNKS) {
	// 	rendermap_keep_only_in_range(map, pool);
	// }
	if (map->count > 0.75f * map->table_size) {
		rendermap_keep_only_in_range(map, pool);
	}
	END_FUNC();
}
static void world_update(World *world)
{
	BEGIN_FUNC();
	rendermap_clean_maybe(&world->render_map, &world->ogl_pool);
	// VINFO("c: %i", world->render_map.count);
	// VINFO("d: %i", world->render_map.deleted_count);

	BEGIN_SECT("chunk pool");
	for (size_t i = 0; i < world->pool.lvl; i++) {
		Chunk *chunk = &world->pool.chunk[i];
		// if (world->pool.chunk[i].up_to_date) {
		if (chunk->updates_this_cycle > 0) {
			// VINFO("bye");
			// TODO: also outdate appropriate neighbours
			rendermap_outdate(&world->render_map, &chunk->coord);
			chunk->cycles_since_update++;
			chunk->updates_this_cycle = 0;
			chunk->cycles_in_pool = 0;
		}

		if (chunk->cycles_in_pool > 4000) {
			if (chunk->modified) {
				//TODO: add back
				// disk_save(chunk, world->uid);
			}
			pool_empty(&world->pool, &world->ogl_pool, i);
		}
		chunk->cycles_in_pool++;
	}
	END_SECT("chunk pool");

	BEGIN_SECT("render req");
	for (int y = RENDER_DISTANCE_Y; y > -RENDER_DISTANCE_Y; y--) {
		for (int z = -RENDER_DISTANCE_Z; z <= RENDER_DISTANCE_Z; z++) {
			for (int x = -RENDER_DISTANCE_X; x <= RENDER_DISTANCE_X; x++) {
				ChunkCoord coord = {
					world->player.chunk_pos.x + x,
					world->player.chunk_pos.y + y,
					world->player.chunk_pos.z + z
				};

				mesh_request(&world->render_map, &world->thread, &coord);
			}
		}
	}
	END_SECT("render req");
	END_FUNC();
}
Block blocktype_to_block(BLOCKTYPE type)
{
	Block block = { 0 };
	switch (type) {
	case BlocktypeGrass:
		block.obstructing = true;
		block.type = BlocktypeGrass;
		block_make_light(&block, (Color){ rand() % 255, rand() % 255, rand() % 255, 1 });
		break;
	case BlocktypeStone:
		block.obstructing = true;
		block.type = BlocktypeStone;
		block_make_light(&block, (Color){ rand() % 255, rand() % 255, rand() % 255, 1 });
		break;
	default:
		break;
	}
	return block;
}

static void player_place_block(Player *player, BLOCKTYPE blocktype, ChunkPool *pool, OGLPool *ogl, RenderMap *map, const WorldCoord *pos, size_t seed)
{
	(void)player;
	// TODO: check if player is actually allowed to
	Block block = blocktype_to_block(blocktype);

	pool_replace_block(pool, ogl, map, pos, block, seed);
	// pool_replace_block(pool, ogl, map, &(WorldCoord){ 0, -10, 0 }, block, seed);
	// player_print_block(player, ogl, map, pool, seed);
}

static void player_update(Player *player)
{
	player->pos.x = floorf(player->camera.pos[0]);
	player->pos.y = floorf(player->camera.pos[1]) - 1;
	player->pos.z = floorf(player->camera.pos[2]);
	ChunkCoord new_chunk = world_coord_to_chunk(&player->pos);
	player->chunk_pos = new_chunk;
}

static void player_break_block(ChunkPool *pool, OGLPool *ogl, RenderMap *map, const WorldCoord *pos, size_t seed)
{
	// TODO: check if player is actually allowed to
	Block block = { 0 };
	pool_replace_block(pool, ogl, map, pos, block, seed);
}

void game_frame(WindowData *window, ShaderData *shader) // TODO: make shaders and assets hot reloadable
{
	f64 now = time_now();
	// game_state.delta_time = now - game_state.last_input;
	// VINFO("delta: %f", game_state.delta_time);
	game_state.acc_input += game_state.delta_time;
	World *world = &game_state.world;
	Player *player = &game_state.world.player;

	// player->movement_speed = 40.0f;
	// window->freq = 1.0 / 60.0;
	// window->freq = 1.0 / 1000;
	// window->freq = 0;

	// INPUTS:
	game_state.acc_input += now - game_state.last_frame;
	if (game_state.acc_input > window->freq * 2) {
		game_state.acc_input = INPUT_FREQ;
	}
	// VINFO("acc: %f", game_state.acc_input);

	size_t n_ins = 0;
	if (game_state.acc_input >= INPUT_FREQ) {
		while (game_state.acc_input >= INPUT_FREQ) {
			game_state.delta_time = now - game_state.last_input;
			game_state.last_input = now;
			// if (now - game_state.last_input > INPUT_FREQ) {
			game_state.acc_input -= INPUT_FREQ;
			// game_state.acc_input -= input_freq;

			process_input(window, player);
			window->should_close = WindowShouldClose(window);

			if (game_state.paused) {
				// VINFO("paused");
#ifndef __ANDROID__
				glfwWaitEvents();
#endif //__ANDROID
				threadpool_pause(&world->thread);
				// END_SECT("main loop");
				return;
			}
			threadpool_resume(&world->thread);

			player_update(player);

			BEGIN_SECT("Poll Events");
			platform_poll_events();
			END_SECT("Poll Events");

			if (player->breaking) {
				player_break_block(&game_state.world.pool, &game_state.world.ogl_pool, &game_state.world.render_map, &player->pos, world->seed);
				player->breaking = false;
			}
			if (player->placing) {
				static bool mwew = false;
				if (mwew)
					player_place_block(player, BlocktypeGrass, &game_state.world.pool, &game_state.world.ogl_pool, &game_state.world.render_map, &player->pos, world->seed);
				else
					player_place_block(player, BlocktypeStone, &game_state.world.pool, &game_state.world.ogl_pool, &game_state.world.render_map, &player->pos, world->seed);
				mwew = !mwew;
				player->placing = false;
			}
			world_update(world);
			// VINFO("in");
			n_ins++;
		}
	} else {
		// precise_sleep(INPUT_FREQ * 0.5);
	}
	VINFO("n_ins: %zu", n_ins);

	// RENDERING:
	// if (now - game_state.last_frame > window->freq) {
	// VINFO("frame");
	render(&game_state, window, shader);
	// }
	// BEGIN_SECT("main loop");

	f64 frame_end = time_now();
	game_state.last_frame = frame_end;
	f64 frame_time = frame_end - now;
	f64 target = window->freq;
	if (frame_time < target) {
		precise_sleep(target - frame_time);
	}
	// VINFO("FPS: %f",1/game_state.delta_time);

	// END_SECT("main loop");
#ifdef PROFILING
	spall_buffer_flush(&spall_ctx, &spall_buffer);
#endif // PROFILING
	// precise_sleep(INPUT_FREQ * 0.1);
}

bool meshuser_try_acquire(MeshResourcePool *pool, MeshUserIndex *index)
{
	for (MeshUserIndex i = 0; i < pool->max_users; i++) {
		bool expected = false;
		if (atomic_compare_exchange_weak(&pool->taken[i], &expected, true)) {
			*index = i;
			return true;
		}
	}
	return false;
}
bool meshupload_try_acquire(MeshResourcePool *pool, MeshUploadIndex *index)
{
	for (MeshUploadIndex i = 0; i < pool->max_uploads; i++) {
		size_t expected = UPLOAD_STATUS_FREE;
		if (atomic_compare_exchange_weak(&pool->upload_status[i], &expected, UPLOAD_STATUS_PROCESSING)) {
			*index = i;
			return true;
		}
	}
	return false;
}

bool workerqueue_pop(WorkerQueue *queue, WorkerTask *task)
{
	size_t current_readpos = atomic_load_explicit(&queue->readpos, memory_order_acquire);

	size_t current_writepos = atomic_load_explicit(&queue->writepos, memory_order_acquire);
	// VINFO("r: %zu w:%zu", current_readpos, current_writepos);
	if (current_readpos == current_writepos) {
		return false;
	}

	size_t next_readpos = (current_readpos + 1) % queue->capacity;

	if (atomic_compare_exchange_weak_explicit((atomic_size_t *)&queue->readpos, &current_readpos, next_readpos, memory_order_release, memory_order_relaxed)) {
		*task = queue->tasks[current_readpos];
		return true;
	}
	return false;
}

void threads_frame(Worker *worker)
{
	WorkerContext *ctx = &worker->context;
	MeshResourcePool *meshres = ctx->mesh_resource;
	WorkerTask task = { 0 };
	MeshResourceHandle res = { 0 };

	if (!meshs_empty(ctx->system)) {
		// VINFO("hi");
		if (meshuser_try_acquire(meshres, &res.user)) {
			// VINFO("%i", test++);
			while (meshupload_try_acquire(meshres, &res.up)) {
				if (workerqueue_pop(&ctx->system->mesh_up, &task)) {
					// VINFO("hi");
					meshworker_process_mesh(worker, meshres, &res, &task);
				} else if (workerqueue_pop(&ctx->system->mesh_new, &task)) {
					meshworker_process_mesh(worker, meshres, &res, &task);
				} else {
					// VINFO("bye");
					break;
				}
				// snooze(1);
			}
			meshres->taken[res.user] = false;
		}
		snooze(1);
	} else
		snooze(1);
}
static void print_block(Block *block)
{
	VINFO("Block: %p", block);
	VINFO("BlockType: %s", BlockTypeString[block->type]);
	VINFO("Obstructing: %s", block->obstructing ? "true" : "false");
}
static void player_print_block(Player *player, OGLPool *ogl, RenderMap *map, ChunkPool *pool, size_t seed)
{
	Block block = pool_read_block(pool, ogl, map, &player->pos, seed);
	VINFO("Pos: x: %i, y: %i, z: %i", player->pos.x, player->pos.y, player->pos.z)
	print_block(&block);
}
