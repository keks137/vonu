#include <GL/glcorearb.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "vassert.h"
#include "logs.h"
#include <time.h>
#include "loadopengl.h"
#include <unistd.h>
#define STB_IMAGE_IMPLEMENTATION
#include "../vendor/stb_image.h"
#include "../vendor/cglm/cglm.h"
#include "image.h"
#include "chunk.h"
#include "pool.h"

#include "verts/vert1.c"
#include "frags/frag1.c"

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

typedef struct {
	WorldCoord pos;
	ChunkCoord chunk_pos;
} Player;

typedef struct {
	ChunkPool pool;
	size_t chunk_count;
	Player player;
} World;

typedef struct {
	World world;
	Camera camera;
	float delta_time;
	float last_frame;
	bool paused;
} GameState;
GameState game_state = { 0 };

#define CHUNK_TOTAL_X 32
#define CHUNK_TOTAL_Y 32
#define CHUNK_TOTAL_Z 32
#define CHUNK_TOTAL_BLOCKS (CHUNK_TOTAL_X * CHUNK_TOTAL_Y * CHUNK_TOTAL_Z)

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

ChunkCoord world_coord_to_chunk(WorldCoord world)
{
	ChunkCoord chunk;

	chunk.x = (world.x >= 0) ? world.x / CHUNK_TOTAL_X : (world.x + 1) / CHUNK_TOTAL_X - 1;
	chunk.y = (world.y >= 0) ? world.y / CHUNK_TOTAL_Y : (world.y + 1) / CHUNK_TOTAL_Y - 1;
	chunk.z = (world.z >= 0) ? world.z / CHUNK_TOTAL_Z : (world.z + 1) / CHUNK_TOTAL_Z - 1;

	return chunk;
}

