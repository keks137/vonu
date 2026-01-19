#ifndef INCLUDE_SRC_GAME_H_
#define INCLUDE_SRC_GAME_H_

#include "pool.h"
#include "thread.h"

typedef struct {
	vec3 pos;
	vec3 front;
	vec3 up;
	float yaw;
	float pitch;
	float fov;
} Camera;
typedef struct {
	Camera camera;
	WorldCoord pos;
	ChunkCoord chunk_pos;
	bool placing;
	bool breaking;
	float movement_speed;
} Player;
typedef struct {
	ChunkPool pool;
	OGLPool ogl_pool;
	ThreadPool thread;
	RenderMap render_map;
	MeshQueue mesh;
	size_t chunk_count;
	Player player;
	size_t seed;
	const char *name;
	const char *uid;
} World;
typedef struct {
	World world;
	float delta_time;
	float last_frame;
	bool paused;
} GameState;

extern GameState game_state;

#endif // INCLUDE_SRC_GAME_H_
