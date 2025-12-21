#include <GL/glcorearb.h>
#include <assert.h>
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

#include "verts/vert1.c"
#include "frags/frag1.c"

typedef enum {
	BLOCKTYPE_AIR,
	BLOCKTYPE_GRASS,
	BLOCKTYPE_STONE,
} BLOCKTYPE;

bool paused = false;

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

static const vec3 face_offsets[6] = {
	{ 0, 0, -1 }, // BACK
	{ 0, 0, 1 }, // FRONT
	{ -1, 0, 0 }, // LEFT
	{ 1, 0, 0 }, // RIGHT
	{ 0, -1, 0 }, // BOTTOM
	{ 0, 1, 0 } // TOP
};

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
	memset(tmp_chunk_verts->data, 0, sizeof(float) * tmp_chunk_verts->fill);
	tmp_chunk_verts->fill = 0;
}

const size_t CHUNK_STRIDE_Y = (CHUNK_TOTAL_X);
const size_t CHUNK_STRIDE_Z = (CHUNK_TOTAL_X * CHUNK_TOTAL_Y);

#define CHUNK_INDEX(x, y, z) ((x) + (y) * CHUNK_STRIDE_Y + (z) * CHUNK_STRIDE_Z)

float movement_speed = 5.0f;
typedef struct {
	BLOCKTYPE type;
	bool obstructing;
} Block;
typedef struct {
	size_t x;
	size_t y;
	size_t z;
} BlockPos;

typedef struct {
	Block *data;
	vec3 pos;
	unsigned int VAO;
	unsigned int VBO;
	size_t vertex_count;
	bool up_to_date;
} Chunk;

void vao_attributes_no_color(unsigned int VAO)
{
	glBindVertexArray(VAO);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
}

Chunk chunk_callocrash()
{
	Chunk chunk = { 0 };
	chunk.data = calloc(CHUNK_TOTAL_BLOCKS, sizeof(Block));
	VASSERT_MSG(chunk.data != NULL, "Buy more ram for more chunks bozo");
	glGenVertexArrays(1, &chunk.VAO);
	glGenBuffers(1, &chunk.VBO);

	glBindVertexArray(chunk.VAO);
	glBindBuffer(GL_ARRAY_BUFFER, chunk.VBO);
	vao_attributes_no_color(chunk.VAO);

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
	    z < 0 || z >= CHUNK_TOTAL_Z)
		return NULL;

	return &chunk->data[CHUNK_INDEX(x, y, z)];
}

#define ARRAY_LEN(array) (sizeof(array) / sizeof(array[0]))

float fov = 90.0f;
float yaw = -90.0f;
float pitch = 0.0f;

const char *vertexShaderSource = (char *)verts_vert1;
const char *fragmentShaderSource = (char *)frags_frag1;

// float vertices[] = {
// 	// positions          // colors           // texture coords
// 	0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, // top right
// 	0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, // bottom right
// 	-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom left
// 	-0.5f, 0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f // top left
// };

vec3 camera_pos = { 0.0f, 0.0f, 0.0f };
vec3 camera_up = { 0.0f, 1.0f, 0.0f };
vec3 camera_front = { 0.0f, 0.0f, -1.0f };

float delta_time = 0.0f;
float last_frame = 0.0f;

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
	if (paused)
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

	yaw += xoffset;
	pitch += yoffset;

	if (pitch > 89.0f)
		pitch = 89.0f;
	if (pitch < -89.0f)
		pitch = -89.0f;
	vec3 direction = { 0 };

	direction[0] = cos(glm_rad(yaw)) * cos(glm_rad(pitch));
	direction[1] = sin(glm_rad(pitch));
	direction[2] = sin(glm_rad(yaw)) * cos(glm_rad(pitch));
	glm_normalize(direction);
	glm_vec3_copy(direction, camera_front);
}
void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
	(void)window;
	glViewport(0, 0, width, height);
}