void world_cord_to_chunk_and_block(WorldCoord world, ChunkCoord *chunk, BlockPos *local)
{
	*chunk = world_coord_to_chunk(world);

	local->x = ((world.x % CHUNK_TOTAL_X) + CHUNK_TOTAL_X) % CHUNK_TOTAL_X;
	local->y = ((world.y % CHUNK_TOTAL_Y) + CHUNK_TOTAL_Y) % CHUNK_TOTAL_Y;
	local->z = ((world.z % CHUNK_TOTAL_Z) + CHUNK_TOTAL_Z) % CHUNK_TOTAL_Z;
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
	VASSERT_MSG(chunk.data != NULL, "Buy more ram for more chunks bozo");

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

Block *chunk_xyz_at(Chunk *chunk, int x, int y, int z)
{
	if (x < 0 || x >= CHUNK_TOTAL_X ||
	    y < 0 || y >= CHUNK_TOTAL_Y ||
	    z < 0 || z >= CHUNK_TOTAL_Z) {
		// VERROR("chunk_xyz_at out of bounds: %i %i %i", x, y, z);
		// print_chunk(chunk);
		return NULL;
	}

	return &chunk->data[CHUNK_INDEX(x, y, z)];
}

#define ARRAY_LEN(array) (sizeof(array) / sizeof(array[0]))

const char *vertexShaderSource = (char *)verts_vert1;
const char *fragmentShaderSource = (char *)frags_frag1;

FaceVertices cube_vertices[] = {
	//BACK
	{
		{ { 0.5f, 0.5f, -0.5f, 1.0f, 1.0f },
		  { 0.5f, -0.5f, -0.5f, 1.0f, 0.0f },
		  { -0.5f, -0.5f, -0.5f, 0.0f, 0.0f },
		  { -0.5f, -0.5f, -0.5f, 0.0f, 0.0f },
		  { -0.5f, 0.5f, -0.5f, 0.0f, 1.0f },
		  { 0.5f, 0.5f, -0.5f, 1.0f, 1.0f } } },
	// FRONT
	{
		{ { -0.5f, -0.5f, 0.5f, 0.0f, 0.0f },
		  { 0.5f, -0.5f, 0.5f, 1.0f, 0.0f },
		  { 0.5f, 0.5f, 0.5f, 1.0f, 1.0f },
		  { 0.5f, 0.5f, 0.5f, 1.0f, 1.0f },
		  { -0.5f, 0.5f, 0.5f, 0.0f, 1.0f },
		  { -0.5f, -0.5f, 0.5f, 0.0f, 0.0f } } },
	// LEFT
	{
		{ { -0.5f, 0.5f, 0.5f, 1.0f, 0.0f },
		  { -0.5f, 0.5f, -0.5f, 1.0f, 1.0f },
		  { -0.5f, -0.5f, -0.5f, 0.0f, 1.0f },
		  { -0.5f, -0.5f, -0.5f, 0.0f, 1.0f },
		  { -0.5f, -0.5f, 0.5f, 0.0f, 0.0f },
		  { -0.5f, 0.5f, 0.5f, 1.0f, 0.0f } } },
	// RIGHT
	{
		{ { 0.5f, -0.5f, -0.5f, 0.0f, 1.0f },
		  { 0.5f, 0.5f, -0.5f, 1.0f, 1.0f },
		  { 0.5f, 0.5f, 0.5f, 1.0f, 0.0f },
		  { 0.5f, 0.5f, 0.5f, 1.0f, 0.0f },
		  { 0.5f, -0.5f, 0.5f, 0.0f, 0.0f },
		  { 0.5f, -0.5f, -0.5f, 0.0f, 1.0f } } },
	// BOTTOM
	{
		{ { -0.5f, -0.5f, -0.5f, 0.0f, 1.0f },
		  { 0.5f, -0.5f, -0.5f, 1.0f, 1.0f },
		  { 0.5f, -0.5f, 0.5f, 1.0f, 0.0f },
		  { 0.5f, -0.5f, 0.5f, 1.0f, 0.0f },
		  { -0.5f, -0.5f, 0.5f, 0.0f, 0.0f },
		  { -0.5f, -0.5f, -0.5f, 0.0f, 1.0f } } },
	// TOP
	{
		{ { 0.5f, 0.5f, 0.5f, 1.0f, 0.0f },
		  { 0.5f, 0.5f, -0.5f, 1.0f, 1.0f },
		  { -0.5f, 0.5f, -0.5f, 0.0f, 1.0f },
		  { -0.5f, 0.5f, -0.5f, 0.0f, 1.0f },
		  { -0.5f, 0.5f, 0.5f, 0.0f, 0.0f },
		  { 0.5f, 0.5f, 0.5f, 1.0f, 0.0f } } }
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

	game_state.camera.yaw += xoffset;
	game_state.camera.pitch += yoffset;

	if (game_state.camera.pitch > 89.0f)
		game_state.camera.pitch = 89.0f;
	if (game_state.camera.pitch < -89.0f)
		game_state.camera.pitch = -89.0f;
	vec3 direction = { 0 };

	direction[0] = cos(glm_rad(game_state.camera.yaw)) * cos(glm_rad(game_state.camera.pitch));
	direction[1] = sin(glm_rad(game_state.camera.pitch));
	direction[2] = sin(glm_rad(game_state.camera.yaw)) * cos(glm_rad(game_state.camera.pitch));
	glm_normalize(direction);
	glm_vec3_copy(direction, game_state.camera.front);
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
void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
	(void)mods;
	if (game_state.paused) {
		if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
			set_paused(window, false);
		}
		if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
			set_paused(window, false);
		}
	} else {
	}
	// if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS) {
	// }
}

void process_input(GLFWwindow *window)
{
	static bool escapeKeyPressedLastFrame = false;

	bool escapeKeyPressedThisFrame = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;

	if (escapeKeyPressedThisFrame && !escapeKeyPressedLastFrame) {
		set_paused(window, !game_state.paused);
	}

	escapeKeyPressedLastFrame = escapeKeyPressedThisFrame;

	if (game_state.paused)
		return;

	const float camera_speed = movement_speed * game_state.delta_time;
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
		game_state.camera.pos[1] += camera_speed;
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
		game_state.camera.pos[1] -= camera_speed;
	}
	static bool sprinting;
	float forward_speed = camera_speed;
	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
		sprinting = true;
	}
	if (sprinting)
		forward_speed *= 2;

	vec3 horizontal_front;
	glm_vec3_copy(game_state.camera.front, horizontal_front);
	horizontal_front[1] = 0.0f;
	glm_normalize(horizontal_front);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		glm_vec3_muladds(horizontal_front, forward_speed, game_state.camera.pos);
	} else {
		sprinting = false;
	}

	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		glm_vec3_mulsubs(horizontal_front, camera_speed, game_state.camera.pos);
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		vec3 right;
		glm_cross(horizontal_front, game_state.camera.up, right);
		glm_normalize(right);
		glm_vec3_mulsubs(right, camera_speed, game_state.camera.pos);
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
		vec3 right;
		glm_cross(horizontal_front, game_state.camera.up, right);
		glm_normalize(right);
		glm_vec3_muladds(right, camera_speed, game_state.camera.pos);
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
	stbi_set_flip_vertically_on_load(true);
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

