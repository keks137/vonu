#include "disk.h"
#include <GL/glcorearb.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "vassert.h"
#include "logs.h"
#include <time.h>
#include "loadopengl.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../vendor/stb_image.h"
#include "../vendor/cglm/cglm.h"
#include "image.h"
#include "chunk.h"
#include "map.h"
#include "pool.h"

#include "verts/vert1.c"
#include "frags/frag1.c"

const char *BlockTypeString[] = {
#define X(name) #name,
	BLOCKTYPE_NAMES
#undef X
};
typedef struct {
	unsigned int program;
	unsigned int model_loc;
	unsigned int projection_loc;
	unsigned int view_loc;
} ShaderData;

typedef struct {
	vec3 pos;
	vec3 front;
	vec3 up;
	float yaw;
	float pitch;
	float fov;
} Camera;

#define RENDER_DISTANCE_X 4
#define RENDER_DISTANCE_Y 4
#define RENDER_DISTANCE_Z 4
#define MAX_VISIBLE_CHUNKS (2 * RENDER_DISTANCE_X + 1) * (2 * RENDER_DISTANCE_Y + 1) * (2 * RENDER_DISTANCE_Z + 1)

/* #define FOR_CHUNKS_IN_RENDERDISTANCE(...)                                             \
// 	for (int z = -RENDER_DISTANCE_Z; z <= RENDER_DISTANCE_Z; z++)                 \
// 		for (int y = -RENDER_DISTANCE_Y; y <= RENDER_DISTANCE_Y; y++)         \
// 			for (int x = -RENDER_DISTANCE_X; x <= RENDER_DISTANCE_X; x++) \
// 	__VA_ARGS__
*/

typedef struct {
	Camera camera;
	WorldCoord pos;
	ChunkCoord chunk_pos;
	bool placing;
	bool breaking;
} Player;