void set_paused(GLFWwindow *window, bool val)
{
	paused = val;
	if (paused) {
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
	if (paused) {
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
		set_paused(window, !paused);
	}

	escapeKeyPressedLastFrame = escapeKeyPressedThisFrame;

	if (paused)
		return;

	const float camera_speed = movement_speed * delta_time;
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
		camera_pos[1] += camera_speed;
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
		camera_pos[1] -= camera_speed;
	}
	static bool sprinting;
	float forward_speed = camera_speed;
	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
		sprinting = true;
	}
	if (sprinting)
		forward_speed *= 2;

	vec3 horizontal_front;
	glm_vec3_copy(camera_front, horizontal_front);
	horizontal_front[1] = 0.0f;
	glm_normalize(horizontal_front);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		glm_vec3_muladds(horizontal_front, forward_speed, camera_pos);
	} else {
		sprinting = false;
	}

	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		glm_vec3_mulsubs(horizontal_front, camera_speed, camera_pos);
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		vec3 right;
		glm_cross(horizontal_front, camera_up, right);
		glm_normalize(right);
		glm_vec3_mulsubs(right, camera_speed, camera_pos);
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
		vec3 right;
		glm_cross(horizontal_front, camera_up, right);
		glm_normalize(right);
		glm_vec3_muladds(right, camera_speed, camera_pos);
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
	int width, height, nrChannels;
	stbi_set_flip_vertically_on_load(true);
	unsigned char *data = stbi_load(path, &width, &height, &nrChannels, 0);
	img->width = width;
	img->height = height;
	img->n_chan = nrChannels;
	img->data = data;
	if (data == NULL)
		return false;

	return true;
}

void vao_attributes_color(unsigned int VAO)
{
	glBindVertexArray(VAO);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);
}

void print_image_info(Image *image)
{
	printf("height: %zu\n", image->height);
	printf("width: %zu\n", image->width);
	printf("n_chan: %zu\n", image->n_chan);
}

void chunk_render(Chunk *chunk, unsigned int shaderProgram)
{
	if (chunk->vertex_count == 0 || !chunk->up_to_date)
		return;

	glUseProgram(shaderProgram);
	glBindVertexArray(chunk->VAO);

	mat4 model = { 0 };
	glm_mat4_identity(model);
	glm_translate(model, chunk->pos);

	int modelLoc = glGetUniformLocation(shaderProgram, "model");
	glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const float *)model);

	glDrawArrays(GL_TRIANGLES, 0, chunk->vertex_count);
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

// void add_block_faces_to_buffer(Chunk *chunk, TmpChunkVerts *tmp_chunk_verts, int x, int y, int z, CubeFace face, BLOCKTYPE type)
// {
// 	// Check each direction for occlusion
// 	CubeFace faces_to_add[6];
// 	int face_count = 0;
//
//
// 	// Simple neighbor checks (would need bounds checking)
// 	if (y == CHUNK_TOTAL_Y - 1 || chunk_xyz_at(chunk, x, y + 1, z)->type == BLOCKTYPE_AIR)
// 		faces_to_add[face_count++] = FACE_TOP;
// 	// ... check other 5 faces
//
// 	for (int i = 0; i < face_count; i++) {
// 		add_face_to_buffer(tmp_chunk_verts, x, y, z, faces_to_add[i], type);
// 	}
// }