void chunk_render(Chunk *chunk, ShaderData shader_data)
{
	if (!chunk->up_to_date)
		return;
	if (chunk->vertex_count == 0) {
		// VERROR("This guy again:");
		// print_chunk(chunk);
		return;
	}

	VASSERT_MSG(chunk->contains_blocks, "How did it get here?");
	// VASSERT_MSG(chunk->vertex_count > 0, "How did it get here?");

	chunk->unchanged_render_count++;

	glUseProgram(shader_data.program);
	glBindVertexArray(chunk->VAO);

	mat4 model = { 0 };
	glm_mat4_identity(model);
	vec3 pos = { 0 };
	pos[0] = chunk->coord.x * CHUNK_TOTAL_X;
	pos[1] = chunk->coord.y * CHUNK_TOTAL_Y;
	pos[2] = chunk->coord.z * CHUNK_TOTAL_Z;
	glm_translate(model, pos);

	glUniformMatrix4fv(shader_data.model_loc, 1, GL_FALSE, (const float *)model);

	glDrawArrays(GL_TRIANGLES, 0, chunk->vertex_count);

	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR) {
		printf("OpenGL error: 0x%04X\n", err);
	}
}

void vec3_assign(vec3 v, float x, float y, float z)
{
	v[0] = x;
	v[1] = y;
	v[2] = z;
}

void add_face_to_buffer(TmpChunkVerts *buffer, int x, int y, int z, CubeFace face)
{
	FaceVertices *face_verts = &cube_vertices[(int)face];
	for (size_t i = 0; i < VERTICES_PER_FACE; i++) {
		float px = face_verts->vertices[i][0] + 0.5f + (float)x;
		float py = face_verts->vertices[i][1] + 0.5f + (float)y;
		float pz = face_verts->vertices[i][2] + 0.5f + (float)z;

		float u = face_verts->vertices[i][3];
		float v = face_verts->vertices[i][4];

		vec2 tex_coords = { u, v };

		VASSERT_MSG(buffer->fill + FLOATS_PER_VERTEX < CHUNK_TOTAL_VERTICES, "Vertex buffer overflow!");

		buffer->data[buffer->fill++] = px;
		buffer->data[buffer->fill++] = py;
		buffer->data[buffer->fill++] = pz;
		buffer->data[buffer->fill++] = tex_coords[0]; // u
		buffer->data[buffer->fill++] = tex_coords[1]; // v
	}
}

void add_block_faces_to_buffer(Chunk *chunk, TmpChunkVerts *tmp_chunk_verts, int x, int y, int z)
{
	static const vec3 face_offsets[6] = {
		{ 0, 0, -1 }, // BACK
		{ 0, 0, 1 }, // FRONT
		{ -1, 0, 0 }, // LEFT
		{ 1, 0, 0 }, // RIGHT
		{ 0, -1, 0 }, // BOTTOM
		{ 0, 1, 0 } // TOP
	};

	for (CubeFace f = FACE_BACK; f < FACES_PER_CUBE; f++) {
		int nx = x + face_offsets[f][0];
		int ny = y + face_offsets[f][1];
		int nz = z + face_offsets[f][2];

		Block *neighbor = chunk_xyz_at(chunk, nx, ny, nz);
		if (neighbor == NULL) {
			add_face_to_buffer(tmp_chunk_verts, x, y, z, f);
		} else if (!neighbor->obstructing) {
			add_face_to_buffer(tmp_chunk_verts, x, y, z, f);
		}
	}
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
				Block *block = chunk_xyz_at(chunk, x, y, z);
				// chunk->data[CHUNK_INDEX(x, y, z)];
				if (!block->obstructing)
					continue;

				add_block_faces_to_buffer(chunk, tmp_chunk_verts, x, y, z);
			}
		}
	}
	glBindVertexArray(chunk->VAO);
	glBindBuffer(GL_ARRAY_BUFFER, chunk->VBO);
	glBufferData(GL_ARRAY_BUFFER,
		     tmp_chunk_verts->fill * sizeof(float),
		     tmp_chunk_verts->data,
		     GL_DYNAMIC_DRAW);
	chunk->vertex_count = tmp_chunk_verts->fill / FLOATS_PER_VERTEX;

	if (chunk->vertex_count == 0) {
		VERROR("This guy:");
		print_chunk(chunk);
	}
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
	VINFO("unloaded: %s", chunk->unloaded ? "true" : "false");
	VINFO("vertex_count: %i", chunk->vertex_count);
}

