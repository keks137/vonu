#include <GL/glcorearb.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#define STB_IMAGE_IMPLEMENTATION
#include "../vendor/stb_image.h"
#include "../vendor/cglm/cglm.h"

#include "loadopengl.c"

#include "verts/vert1.c"
#include "frags/frag1.c"

#define ARRAY_LEN(array) (sizeof(array) / sizeof(array[0]))

float fov = 45.0f;
float yaw = -90.0f;
float pitch = 0.0f;

const char *vertexShaderSource = (char *)verts_vert1;
const char *fragmentShaderSource = (char *)frags_frag1;

typedef struct {
	unsigned char *data;
	size_t width;
	size_t height;
	size_t n_chan;

} Image;
// float vertices[] = {
// 	// positions          // colors           // texture coords
// 	0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, // top right
// 	0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, // bottom right
// 	-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom left
// 	-0.5f, 0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f // top left
// };

vec3 camera_pos = { 0.0f, 0.0f, 3.0f };
vec3 camera_up = { 0.0f, 1.0f, 0.0f };
vec3 camera_front = { 0.0f, 0.0f, -1.0f };

float delta_time = 0.0f;
float last_frame = 0.0f;

vec3 cubePositions[] = {
	{ 0.0f, 0.0f, 0.0f },
	{ 2.0f, 5.0f, -15.0f },
	{ -1.5f, -2.2f, -2.5f },
	{ -3.8f, -2.0f, -12.3f },
	{ 2.4f, -0.4f, -3.5f },
	{ -1.7f, 3.0f, -7.5f },
	{ 1.3f, -2.0f, -2.5f },
	{ 1.5f, 2.0f, -2.5f },
	{ 1.5f, 0.2f, -1.5f },
	{ -1.3f, 1.0f, -1.5f }
};

float vertices[] = {
	-0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
	0.5f, -0.5f, -0.5f, 1.0f, 0.0f,
	0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
	0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
	-0.5f, 0.5f, -0.5f, 0.0f, 1.0f,
	-0.5f, -0.5f, -0.5f, 0.0f, 0.0f,

	-0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
	0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
	0.5f, 0.5f, 0.5f, 1.0f, 1.0f,
	0.5f, 0.5f, 0.5f, 1.0f, 1.0f,
	-0.5f, 0.5f, 0.5f, 0.0f, 1.0f,
	-0.5f, -0.5f, 0.5f, 0.0f, 0.0f,

	-0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
	-0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
	-0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
	-0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
	-0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
	-0.5f, 0.5f, 0.5f, 1.0f, 0.0f,

	0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
	0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
	0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
	0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
	0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
	0.5f, 0.5f, 0.5f, 1.0f, 0.0f,

	-0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
	0.5f, -0.5f, -0.5f, 1.0f, 1.0f,
	0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
	0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
	-0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
	-0.5f, -0.5f, -0.5f, 0.0f, 1.0f,

	-0.5f, 0.5f, -0.5f, 0.0f, 1.0f,
	0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
	0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
	0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
	-0.5f, 0.5f, 0.5f, 0.0f, 0.0f,
	-0.5f, 0.5f, -0.5f, 0.0f, 1.0f
};

unsigned int indices[] = {
	0, 1, 3,
	1, 2, 3
};

