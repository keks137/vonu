#ifndef INCLUDE_SRC_PLAYER_H_
#define INCLUDE_SRC_PLAYER_H_

#include "chunk.h"
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


#endif  // INCLUDE_SRC_PLAYER_H_
