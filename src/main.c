#include <stdio.h>
#include <stdlib.h>

#define MULTILINE_STR(...) #__VA_ARGS__

#include "loadopengl.c"

float vertices[] = {
	-0.5f, -0.5f, 0.0f,
	0.5f, -0.5f, 0.0f,
	0.0f, 0.5f, 0.0f
};

const char *vertexShaderSource = MULTILINE_STR(#version 330 core\n
						       layout(location = 0) in vec3 aPos;\n void main()\n {
							       \n
								       gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
						       });
const char *fragmentShaderSource = MULTILINE_STR(#version 330 core\n
							 out vec4 FragColor;\n void main()\n {
								 \n
									 FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);
								 \n
							 });

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
	(void)window;
	glViewport(0, 0, width, height);
}

void process_input(GLFWwindow *window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
}

bool verify_shader(uint shader)
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
bool verify_program(uint program)
{
	int success;
	char infoLog[512];
	glGetProgramiv(program, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(program, 512, NULL, infoLog);
		fprintf(stderr, "Error: %s", infoLog);
		return false;
	}
	return true;
}

int main()
{
	const int width = 800;
	const int height = 600;
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

	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	unsigned int VBO;
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);
	if (!verify_shader(vertexShader))
		exit(1);

	unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);
	if (!verify_shader(fragmentShader))
		exit(1);

	unsigned int shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);
	verify_program(shaderProgram);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);

	glUseProgram(shaderProgram);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	unsigned int VAO;
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);

	while (!glfwWindowShouldClose(window)) {
		process_input(window);

		glClearColor(0.39f, 0.58f, 0.92f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		glUseProgram(shaderProgram);
		glBindVertexArray(VAO);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwDestroyWindow(window);
	glfwTerminate();
}