float last_x = 400;
float last_y = 300;
void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
	static bool first_mouse = true;
	if (first_mouse) // initially set to true
	{
		last_x = xpos;
		last_y = ypos;
		first_mouse = false;
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

void process_input(GLFWwindow *window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
	const float camera_speed = 2.5f * delta_time;
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		glm_vec3_muladds(camera_front, camera_speed, camera_pos);
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		glm_vec3_mulsubs(camera_front, camera_speed, camera_pos);
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		vec3 right;
		glm_cross(camera_front, camera_up, right);
		glm_normalize(right);
		glm_vec3_mulsubs(right, camera_speed, camera_pos);
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
		vec3 right;
		glm_cross(camera_front, camera_up, right);
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
		fprintf(stderr, "Error: %s", infoLog);
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
		fprintf(stderr, "Error: %s", infoLog);
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

void vao_attributes_no_color(unsigned int VAO)
{
	glBindVertexArray(VAO);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
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

int main()
{
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	const int width = 800;
	const int height = 600;

	Image imageData = { 0 };
	if (!load_image(&imageData, "assets/container.jpg"))
		exit(1);

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

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
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

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, imageData.width, imageData.height, 0, GL_RGB, GL_UNSIGNED_BYTE, imageData.data);
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
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	vao_attributes_no_color(VAO);

	// glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); //wireframe

	// glfwSwapInterval(0); // no vsync

	mat4 view = { 0 };

	vec3 up = { 0.0f, 1.0f, 0.0f };
	// vec3 camera_right = { 0 };
	// glm_cross(up, camera_opposite_direction, camera_right);
	// glm_normalize(camera_right);

	// glm_cross(camera_opposite_direction, camera_right, camera_up);

	while (!glfwWindowShouldClose(window)) {
		float current_frame = glfwGetTime();
		delta_time = current_frame - last_frame;
		last_frame = current_frame;

		// mat4 trans = { 0 };
		// glm_mat4_identity(trans);

		// glm_translate(trans, (vec3){ 0.5f, -0.5f, 0.0f });
		// glm_rotate(trans, (float)glfwGetTime(), (vec3){ 0.0f, 0.0f, 1.0f });

		// glm_mat4_identity(view);
		// glm_translate(view, (vec3){ 0.0f, 0.0f, -5.5f });

		const float radius = 10.0f;
		mat4 view;

		vec3 camera_pos_plus_front;
		glm_vec3_add(camera_pos, camera_front, camera_pos_plus_front);
		glm_lookat(camera_pos, camera_pos_plus_front, camera_up, view);

		// glm_lookat((vec3){ 0.0f, 0.0f, 3.0f },
		// 	   (vec3){ 0.0f, 0.0f, 0.0f },
		// 	   (vec3){ 0.0f, 1.0f, 0.0f },
		// 	   view);

		mat4 projection = { 0 };
		float aspect = (float)width / (float)height;
		glm_perspective(glm_rad(fov), aspect, 0.1f, 100.0f, projection);

		process_input(window);

		glClearColor(0.39f, 0.58f, 0.92f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(shaderProgram);

		// unsigned int transformLoc = glGetUniformLocation(shaderProgram, "transform");
		// glUniformMatrix4fv(transformLoc, 1, GL_FALSE, (const float *)trans);

		int viewLoc = glGetUniformLocation(shaderProgram, "view");
		glUniformMatrix4fv(viewLoc, 1, GL_FALSE, (const float *)view);

		int projectionLoc = glGetUniformLocation(shaderProgram, "projection");
		glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, (const float *)projection);

		GLenum err;
		while ((err = glGetError()) != GL_NO_ERROR) {
			fprintf(stderr, "OpenGL error: 0x%04X\n", err);
		}

		glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
		glActiveTexture(GL_TEXTURE0);

		glBindTexture(GL_TEXTURE_2D, texture);
		glBindVertexArray(VAO);
		// glDrawArrays(GL_TRIANGLES, 0, 3);
		// glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		glBindVertexArray(VAO);
		for (size_t i = 0; i < ARRAY_LEN(cubePositions); i++) {
			mat4 model = { 0 };
			glm_mat4_identity(model);
			glm_translate(model, cubePositions[i]);

			float angle = 20.0f * i;
			glm_rotate(model, glm_rad(angle), (vec3){ 1.0f, 0.3f, 0.5f });

			int modelLoc = glGetUniformLocation(shaderProgram, "model");
			glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const float *)model);
			glDrawArrays(GL_TRIANGLES, 0, 36);
		}

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwDestroyWindow(window);
	glfwTerminate();
}