typedef struct {
	ChunkPool pool;
	RenderMap render_map;
	// ChunkPoolUnloaded unloaded;
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
GameState game_state = { 0 };

#define FLOATS_PER_VERTEX 5
#define VERTICES_PER_FACE 6
#define FACES_PER_CUBE 6

typedef enum {
	FACE_BACK = 0,
	FACE_FRONT,
	FACE_LEFT,
	FACE_RIGHT,
	FACE_BOTTOM,
	FACE_TOP
} CubeFace;

// static const vec3 face_normals[6] = {
// 	{ 0, 0, -1 }, // BACK
// 	{ 0, 0, 1 }, // FRONT
// 	{ -1, 0, 0 }, // LEFT
// 	{ 1, 0, 0 }, // RIGHT
// 	{ 0, -1, 0 }, // BOTTOM
// 	{ 0, 1, 0 } // TOP
// };

typedef struct {
	float vertices[VERTICES_PER_FACE][FLOATS_PER_VERTEX];
} FaceVertices;

#define CHUNK_TOTAL_VERTICES (CHUNK_TOTAL_BLOCKS * FACES_PER_CUBE * FLOATS_PER_VERTEX * VERTICES_PER_FACE)

typedef struct {
	float *data;
	size_t fill;
} TmpChunkVerts;

void clear_tmp_chunk_verts(TmpChunkVerts *tmp_chunk_verts)
{
	// memset(tmp_chunk_verts->data, 0, sizeof(float) * tmp_chunk_verts->fill);
	tmp_chunk_verts->fill = 0;
}

const size_t CHUNK_STRIDE_Y = (CHUNK_TOTAL_X);
const size_t CHUNK_STRIDE_Z = (CHUNK_TOTAL_X * CHUNK_TOTAL_Y);

#define CHUNK_INDEX(x, y, z) ((x) + (y) * CHUNK_STRIDE_Y + (z) * CHUNK_STRIDE_Z)

float movement_speed = 5.0f;

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
void vao_attributes_no_color(unsigned int VAO)
{
	glBindVertexArray(VAO);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
}

void chunk_init(Chunk *chunk)
{
	glGenVertexArrays(1, &chunk->VAO);
	glGenBuffers(1, &chunk->VBO);

	glBindVertexArray(chunk->VAO);
	glBindBuffer(GL_ARRAY_BUFFER, chunk->VBO);
	vao_attributes_no_color(chunk->VAO);
	chunk->initialized = true;
}

Chunk chunk_callocrash()
{
	Chunk chunk = { 0 };
	chunk.data = calloc(CHUNK_TOTAL_BLOCKS, sizeof(Block));
	VASSERT_RELEASE_MSG(chunk.data != NULL, "Buy more ram for more chunks bozo");

	(void)0;
	chunk_init(&chunk);

	return chunk;
}

BlockPos chunk_index_to_blockpos(size_t index)
{
	BlockPos pos = { 0 };
	pos.x = index % CHUNK_TOTAL_X;
	index /= CHUNK_TOTAL_X;
	pos.y = index % CHUNK_TOTAL_Y;
	pos.z = index / CHUNK_TOTAL_Y;
	return pos;
}

Block *chunk_xyz_at(const Chunk *chunk, int x, int y, int z)
{
	if (x < 0 || x >= CHUNK_TOTAL_X ||
	    y < 0 || y >= CHUNK_TOTAL_Y ||
	    z < 0 || z >= CHUNK_TOTAL_Z) {
		return NULL;
	}

	return &chunk->data[CHUNK_INDEX(x, y, z)];
}
Block *chunk_blockpos_at(const Chunk *chunk, const BlockPos *pos)
{
	VASSERT(pos->x < CHUNK_TOTAL_X &&
		pos->y < CHUNK_TOTAL_Y &&
		pos->z < CHUNK_TOTAL_Z);

	return &chunk->data[CHUNK_INDEX(pos->x, pos->y, pos->z)];
}

#define ARRAY_LEN(array) (sizeof(array) / sizeof(array[0]))

const char *vertexShaderSource = (char *)verts_vert1;
const char *fragmentShaderSource = (char *)frags_frag1;

FaceVertices cube_vertices[] = {
	// BACK
	{
		{ { 0.5f, 0.5f, -0.5f, 0.0f, 0.0f },
		  { 0.5f, -0.5f, -0.5f, 0.0f, 1.0f },
		  { -0.5f, -0.5f, -0.5f, 1.0f, 1.0f },
		  { -0.5f, -0.5f, -0.5f, 1.0f, 1.0f },
		  { -0.5f, 0.5f, -0.5f, 1.0f, 0.0f },
		  { 0.5f, 0.5f, -0.5f, 0.0f, 0.0f } } },
	// FRONT
	{
		{ { -0.5f, -0.5f, 0.5f, 0.0f, 1.0f },
		  { 0.5f, -0.5f, 0.5f, 1.0f, 1.0f },
		  { 0.5f, 0.5f, 0.5f, 1.0f, 0.0f },
		  { 0.5f, 0.5f, 0.5f, 1.0f, 0.0f },
		  { -0.5f, 0.5f, 0.5f, 0.0f, 0.0f },
		  { -0.5f, -0.5f, 0.5f, 0.0f, 1.0f } } },
	// LEFT
	{
		{ { -0.5f, 0.5f, 0.5f, 1.0f, 0.0f },
		  { -0.5f, 0.5f, -0.5f, 0.0f, 0.0f },
		  { -0.5f, -0.5f, -0.5f, 0.0f, 1.0f },
		  { -0.5f, -0.5f, -0.5f, 0.0f, 1.0f },
		  { -0.5f, -0.5f, 0.5f, 1.0f, 1.0f },
		  { -0.5f, 0.5f, 0.5f, 1.0f, 0.0f } } },
	// RIGHT
	{
		{ { 0.5f, -0.5f, -0.5f, 1.0f, 1.0f },
		  { 0.5f, 0.5f, -0.5f, 1.0f, 0.0f },
		  { 0.5f, 0.5f, 0.5f, 0.0f, 0.0f },
		  { 0.5f, 0.5f, 0.5f, 0.0f, 0.0f },
		  { 0.5f, -0.5f, 0.5f, 0.0f, 1.0f },
		  { 0.5f, -0.5f, -0.5f, 1.0f, 1.0f } } },
	// BOTTOM
	{
		{ { -0.5f, -0.5f, -0.5f, 1.0f, 0.0f },
		  { 0.5f, -0.5f, -0.5f, 0.0f, 0.0f },
		  { 0.5f, -0.5f, 0.5f, 0.0f, 1.0f },
		  { 0.5f, -0.5f, 0.5f, 0.0f, 1.0f },
		  { -0.5f, -0.5f, 0.5f, 1.0f, 1.0f },
		  { -0.5f, -0.5f, -0.5f, 1.0f, 0.0f } } },
	// TOP
	{
		{ { 0.5f, 0.5f, 0.5f, 1.0f, 1.0f },
		  { 0.5f, 0.5f, -0.5f, 1.0f, 0.0f },
		  { -0.5f, 0.5f, -0.5f, 0.0f, 0.0f },
		  { -0.5f, 0.5f, -0.5f, 0.0f, 0.0f },
		  { -0.5f, 0.5f, 0.5f, 0.0f, 1.0f },
		  { 0.5f, 0.5f, 0.5f, 1.0f, 1.0f } } }
};
unsigned int indices[] = {
	0, 1, 3,
	1, 2, 3
};

float last_x = 400;
float last_y = 300;
typedef struct {
	bool reset_mouse;
} MouseState;
MouseState mouse_state;

void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
	if (game_state.paused)
		return;
	(void)window;
	if (mouse_state.reset_mouse) {
		last_x = xpos;
		last_y = ypos;
		mouse_state.reset_mouse = false;
	}

	float xoffset = xpos - last_x;
	float yoffset = last_y - ypos;
	last_x = xpos;
	last_y = ypos;

	const float sensitivity = 0.1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	Camera *camera = &game_state.world.player.camera;
	camera->yaw += xoffset;
	camera->pitch += yoffset;

	if (camera->pitch > 89.0f)
		camera->pitch = 89.0f;
	if (camera->pitch < -89.0f)
		camera->pitch = -89.0f;
	vec3 direction = { 0 };

	direction[0] = cos(glm_rad(camera->yaw)) * cos(glm_rad(camera->pitch));
	direction[1] = sin(glm_rad(camera->pitch));
	direction[2] = sin(glm_rad(camera->yaw)) * cos(glm_rad(camera->pitch));
	glm_normalize(direction);
	glm_vec3_copy(direction, camera->front);
}
void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
	(void)window;
	glViewport(0, 0, width, height);
}

void set_paused(GLFWwindow *window, bool val)
{
	game_state.paused = val;
	if (game_state.paused) {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	} else {
		mouse_state.reset_mouse = true;
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
}

void window_focus_callback(GLFWwindow *window, int focused)
{
	if (focused == GLFW_TRUE) {
	} else {
		set_paused(window, true);
	}
}

void process_input(GLFWwindow *window, Player *player)
{
	static bool escapeKeyPressedLastFrame = false;

	bool escapeKeyPressedThisFrame = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;

	if (escapeKeyPressedThisFrame && !escapeKeyPressedLastFrame) {
		set_paused(window, !game_state.paused);
	}

	escapeKeyPressedLastFrame = escapeKeyPressedThisFrame;

	if (game_state.paused) {
		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
			set_paused(window, false);
		}
		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
			set_paused(window, false);
		}

		return;
	}

	Camera *camera = &player->camera;
	const float camera_speed = movement_speed * game_state.delta_time;
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
		camera->pos[1] += camera_speed;
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
		camera->pos[1] -= camera_speed;
	}
	static bool sprinting;
	float forward_speed = camera_speed;
	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
		sprinting = true;
	}
	if (sprinting)
		forward_speed *= 2;

	vec3 horizontal_front;
	glm_vec3_copy(camera->front, horizontal_front);
	horizontal_front[1] = 0.0f;
	glm_normalize(horizontal_front);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		glm_vec3_muladds(horizontal_front, forward_speed, camera->pos);
	} else {
		sprinting = false;
	}

	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		glm_vec3_mulsubs(horizontal_front, camera_speed, camera->pos);
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		vec3 right;
		glm_cross(horizontal_front, camera->up, right);
		glm_normalize(right);
		glm_vec3_mulsubs(right, camera_speed, camera->pos);
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
		vec3 right;
		glm_cross(horizontal_front, camera->up, right);
		glm_normalize(right);
		glm_vec3_muladds(right, camera_speed, camera->pos);
	}
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
		player->breaking = true;
	}
}