bool chunk_caught = false;
Chunk *chunk_to_catch = NULL;
void chunk_generate_terrain(Chunk *chunk)
{
	VASSERT(chunk->data != NULL);
	// VINFO("Generating terrain: %i %i %i", chunk->coord.x, chunk->coord.y, chunk->coord.z);

	if (chunk->coord.y == 0) {
		for (size_t z = 0; z < CHUNK_TOTAL_Z; z++)
			for (size_t y = 0; y < CHUNK_TOTAL_Y; y++)
				for (size_t x = 0; x < CHUNK_TOTAL_X; x++) {
					if (y < 5) {
						chunk->contains_blocks = true;
						chunk->data[CHUNK_INDEX(x, y, z)].type = BLOCKTYPE_GRASS;
						chunk->data[CHUNK_INDEX(x, y, z)].obstructing = true;
					}
				}
	}
	chunk->terrain_generated = true;
}

void chunk_load(Chunk *chunk, ChunkCoord coord)
{
	chunk_init(chunk);
	chunk->last_used = time(NULL);
	chunk->coord = coord;
	memset(chunk->data, 0, sizeof(Block) * CHUNK_TOTAL_BLOCKS);
	chunk_generate_terrain(chunk);
}

bool pool_find_chunk_coord(ChunkPool *pool, ChunkCoord coord, size_t *index)
{
	for (size_t i = 0; i < pool->lvl; i++) {
		Chunk *pc = &pool->chunk[i];
		if (pc->coord.x == coord.x &&
		    pc->coord.y == coord.y &&
		    pc->coord.z == coord.z) {
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

void chunk_clear_metadata(Chunk *chunk)
{
	Chunk tmp = { 0 };
	tmp.data = chunk->data;
	tmp.VAO = chunk->VAO;
	tmp.VBO = chunk->VBO;
	memset(chunk, 0, sizeof(*chunk));
	chunk->VAO = tmp.VAO;
	chunk->VBO = tmp.VBO;
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
size_t pool_append(ChunkPool *pool, ChunkCoord coord)
{
	VASSERT(pool->lvl < pool->cap);
	chunk_clear_metadata(&pool->chunk[pool->lvl]);
	chunk_load(&pool->chunk[pool->lvl], coord);
	return pool->lvl++;
}

void pool_replace(ChunkPool *pool, ChunkCoord coord, size_t index)
{
	VASSERT(pool->lvl > index);
	chunk_clear_metadata(&pool->chunk[index]);
	chunk_load(&pool->chunk[index], coord);
}

size_t pool_add(ChunkPool *pool, ChunkCoord coord)
{
	if (pool->lvl < pool->cap) {
		return pool_append(pool, coord);
	} else {
		size_t index = pool_replaceable_index(pool);
		pool_replace(pool, coord, index);
		return index;
	}
}

void pool_remove(ChunkPool *pool, size_t index)
{
	VASSERT(pool->lvl > index);
	pool->lvl--;
	pool->chunk[index] = pool->chunk[pool->lvl];
}

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

size_t poolunloaded_add(ChunkPool *pool, Chunk chunk)
{
	if (pool->unloaded.lvl < pool->unloaded.cap) {
		return poolunloaded_append(&pool->unloaded, chunk);
	} else {
		size_t index = poolunloaded_replaceable_index(&pool->unloaded);
		poolunloaded_replace(&pool->unloaded, chunk, index);
		return index;
	}
}

size_t pool_unload(ChunkPool *pool, size_t index)
{
	VASSERT(pool->lvl > index);
	VASSERT_MSG(pool->chunk[index].up_to_date || !pool->chunk[index].contains_blocks, "There was no reason to unload this");

	Chunk tmp = pool->chunk[index];

	tmp.data = NULL;
	tmp.unloaded = true;

	size_t unloaded_index = poolunloaded_add(pool, tmp);

	pool->lvl--;
	pool->chunk[index] = pool->chunk[pool->lvl];
	// print_chunk(&pool->chunk[index]);
	return unloaded_index;
}
// TODO: add some kind of chunk activeness scoring system
size_t pool_find_unloadable(ChunkPool *pool)
{
	size_t count = 0;
	for (size_t i = 0; i < pool->lvl; i++) {
		if (pool->chunk[i].unchanged_render_count > 5) {
			VASSERT(pool->chunk[i].terrain_generated);
			pool_unload(pool, i);
			count++;
		} else if (pool->chunk[i].contains_blocks == false) {
			pool_unload(pool, i);
			count++;
		}
	}
	return count;
}

bool pool_load_relaxed(ChunkPool *pool, ChunkCoord coord, size_t *index)
{
	if (poolunloaded_find_chunk_coord(&pool->unloaded, coord, index)) {
		return true;
	}
	if (pool_find_chunk_coord(pool, coord, index)) {
		return true;
	}
	if (pool->lvl < pool->cap) {
		*index = pool_append(pool, coord);
		return true;
	}

	return false;
}

size_t pool_load(ChunkPool *pool, ChunkCoord coord)
{
	size_t index = 0;

	if (poolunloaded_find_chunk_coord(&pool->unloaded, coord, &index)) {
		return index;
	}
	if (pool_find_chunk_coord(pool, coord, &index)) {
		return index;
	}

	return pool_add(pool, coord);
}

void world_init(World *world)
{
	world->pool.cap = 128;
	world->pool.chunk = calloc(world->pool.cap, sizeof(Chunk));
	VASSERT(world->pool.chunk != NULL);
	// world->pool.unloaded.cap = 512;
	world->pool.unloaded.cap = 2048;
	world->pool.unloaded.lvl = 0;
	world->pool.unloaded.chunk = calloc(world->pool.unloaded.cap, sizeof(Chunk));
	VASSERT(world->pool.unloaded.chunk != NULL);

	pool_reserve(&world->pool, world->pool.cap);

	size_t pl = 0;

	for (int z = -RENDER_DISTANCE_Z; z <= RENDER_DISTANCE_Z; z++) {
		for (int y = -RENDER_DISTANCE_Y; y <= RENDER_DISTANCE_Y; y++) {
			for (int x = -RENDER_DISTANCE_X; x <= RENDER_DISTANCE_X; x++) {
				if (pl == world->pool.lvl)
					goto done;

				ChunkCoord coord = { x, y, z };

				chunk_load(&world->pool.chunk[pl], coord);
				pl++;
			}
		}
	}

	// pool_load(&world->pool, (ChunkCoord){ 0, 0, 0 });
done:
	return;
}

void world_render(World *world, TmpChunkVerts *tmp_chunk_verts, ShaderData shader_data)
{
	for (size_t i = 0; i < world->pool.unloaded.lvl; i++) {
		chunk_render(&world->pool.unloaded.chunk[i], shader_data);
	}
	for (size_t i = 0; i < world->pool.lvl; i++) {
		VASSERT(world->pool.chunk[i].data != NULL);
		chunk_generate_mesh(&world->pool.chunk[i], tmp_chunk_verts);
		chunk_render(&world->pool.chunk[i], shader_data);
	}
}
void print_poolunloaded(ChunkPoolUnloaded *pool)
{
	VINFO("PoolUnloaded: %p", pool);
	VINFO("lvl: %zu", pool->lvl);
	VINFO("cap: %zu", pool->cap);
}

void world_update(World *world)
{
	size_t unloadable = pool_find_unloadable(&world->pool);
	// VINFO("Unloadable: %zu", unloadable);
	// VINFO("==========================================");
	// print_poolunloaded(&world->pool.unloaded);

	// if (world->pool.lvl < world->pool.cap) {
	// TODO: something better (or separate thread)
	ChunkCoord new_chunk = world_coord_to_chunk(world->player.pos);
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

				// VINFO("Trying to load: %i %i %i", coord.x, coord.y, coord.z);
				size_t index;
				pool_load_relaxed(&world->pool, coord, &index);
				// VINFO("Loaded index: %i", index);
			}
		}
	}
	// }
}
void player_update(Player *player, Camera *camera)
{
	player->pos.x = floorf(camera->pos[0]);
	player->pos.y = floorf(camera->pos[1]);
	player->pos.z = floorf(camera->pos[2]);
}

void print_pool(ChunkPool *pool)
{
	VINFO("Pool: %p", pool);
	VINFO("lvl: %zu", pool->lvl);
	VINFO("cap: %zu", pool->cap);
	for (size_t i = 0; i < pool->lvl; i++) {
		VINFO("Chunk index: %zu", i);
		print_chunk(&pool->chunk[i]);
	}
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
	glm_vec3_add(game_state->camera.pos, game_state->camera.front, camera_pos_plus_front);
	glm_lookat(game_state->camera.pos, camera_pos_plus_front, game_state->camera.up, view);

	mat4 projection = { 0 };
	float aspect = (float)window.width / (float)window.height;
	glm_perspective(glm_rad(game_state->camera.fov), aspect, 0.1f, 100.0f, projection);

	glClearColor(0.39f, 0.58f, 0.92f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUniformMatrix4fv(shader_data.view_loc, 1, GL_FALSE, (const float *)view);

	glUniformMatrix4fv(shader_data.projection_loc, 1, GL_FALSE, (const float *)projection);

	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR) {
		VERROR("OpenGL error: 0x%04X", err);
	}

	// Chunk chunk = chunk_callocrash();
	// chunk_load(&chunk, (ChunkCoord){ 5, 0, 0 });
	// chunk_generate_terrain(&chunk);
	// chunk_generate_mesh(&chunk, &tmp_chunk_verts);
	// chunk_render(&chunk, shader_data);
	// chunk_free(&chunk);

	// print_pool(&game_state->world.pool);
	world_render(&game_state->world, &tmp_chunk_verts, shader_data);
	// VWARN("not drawing world");

	glfwSwapBuffers(window.glfw);
	glfwPollEvents();
}

int main()
{
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	Image imageData = { 0 };
	if (!load_image(&imageData, "assets/grass_side.png"))
		exit(1);
	// print_image_info(&imageData);
	if (!glfwInit()) {
		exit(1);
	}

	WindowData window = { 0 };
	window.width = 1080;
	window.height = 800;
	window.glfw = glfwCreateWindow(window.width, window.height, "Hello GLFW", NULL, NULL);
	if (!window.glfw) {
		glfwTerminate();
		exit(1);
	}

	glfwMakeContextCurrent(window.glfw);
	if (!load_gl_functions())
		exit(1);

	glEnable(GL_DEPTH_TEST);

	glfwSetCursorPosCallback(window.glfw, mouse_callback);

	glfwSetFramebufferSizeCallback(window.glfw, framebuffer_size_callback);

	unsigned int texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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

	game_state.camera = (Camera){
		.fov = 90.0f,
		.yaw = -00.0f,
		.pitch = 0.0f,
		.pos = { 0.0f, 10.0f, 0.0f },
		.up = { 0.0f, 1.0f, 0.0f },
		.front = { 0.0f, 0.0f, -1.0f },
	};

	// glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); //wireframe

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	// glfwSwapInterval(0); // no vsync

	world_init(&game_state.world);

	glUseProgram(shader_data.program);
	glUniform1i(glGetUniformLocation(shader_data.program, "texture1"), 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);

	glfwSetInputMode(window.glfw, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetWindowFocusCallback(window.glfw, window_focus_callback);
	glfwSetMouseButtonCallback(window.glfw, mouse_button_callback);

	static float tmp_chunk_verts_data[CHUNK_TOTAL_VERTICES];
	TmpChunkVerts tmp_chunk_verts = { .data = tmp_chunk_verts_data, .fill = 0 };

	{
		shader_data.view_loc = glGetUniformLocation(shader_data.program, "view");
		shader_data.projection_loc = glGetUniformLocation(shader_data.program, "projection");
		shader_data.model_loc = glGetUniformLocation(shader_data.program, "model");
	}

	while (!glfwWindowShouldClose(window.glfw)) {
		float current_frame = glfwGetTime();
		game_state.delta_time = current_frame - game_state.last_frame;
		game_state.last_frame = current_frame;

		process_input(window.glfw);

		if (game_state.paused) {
			glfwWaitEvents();
			continue;
		}

		player_update(&game_state.world.player, &game_state.camera);
		world_update(&game_state.world);

		render(&game_state, window, shader_data, tmp_chunk_verts);
	}

	glfwDestroyWindow(window.glfw);
	glfwTerminate();
}
