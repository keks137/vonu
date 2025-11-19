#include <GLFW/glfw3.h>
#include <GL/glcorearb.h>
#include <stdbool.h>
PFNGLVIEWPORTPROC glViewport;
PFNGLCLEARCOLORPROC glClearColor;
PFNGLCLEARPROC glClear;
bool load_gl_functions()
{
	glClear = (PFNGLCLEARPROC)glfwGetProcAddress("glClear");
	glClearColor = (PFNGLCLEARCOLORPROC)glfwGetProcAddress("glClearColor");
	glViewport = (PFNGLVIEWPORTPROC)glfwGetProcAddress("glViewport");

	return true && glViewport;
}