bool verify_shader(unsigned int shader)
{
	int success;
	char infoLog[512];
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(shader, 512, NULL, infoLog);
		VERROR(infoLog);
		return false;
	}
	return true;
}
bool verify_program(unsigned int program)
{
	int success;
	char infoLog[512];
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(program, 512, NULL, infoLog);
		VERROR(infoLog);
		return false;
	}
	return true;
}

bool create_shader_program(unsigned int *shader_program, const char *vertex_shader_str, const char *fragment_shader_str)
{
	unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &vertex_shader_str, NULL);
	glCompileShader(vertex_shader);
	if (!verify_shader(vertex_shader))
		return false;

	unsigned int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &fragment_shader_str, NULL);
	glCompileShader(fragment_shader);
	if (!verify_shader(fragment_shader))
		return false;

	*shader_program = glCreateProgram();
	glAttachShader(*shader_program, vertex_shader);
	glAttachShader(*shader_program, fragment_shader);
	glLinkProgram(*shader_program);
	verify_program(*shader_program);
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
	return true;
}

bool load_image(Image *img, const char *path)
{
	int width = 0;
	int height = 0;
	int nrChannels = 0;
	// stbi_set_flip_vertically_on_load(true);
	FILE *file = fopen(path, "rb");
	if (file == NULL) {
		VERROR("Could not open file: %s", path);
		return false;
	}
	unsigned char *data = stbi_load_from_file(file, &width, &height, &nrChannels, 0);
	fclose(file);
	img->width = width;
	img->height = height;
	img->n_chan = nrChannels;
	img->data = data;
	if (data == NULL)
		return false;

	return true;
}

void print_image_info(Image *image)
{
	VINFO("height: %zu", image->height);
	VINFO("width: %zu", image->width);
	VINFO("n_chan: %zu", image->n_chan);
}

#define GL_CHECK(stmt)                                                                                            \
	do {                                                                                                      \
		stmt;                                                                                             \
		GLenum err = glGetError();                                                                        \
		if (err != GL_NO_ERROR) {                                                                         \
			VERROR("OpenGL error 0x%04X at %s:%d: %s: %s", err, __FILE__, __LINE__, __func__, #stmt); \
			print_chunk(chunk);                                                                       \
		}                                                                                                 \
	} while (0)

void chunk_render(Chunk *chunk, ShaderData shader_data)
{
	if (!chunk->up_to_date || !chunk->initialized)
		return;
	if (chunk->vertex_count == 0) {
		// VERROR("This guy again:");
		return;
	}
	// print_chunk(chunk);

	VASSERT_MSG(chunk->contains_blocks, "How did it get here?");
	VASSERT_MSG(chunk->VBO != 0, "How did it get here?");
	VASSERT_MSG(chunk->vertex_count > 0, "How did it get here?");

	chunk->unchanged_render_count++;

	GL_CHECK(glUseProgram(shader_data.program));
	GL_CHECK(glBindVertexArray(chunk->VAO));

	mat4 model = { 0 };
	glm_mat4_identity(model);
	vec3 pos = { 0 };
	pos[0] = chunk->coord.x * CHUNK_TOTAL_X;
	pos[1] = chunk->coord.y * CHUNK_TOTAL_Y;
	pos[2] = chunk->coord.z * CHUNK_TOTAL_Z;
	GL_CHECK(glm_translate(model, pos));

	GL_CHECK(glUniformMatrix4fv(shader_data.model_loc, 1, GL_FALSE, (const float *)model));

	GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, chunk->vertex_count));
	// GL_CHECK(print_chunk(chunk));
}

void vec3_assign(vec3 v, float x, float y, float z)
{
	v[0] = x;
	v[1] = y;
	v[2] = z;
}
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

void texture_get_uv_vertex(float local_u, float local_v, BLOCKTYPE type, CubeFace face, float *u, float *v)
{
	static const TextureCoords texture_map[256][FACES_PER_CUBE] = {
		[BlocktypeGrass] = {
			[FACE_BACK] = { 1, 0 },
			[FACE_FRONT] = { 1, 0 },
			[FACE_LEFT] = { 1, 0 },
			[FACE_RIGHT] = { 1, 0 },
			[FACE_BOTTOM] = { 2, 0 },
			[FACE_TOP] = { 0, 0 } }
	};

	TextureCoords coords = texture_map[type][face];
	// float adjusted_u = local_u;
	// float adjusted_v = local_v;
	//
	// // Grass side textures need rotation for proper orientation
	// if (type == BlocktypeGrass && (face >= FACE_BACK && face <= FACE_RIGHT)) {
	// 	// For side faces of grass blocks, rotate UVs
	// 	switch (face) {
	// 	case FACE_BACK:
	// 		// Back face: rotate 180 degrees
	// 		adjusted_u = 1.0f - local_u;
	// 		adjusted_v = 1.0f - local_v;
	// 		break;
	// 	case FACE_FRONT:
	// 		// Front face: no rotation needed (already correct)
	// 		adjusted_u = local_v;
	// 		adjusted_v =  1.0f -local_u;
	// 		break;
	// 	case FACE_LEFT:
	// 		// Left face: rotate -90 degrees
	// 		adjusted_u = local_v;
	// 		adjusted_v = 1.0f - local_u;
	// 		break;
	// 	case FACE_RIGHT:
	// 		// Right face: rotate 90 degrees
	// 		adjusted_u = 1.0f - local_v;
	// 		adjusted_v = 1.0f -local_u;
	// 		break;
	// 	default:
	// 		break;
	// 	}
	// }
	// For non-grass blocks or top/bottom faces, use standard orientation
	// (You might need similar adjustments for other block types)

	// get_tile_uv(coords.tileX, coords.tileY, adjusted_u, adjusted_v, u, v);
	get_tile_uv(coords.tileX, coords.tileY, local_u, local_v, u, v);
}