void chunk_generate_mesh(Chunk *chunk, TmpChunkVerts *tmp_chunk_verts)
{
	if (chunk->up_to_date)
		return;
	clear_tmp_chunk_verts(tmp_chunk_verts);
	for (size_t z = 0; z < CHUNK_TOTAL_Z; z++) {
		for (size_t y = 0; y < CHUNK_TOTAL_Y; y++) {
			for (size_t x = 0; x < CHUNK_TOTAL_X; x++) {
				Block *block = chunk_xyz_at(chunk, x, y, z);
				if (!block->obstructing)
					continue;

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
		}
	}
	glBindVertexArray(chunk->VAO);
	glBindBuffer(GL_ARRAY_BUFFER, chunk->VBO);
	glBufferData(GL_ARRAY_BUFFER,
		     tmp_chunk_verts->fill * sizeof(float),
		     tmp_chunk_verts->data,
		     GL_STATIC_DRAW);
	chunk->vertex_count = tmp_chunk_verts->fill / FLOATS_PER_VERTEX;
	chunk->up_to_date = true;
}

int main()
{
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	const int width = 800;
	const int height = 600;

	Image imageData = { 0 };
	if (!load_image(&imageData, "assets/grass.png"))
		exit(1);
	print_image_info(&imageData);

	if (!glfwInit()) {
		exit(1);
	}

	GLFWwindow *window = glfwCreateWindow(width, height, "Hello GLFW", NULL, NULL);
	if (!window) {
		glfwTerminate();
		exit(1);
	}

	glfwMakeContextCurrent(window);
	if (!load_gl_functions())
		exit(1);

	glEnable(GL_DEPTH_TEST);

	glfwSetCursorPosCallback(window, mouse_callback);

	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

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

	unsigned int shaderProgram;
	printf("==vert==:\n%s\n========\n", vertexShaderSource);
	printf("==frag==:\n%s\n========\n", fragmentShaderSource);
	if (!create_shader_program(&shaderProgram, vertexShaderSource, fragmentShaderSource))
		exit(1);

	unsigned int VBO, VAO, EBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	vao_attributes_no_color(VAO);

	// glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); //wireframe

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	// glfwSwapInterval(0); // no vsync

	Chunk chunk = chunk_callocrash();
	vec3_assign(chunk.pos, 0.0f, 0.0f, 0.0f);
	Chunk chunk2 = chunk_callocrash();
	vec3_assign(chunk2.pos, 20.0f, 20.0f, 20.0f);
	for (size_t i = 0; i < CHUNK_TOTAL_BLOCKS; i++) {
		BlockPos block = chunk_index_to_blockpos(i);
		if (block.y <= 5) {
			chunk.data[i].type = BLOCKTYPE_GRASS;
			chunk.data[i].obstructing = true;
		}
	}
	for (size_t i = 0; i < 400; i++) {
		chunk2.data[i].type = BLOCKTYPE_GRASS;
		chunk2.data[i].obstructing = true;
	}

	glUseProgram(shaderProgram);
	glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetWindowFocusCallback(window, window_focus_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);

	static float tmp_chunk_verts_data[CHUNK_TOTAL_VERTICES];
	TmpChunkVerts tmp_chunk_verts = { .data = tmp_chunk_verts_data, .fill = 0 };

	while (!glfwWindowShouldClose(window)) {
		float current_frame = glfwGetTime();
		delta_time = current_frame - last_frame;
		last_frame = current_frame;

		process_input(window);

		if (paused) {
			glfwWaitEvents();
			continue;
		}

		mat4 view;

		vec3 camera_pos_plus_front;
		glm_vec3_add(camera_pos, camera_front, camera_pos_plus_front);
		glm_lookat(camera_pos, camera_pos_plus_front, camera_up, view);

		mat4 projection = { 0 };
		float aspect = (float)width / (float)height;
		glm_perspective(glm_rad(fov), aspect, 0.1f, 100.0f, projection);

		glClearColor(0.39f, 0.58f, 0.92f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		int viewLoc = glGetUniformLocation(shaderProgram, "view");
		glUniformMatrix4fv(viewLoc, 1, GL_FALSE, (const float *)view);

		int projectionLoc = glGetUniformLocation(shaderProgram, "projection");
		glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, (const float *)projection);

		GLenum err;
		while ((err = glGetError()) != GL_NO_ERROR) {
			VERROR("OpenGL error: 0x%04X", err);
		}

		glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
		glActiveTexture(GL_TEXTURE0);

		glBindTexture(GL_TEXTURE_2D, texture);

		chunk_generate_mesh(&chunk, &tmp_chunk_verts);
		chunk_render(&chunk, shaderProgram);
		chunk_generate_mesh(&chunk2, &tmp_chunk_verts);
		chunk_render(&chunk2, shaderProgram);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwDestroyWindow(window);
	glfwTerminate();
}