void add_face_to_buffer(TmpChunkVerts *buffer, int x, int y, int z, BLOCKTYPE type, CubeFace face)
{
	FaceVertices *face_verts = &cube_vertices[face];

	for (size_t i = 0; i < VERTICES_PER_FACE; i++) {
		float *vertex_data = face_verts->vertices[i];
		float px = vertex_data[0] + 0.5f + (float)x;
		float py = vertex_data[1] + 0.5f + (float)y;
		float pz = vertex_data[2] + 0.5f + (float)z;

		float local_u = vertex_data[3];
		float local_v = vertex_data[4];

		float u, v;
		texture_get_uv_vertex(local_u, local_v, type, face, &u, &v);

		VASSERT_MSG(buffer->fill + FLOATS_PER_VERTEX <= CHUNK_TOTAL_VERTICES * FLOATS_PER_VERTEX, "Vertex buffer overflow!");

		buffer->data[buffer->fill++] = px;
		buffer->data[buffer->fill++] = py;
		buffer->data[buffer->fill++] = pz;
		buffer->data[buffer->fill++] = u;
		buffer->data[buffer->fill++] = v;
	}
}

void add_block_faces_to_buffer(Chunk *chunk, TmpChunkVerts *tmp_chunk_verts, int x, int y, int z)
{
	static const vec3 face_offsets[FACES_PER_CUBE] = {
		{ 0, 0, -1 },
		{ 0, 0, 1 },
		{ -1, 0, 0 },
		{ 1, 0, 0 },
		{ 0, -1, 0 },
		{ 0, 1, 0 }
	};

	Block *block = chunk_xyz_at(chunk, x, y, z);
	if (!block || !block->obstructing)
		return;

	for (CubeFace face = FACE_BACK; face <= FACE_TOP; face++) {
		int nx = x + face_offsets[face][0];
		int ny = y + face_offsets[face][1];
		int nz = z + face_offsets[face][2];

		Block *neighbor = chunk_xyz_at(chunk, nx, ny, nz);
		if (!neighbor || !neighbor->obstructing) {
			add_face_to_buffer(tmp_chunk_verts, x, y, z, block->type, face);
		}
	}
}
void print_block(Block *block)
{
	VINFO("Block: %p", block);
	VINFO("BlockType: %s", BlockTypeString[block->type]);
	VINFO("Obstructing: %s", block->obstructing ? "true" : "false");
}

void chunk_generate_mesh(Chunk *chunk, TmpChunkVerts *tmp_chunk_verts)
{
	if (chunk->up_to_date || !chunk->contains_blocks)
		return;
	VASSERT(chunk->data != NULL);

	chunk->unchanged_render_count = 0;
	clear_tmp_chunk_verts(tmp_chunk_verts);
	for (size_t z = 0; z < CHUNK_TOTAL_Z; z++) {
		for (size_t y = 0; y < CHUNK_TOTAL_Y; y++) {
			for (size_t x = 0; x < CHUNK_TOTAL_X; x++) {
				Block *block = &chunk->data[CHUNK_INDEX(x, y, z)];
				// chunk->data[CHUNK_INDEX(x, y, z)];
				if (!block->obstructing)
					continue;

				add_block_faces_to_buffer(chunk, tmp_chunk_verts, x, y, z);
			}
		}
	}
	chunk->vertex_count = tmp_chunk_verts->fill / FLOATS_PER_VERTEX;

	if (chunk->vertex_count == 0) {
		VERROR("This guy:");
		print_chunk(chunk);
		print_block(&chunk->data[0]);
		return;
	}
	if (!chunk->initialized)
		chunk_init(chunk);
	else
		VINFO("did it again");

	glBindVertexArray(chunk->VAO);
	glBindBuffer(GL_ARRAY_BUFFER, chunk->VBO);
	glBufferData(GL_ARRAY_BUFFER,
		     tmp_chunk_verts->fill * sizeof(float),
		     tmp_chunk_verts->data,
		     GL_DYNAMIC_DRAW);
	chunk->has_vbo_data = true;

	chunk->up_to_date = true;
}

void print_chunk(Chunk *chunk)
{
	VINFO("Chunk: %p", chunk);
	VINFO("Data: %p", chunk->data);
	VINFO("coords: %i %i %i", chunk->coord.x, chunk->coord.y, chunk->coord.z);
	VINFO("VAO: %i", chunk->VAO);
	VINFO("VBO: %i", chunk->VBO);
	VINFO("unchanged_render_count: %i", chunk->unchanged_render_count);
	VINFO("up_to_date: %s", chunk->up_to_date ? "true" : "false");
	VINFO("terrain_generated: %s", chunk->terrain_generated ? "true" : "false");
	VINFO("contains_blocks: %s", chunk->contains_blocks ? "true" : "false");
	VINFO("initialized: %s", chunk->initialized ? "true" : "false");
	VINFO("vertex_count: %i", chunk->vertex_count);
	VINFO("cycles_since_update: %i", chunk->cycles_since_update);
}

bool chunk_caught = false;
Chunk *chunk_to_catch = NULL;
void chunk_generate_terrain(Chunk *chunk, size_t seed)
{
	VASSERT(chunk->data != NULL);
	// VINFO("Generating terrain: %i %i %i", chunk->coord.x, chunk->coord.y, chunk->coord.z);

	srand(seed);
	if (chunk->coord.y == 0) {
		for (size_t z = 0; z < CHUNK_TOTAL_Z; z++) {
			for (size_t y = 0; y < CHUNK_TOTAL_Y; y++) {
				size_t y_lim = 1 + rand() % 20;
				for (size_t x = 0; x < CHUNK_TOTAL_X; x++) {
					if (y < y_lim) {
						chunk->contains_blocks = true;
						chunk->data[CHUNK_INDEX(x, y, z)].type = BlocktypeGrass;
						chunk->data[CHUNK_INDEX(x, y, z)].obstructing = true;
					}
				}
			}
		}
		// VINFO("Generated:");
		// print_block(&chunk->data[0]);
	}
	chunk->terrain_generated = true;
}

void chunk_load(Chunk *chunk, const ChunkCoord *coord, size_t seed)
{
	chunk->last_used = time(NULL);
	chunk->coord = *coord;
	memset(chunk->data, 0, sizeof(Block) * CHUNK_TOTAL_BLOCKS);
	chunk_generate_terrain(chunk, seed);
}

bool pool_find_chunk_coord(ChunkPool *pool, const ChunkCoord *coord, size_t *index)
{
	size_t idx;
	if (index == NULL)
		index = &idx;
	for (size_t i = 0; i < pool->lvl; i++) {
		Chunk *pc = &pool->chunk[i];
		if (pc->coord.x == coord->x &&
		    pc->coord.y == coord->y &&
		    pc->coord.z == coord->z) {
			*index = i;
			return true;
		}
	}
	return false;
}

bool poolunloaded_find_chunk_coord(ChunkPoolUnloaded *unloaded, ChunkCoord coord, size_t *index)
{
	for (size_t i = 0; i < unloaded->lvl; i++) {
		Chunk *pc = &unloaded->chunk[i];
		if (pc->coord.x == coord.x &&
		    pc->coord.y == coord.y &&
		    pc->coord.z == coord.z) {
			*index = i;
			return true;
		}
	}
	return false;
}
bool pool_get_index(ChunkPool *pool, Chunk *chunk, size_t *index)
{
	for (size_t i = 0; i < pool->lvl; i++) {
		if (
			chunk->coord.x == pool->chunk[i].coord.x &&
			chunk->coord.y == pool->chunk[i].coord.y &&
			chunk->coord.z == pool->chunk[i].coord.z) {
			*index = i;
			return true;
		}
	}
	return false;
}

size_t pool_replaceable_index(ChunkPool *pool)
{
	size_t index = 0;
	time_t oldest = time(NULL);

	for (size_t i = 0; i < pool->lvl; i++) {
		Chunk *pc = &pool->chunk[i];
		if (pc->last_used < oldest) {
			oldest = pc->last_used;
			index = i;
		}
	}

	return index;
}

void chunk_free(Chunk *chunk)
{
	VASSERT(chunk->initialized);
	VASSERT(chunk->VAO != 0 && chunk->VBO != 0);
	// VINFO("deleted:");
	// print_chunk(chunk);
	glDeleteVertexArrays(1, &chunk->VAO);
	glDeleteBuffers(1, &chunk->VBO);
}

void chunk_clear_metadata(Chunk *chunk)
{
	Chunk tmp = { 0 };
	tmp.data = chunk->data;
	// tmp.VAO = chunk->VAO;
	// tmp.VBO = chunk->VBO;
	// glDeleteBuffers(1, &chunk->VBO);
	memset(chunk, 0, sizeof(*chunk));
	// chunk->VAO = tmp.VAO;
	// chunk->VBO = tmp.VBO;
	chunk->data = tmp.data;
}
void pool_reserve(ChunkPool *pool, size_t num)
{
	VASSERT(pool->lvl + num <= pool->cap);
	pool->blockdata = realloc(pool->blockdata, sizeof(Block) * (pool->lvl + num) * CHUNK_TOTAL_BLOCKS);
	VASSERT_MSG(pool->blockdata != NULL, "Buy more ram bozo");
	Block *start_new_blocks = pool->blockdata + sizeof(Block) * CHUNK_TOTAL_BLOCKS * pool->lvl;
	memset(start_new_blocks, 0, num * sizeof(Block) * CHUNK_TOTAL_BLOCKS);

	for (size_t i = 0; i < num; i++) {
		Chunk chunk = { 0 };
		chunk.data = start_new_blocks + i * CHUNK_TOTAL_BLOCKS;
		pool->chunk[pool->lvl + i] = chunk;
	}
	pool->lvl += num;
}
size_t pool_append(ChunkPool *pool, const ChunkCoord *coord, size_t seed)
{
	VASSERT(pool->lvl < pool->cap);
	chunk_clear_metadata(&pool->chunk[pool->lvl]);
	chunk_load(&pool->chunk[pool->lvl], coord, seed);
	return pool->lvl++;
}

void pool_replace(ChunkPool *pool, const ChunkCoord *coord, size_t index, size_t seed)
{
	VASSERT(pool->lvl > index);
	chunk_clear_metadata(&pool->chunk[index]);
	chunk_load(&pool->chunk[index], coord, seed);
}

size_t pool_add(ChunkPool *pool, const ChunkCoord *coord, size_t seed)
{
	if (pool->lvl < pool->cap) {
		return pool_append(pool, coord, seed);
	} else {
		size_t index = pool_replaceable_index(pool);
		pool_replace(pool, coord, index, seed);
		return index;
	}
}

// void pool_remove(ChunkPool *pool, size_t index)
// {
// 	VASSERT(pool->lvl > index);
// 	pool->lvl--;
// 	pool->chunk[index] = pool->chunk[pool->lvl];
// }

size_t poolunloaded_append(ChunkPoolUnloaded *unloaded, Chunk chunk)
{
	VASSERT(unloaded->lvl < unloaded->cap);
	unloaded->chunk[unloaded->lvl] = chunk;
	return unloaded->lvl++;
}
size_t poolunloaded_replaceable_index(ChunkPoolUnloaded *unloaded)
{
	size_t index = 0;
	time_t oldest = time(NULL);

	for (size_t i = 0; i < unloaded->lvl; i++) {
		Chunk *pc = &unloaded->chunk[i];
		if (pc->last_used < oldest) {
			oldest = pc->last_used;
			index = i;
		}
	}

	return index;
}
void poolunloaded_remove(ChunkPoolUnloaded *unloaded, size_t index)
{
	VASSERT(unloaded->lvl > index);

	unloaded->lvl--;
	unloaded->chunk[index] = unloaded->chunk[unloaded->lvl];
}
void poolunloaded_replace(ChunkPoolUnloaded *unloaded, Chunk chunk, size_t index)
{
	VASSERT(unloaded->lvl > index);
	unloaded->chunk[index] = chunk;
}

size_t poolunloaded_add(ChunkPoolUnloaded *unloaded, Chunk chunk)
{
	if (unloaded->lvl < unloaded->cap) {
		return poolunloaded_append(unloaded, chunk);
	} else {
		size_t index = poolunloaded_replaceable_index(unloaded);
		poolunloaded_replace(unloaded, chunk, index);
		return index;
	}
}
void pool_empty(ChunkPool *pool, size_t index)
{
	VASSERT(pool->lvl > index);
	// TODO: store completely empty chunks somewhere else, since they don't need a VBO anyways
	VASSERT_MSG(pool->chunk[index].up_to_date || !pool->chunk[index].contains_blocks, "There was no reason to unload this");

	pool->lvl--;
	Chunk replacement = pool->chunk[pool->lvl];
	pool->chunk[pool->lvl].data = pool->chunk[index].data;
	pool->chunk[index] = replacement;
	// print_chunk(&pool->chunk[index]);
}

size_t pool_unload(ChunkPool *pool, ChunkPoolUnloaded *unloaded, size_t index)
{
	VASSERT(pool->lvl > index);
	// TODO: store completely empty chunks somewhere else, since they don't need a VBO anyways
	VASSERT_MSG(pool->chunk[index].up_to_date || !pool->chunk[index].contains_blocks, "There was no reason to unload this");

	Chunk tmp = pool->chunk[index];

	tmp.data = NULL;

	size_t unloaded_index = poolunloaded_add(unloaded, tmp);

	pool->lvl--;
	Chunk replacement = pool->chunk[pool->lvl];
	pool->chunk[pool->lvl].data = pool->chunk[index].data;
	pool->chunk[index] = replacement;
	// print_chunk(&pool->chunk[index]);
	return unloaded_index;
}
// TODO: add some kind of chunk activeness scoring system
size_t pool_find_unloadable(ChunkPool *pool, ChunkPoolUnloaded *unloaded)
{
	size_t count = 0;
	for (size_t i = 0; i < pool->lvl; i++) {
		if (pool->chunk[i].unchanged_render_count > 5) {
			VASSERT(pool->chunk[i].terrain_generated);
			pool_unload(pool, unloaded, i);
			count++;
		} else if (pool->chunk[i].contains_blocks == false) {
			pool_unload(pool, unloaded, i);
			count++;
		}
	}
	return count;
}

bool pool_load_relaxed(ChunkPool *pool, RenderMap *map, const ChunkCoord *coord, size_t *index, size_t seed)
{
	if (rendermap_find(map, coord, index)) {
		return true;
	}
	if (pool_find_chunk_coord(pool, coord, index)) {
		return true;
	}
	if (pool->lvl < pool->cap) {
		*index = pool_append(pool, coord, seed);

		return true;
	}

	return false;
}

size_t pool_load(ChunkPool *pool, const ChunkCoord *coord, size_t seed)
{
	size_t index = 0;

	// TODO: cache disk chunks in hash set

	if (pool_find_chunk_coord(pool, coord, &index)) {
		return index;
	}

	return pool_add(pool, coord, seed);
}

typedef struct {
	ChunkCoord coord;
	int64_t x_min, x_max, y_min, y_max, z_min, z_max;
} ChunkIterator;

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

void world_init(World *world)
{
	world->pool.cap = 128;
	world->pool.chunk = calloc(world->pool.cap, sizeof(Chunk));
	VASSERT_RELEASE_MSG(world->pool.chunk != NULL, "Buy more RAM");

	rendermap_init(&world->render_map, 2 * 2048, 2);

	pool_reserve(&world->pool, world->pool.cap);

	size_t pl = 0;
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

	world->seed = time(NULL);
	do {
		ChunkCoord coord = { it.coord.x, it.coord.y, it.coord.z };
		if (pl == world->pool.lvl)
			goto done;
		chunk_load(&world->pool.chunk[pl], &coord, world->seed);
		pl++;
	} while (next_chunk(&it));
	chunk_load(&world->pool.chunk[0], &(ChunkCoord){ 0, 0, 0 }, world->seed);

// pool_load(&world->pool, (ChunkCoord){ 0, 0, 0 });
done:
	return;
}

void world_render(World *world, TmpChunkVerts *tmp_chunk_verts, ShaderData shader_data)
{
	for (size_t i = 0; i < world->pool.lvl; i++) {
		Chunk *chunk = &world->pool.chunk[i];
		VASSERT(chunk->data != NULL);
		chunk_generate_mesh(chunk, tmp_chunk_verts);

		// TODO: add an empty map for empty chunks
		// if (world->pool.chunk[i].up_to_date) {
		rendermap_add(&world->render_map, chunk);
		if (chunk->modified && chunk->contains_blocks) {
			if (chunk->cycles_since_update > 400) {
				disk_save(chunk, world->uid);
				pool_empty(&world->pool, i);
			}
			chunk->cycles_since_update++;
		} else // TODO:dont do this, maybe do keep it around for a while
			pool_empty(&world->pool, i);
	}

	for (int z = -RENDER_DISTANCE_Z; z <= RENDER_DISTANCE_Z; z++) {
		for (int y = -RENDER_DISTANCE_Y; y <= RENDER_DISTANCE_Y; y++) {
			for (int x = -RENDER_DISTANCE_X; x <= RENDER_DISTANCE_X; x++) {
				ChunkCoord coord = {
					world->player.chunk_pos.x + x,
					world->player.chunk_pos.y + y,
					world->player.chunk_pos.z + z
				};
				Chunk chunk = { 0 };
				if (rendermap_get_chunk(&world->render_map, &chunk, &coord)) {
					// VINFO("hi");
					// print_chunk(&chunk);
					chunk_render(&chunk, shader_data);
				}
			}
		}
	}

	// for (size_t i = 0; i < world->unloaded.lvl; i++) {
	// 	chunk_render(&world->unloaded.chunk[i], shader_data);
	// }
	// ChunkCoord player_chunk = world_coord_to_chunk(world->player.pos);
	// VINFO("Player coords: %i %i %i", player_chunk.x, player_chunk.y, player_chunk.z);
}
void print_poolunloaded(ChunkPoolUnloaded *pool)
{
	VINFO("PoolUnloaded: %p", pool);
	VINFO("lvl: %zu", pool->lvl);
	VINFO("cap: %zu", pool->cap);
}

void rendermap_keep_only_in_range(RenderMap *map)
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
			rendermap_add_next_buffer(map, &chunk);
		}
		rendermap_advance_buffer(map);
	} while (next_chunk(&it));
}

void rendermap_clean_maybe(RenderMap *map)
{
	if (map->count > 1.5f * MAX_VISIBLE_CHUNKS) {
		rendermap_keep_only_in_range(map);
	}
	if (map->count > .75f * map->table_size) {
		rendermap_keep_only_in_range(map);
	}
}

void world_update(World *world)
{
	rendermap_clean_maybe(&world->render_map);
	// size_t num = pool_find_unloadable(&world->pool, &world->unloaded);
	// VINFO("Num unloaded: %zu", num);
	// VINFO("==========================================");
	// print_poolunloaded(&world->unloaded);

	// if (world->pool.lvl < world->pool.cap) {
	// TODO: something better (or separate thread)
	ChunkCoord new_chunk = world_coord_to_chunk(&world->player.pos);
	// if (new_chunk.x != world->player.chunk_pos.x ||
	//     new_chunk.y != world->player.chunk_pos.y ||
	//     new_chunk.z != world->player.chunk_pos.z) {
	world->player.chunk_pos = new_chunk;
	for (int z = -RENDER_DISTANCE_Z; z <= RENDER_DISTANCE_Z; z++) {
		for (int y = -RENDER_DISTANCE_Y; y <= RENDER_DISTANCE_Y; y++) {
			for (int x = -RENDER_DISTANCE_X; x <= RENDER_DISTANCE_X; x++) {
				ChunkCoord coord = {
					world->player.chunk_pos.x + x,
					world->player.chunk_pos.y + y,
					world->player.chunk_pos.z + z
				};

				size_t index;
				pool_load_relaxed(&world->pool, &world->render_map, &coord, &index, world->seed);
				// VINFO("Trying to load: %i %i %i", coord.x, coord.y, coord.z);
				// VINFO("Loaded index: %i", index);
			}
		}
	}
	// }
}
void player_update(Player *player)
{
	player->pos.x = floorf(player->camera.pos[0]);
	player->pos.y = floorf(player->camera.pos[1]) - 1;
	player->pos.z = floorf(player->camera.pos[2]);
}

void print_pool(ChunkPool *pool)
{
	VINFO("Pool: %p", pool);
	VINFO("lvl: %zu", pool->lvl);
	VINFO("cap: %zu", pool->cap);
#if 0
	for (size_t i = 0; i < pool->lvl; i++) {
		VINFO("Chunk index: %zu", i);
		print_chunk(&pool->chunk[i]);
	}
#endif
}

typedef struct {
	GLFWwindow *glfw;
	int width;
	int height;
} WindowData;
void render(GameState *game_state, WindowData window, ShaderData shader_data, TmpChunkVerts tmp_chunk_verts)
{
	mat4 view;

	vec3 camera_pos_plus_front;
	Camera *camera = &game_state->world.player.camera;

	glm_vec3_add(camera->pos, camera->front, camera_pos_plus_front);
	glm_lookat(camera->pos, camera_pos_plus_front, camera->up, view);

	mat4 projection = { 0 };
	float aspect = (float)window.width / (float)window.height;

	float chunkRadius = RENDER_DISTANCE_X * CHUNK_TOTAL_X;
	float farPlane = sqrt(3 * chunkRadius * chunkRadius);
	// glm_perspective(glm_rad(game_state->camera.fov), aspect, 0.1f, 500.0f, projection);
	glm_perspective(glm_rad(camera->fov), aspect, 0.1f, farPlane, projection);

	glClearColor(0.39f, 0.58f, 0.92f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUniformMatrix4fv(shader_data.view_loc, 1, GL_FALSE, (const float *)view);

	glUniformMatrix4fv(shader_data.projection_loc, 1, GL_FALSE, (const float *)projection);

	// GL_CHECK((void)0);// TODO:reenable

	world_render(&game_state->world, &tmp_chunk_verts, shader_data);

	glfwSwapBuffers(window.glfw);
	glfwPollEvents();
}

typedef struct {
	vec3 origin;
	vec3 direction;
} Ray;

void pool_update_chunk(ChunkPool *pool, const ChunkCoord *pos, Chunk **chunk, size_t seed)
{
	size_t index = pool_load(pool, pos, seed);
	*chunk = &pool->chunk[index];
	VASSERT(chunk != NULL);
	(*chunk)->up_to_date = false;
	(*chunk)->modified = true;
	(*chunk)->updates_this_cycle++;
	(*chunk)->cycles_since_update = 0;
}

void pool_update_block(ChunkPool *pool, const WorldCoord *pos, Block **block, size_t seed)
{
	ChunkCoord chunk_pos = { 0 };
	BlockPos block_pos = { 0 };
	world_cord_to_chunk_and_block(pos, &chunk_pos, &block_pos);
	Chunk *chunk = NULL;
	pool_update_chunk(pool, &chunk_pos, &chunk, seed);
	VASSERT(chunk != NULL);
	VASSERT(chunk->data != NULL);
	*block = chunk_blockpos_at(chunk, &block_pos);
}

void player_print_block(Player *player, ChunkPool *pool, size_t seed)
{
	Block *block = NULL;
	pool_update_block(pool, &player->pos, &block, seed);
	VINFO("Pos: x: %i, y: %i, z: %i", player->pos.x, player->pos.y, player->pos.z);
	print_block(block);
}
void player_break_block(ChunkPool *pool, const WorldCoord *pos, size_t seed)
{
	// TODO: check if player is actually allowed to
	Block *block = NULL;
	pool_update_block(pool, pos, &block, seed);
	memset(block, 0, sizeof(*block));
}

int main()
{
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	Image imageData = { 0 };
	if (!load_image(&imageData, "assets/atlas.png"))
		exit(1);
	// print_image_info(&imageData);
	if (!glfwInit()) {
		exit(1);
	}

	WindowData window = { 0 };
	window.width = 1080;
	window.height = 800;
	window.glfw = glfwCreateWindow(window.width, window.height, "Vonu", NULL, NULL);
	if (!window.glfw) {
		glfwTerminate();
		exit(1);
	}

	glfwMakeContextCurrent(window.glfw);
	if (!load_gl_functions())
		exit(1);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_DEPTH_CLAMP);

	glfwSetCursorPosCallback(window.glfw, mouse_callback);

	glfwSetFramebufferSizeCallback(window.glfw, framebuffer_size_callback);

	unsigned int texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	float borderColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	GLenum format;
	if (imageData.n_chan == 3) {
		format = GL_RGB;
	} else if (imageData.n_chan == 4) {
		format = GL_RGBA;
	} else {
		format = GL_RGB;
	}

	glTexImage2D(GL_TEXTURE_2D, 0, format, imageData.width, imageData.height, 0, format, GL_UNSIGNED_BYTE, imageData.data);
	glGenerateMipmap(GL_TEXTURE_2D);
	stbi_image_free(imageData.data);

	ShaderData shader_data = { 0 };
	// VINFO("==vert==:\n%s\n========", vertexShaderSource);
	// VINFO("==frag==:\n%s\n========", fragmentShaderSource);
	if (!create_shader_program(&shader_data.program, vertexShaderSource, fragmentShaderSource))
		exit(1);

	game_state.world.player.camera = (Camera){
		.fov = 90.0f,
		.yaw = -00.0f,
		.pitch = 0.0f,
		.pos = { 0.0f, 30.0f, 0.0f },
		.up = { 0.0f, 1.0f, 0.0f },
		.front = { 0.0f, 0.0f, -1.0f },
	};

	// glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); //wireframe

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	// glfwSwapInterval(0); // no vsync

	World *world = &game_state.world;
	world_init(world);

	{
		static char *uid = "hi";
		world->uid = uid;
	}
	disk_init(world->uid);

	glUseProgram(shader_data.program);
	glUniform1i(glGetUniformLocation(shader_data.program, "texture1"), 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);

	glfwSetInputMode(window.glfw, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetWindowFocusCallback(window.glfw, window_focus_callback);
	// glfwSetMouseButtonCallback(window.glfw, mouse_button_callback);

	static float tmp_chunk_verts_data[CHUNK_TOTAL_VERTICES];
	TmpChunkVerts tmp_chunk_verts = { .data = tmp_chunk_verts_data, .fill = 0 };

	{
		shader_data.view_loc = glGetUniformLocation(shader_data.program, "view");
		shader_data.projection_loc = glGetUniformLocation(shader_data.program, "projection");
		shader_data.model_loc = glGetUniformLocation(shader_data.program, "model");
	}

	Player *player = &world->player;
	while (!glfwWindowShouldClose(window.glfw)) {
		float current_frame = glfwGetTime();
		game_state.delta_time = current_frame - game_state.last_frame;
		game_state.last_frame = current_frame;

		// VINFO("FPS: %f",1/game_state.delta_time);

		process_input(window.glfw, player);

		if (game_state.paused) {
			glfwWaitEvents();
			continue;
		}
		player_update(player);

		// player_print_block(player, &game_state.world.pool, world->seed);
		if (player->breaking) {
			player_break_block(&game_state.world.pool, &player->pos, world->seed);
			player->breaking = false;
		}
		world_update(world);

		render(&game_state, window, shader_data, tmp_chunk_verts);
	}

	glfwDestroyWindow(window.glfw);
	glfwTerminate();
}
