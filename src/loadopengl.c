#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/glcorearb.h>
#include <stdbool.h>
PFNGLVIEWPORTPROC glViewport;
PFNGLCLEARCOLORPROC glClearColor;
PFNGLCLEARPROC glClear;
PFNGLGENBUFFERSPROC glGenBuffers;
PFNGLBINDBUFFERPROC glBindBuffer;
PFNGLBUFFERDATAPROC glBufferData;
PFNGLCREATESHADERPROC glCreateShader;
PFNGLSHADERSOURCEPROC glShaderSource;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLGETSHADERIVPROC glGetShaderiv;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLATTACHSHADERPROC glAttachShader;
PFNGLLINKPROGRAMPROC glLinkProgram;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
PFNGLGETPROGRAMIVPROC glGetProgramiv;
PFNGLUSEPROGRAMPROC glUseProgram;
PFNGLDELETESHADERPROC glDeleteShader;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
PFNGLDRAWARRAYSPROC glDrawArrays;
PFNGLDRAWELEMENTSPROC glDrawElements;
PFNGLPOLYGONMODEPROC glPolygonMode;
PFNGLGENTEXTURESPROC glGenTextures;
PFNGLBINDTEXTUREPROC glBindTexture;
PFNGLTEXIMAGE2DPROC glTexImage2D;
PFNGLGENERATEMIPMAPPROC glGenerateMipmap;
PFNGLTEXPARAMETERIPROC glTexParameteri;
bool load_gl_functions()
{
glTexParameteri = (PFNGLTEXPARAMETERIPROC)glfwGetProcAddress("glTexParameteri");
glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC)glfwGetProcAddress("glGenerateMipmap");
glTexImage2D = (PFNGLTEXIMAGE2DPROC)glfwGetProcAddress("glTexImage2D");
glBindTexture = (PFNGLBINDTEXTUREPROC)glfwGetProcAddress("glBindTexture");
glGenTextures = (PFNGLGENTEXTURESPROC)glfwGetProcAddress("glGenTextures");
glPolygonMode = (PFNGLPOLYGONMODEPROC)glfwGetProcAddress("glPolygonMode");
glDrawElements = (PFNGLDRAWELEMENTSPROC)glfwGetProcAddress("glDrawElements");
glDrawArrays = (PFNGLDRAWARRAYSPROC)glfwGetProcAddress("glDrawArrays");
glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)glfwGetProcAddress("glBindVertexArray");
	glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)glfwGetProcAddress("glGenVertexArrays");
	glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)glfwGetProcAddress("glEnableVertexAttribArray");
	glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)glfwGetProcAddress("glVertexAttribPointer");
	glDeleteShader = (PFNGLDELETESHADERPROC)glfwGetProcAddress("glDeleteShader");
	glUseProgram = (PFNGLUSEPROGRAMPROC)glfwGetProcAddress("glUseProgram");
	glGetProgramiv = (PFNGLGETPROGRAMIVPROC)glfwGetProcAddress("glGetProgramiv");
	glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)glfwGetProcAddress("glGetProgramInfoLog");
	glLinkProgram = (PFNGLLINKPROGRAMPROC)glfwGetProcAddress("glLinkProgram");
	glAttachShader = (PFNGLATTACHSHADERPROC)glfwGetProcAddress("glAttachShader");
	glCreateProgram = (PFNGLCREATEPROGRAMPROC)glfwGetProcAddress("glCreateProgram");
	glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)glfwGetProcAddress("glGetShaderInfoLog");
	glGetShaderiv = (PFNGLGETSHADERIVPROC)glfwGetProcAddress("glGetShaderiv");
	glCompileShader = (PFNGLCOMPILESHADERPROC)glfwGetProcAddress("glCompileShader");
	glShaderSource = (PFNGLSHADERSOURCEPROC)glfwGetProcAddress("glShaderSource");
	glCreateShader = (PFNGLCREATESHADERPROC)glfwGetProcAddress("glCreateShader");
	glBufferData = (PFNGLBUFFERDATAPROC)glfwGetProcAddress("glBufferData");
	glBindBuffer = (PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");
	glGenBuffers = (PFNGLGENBUFFERSPROC)glfwGetProcAddress("glGenBuffers");
	glClear = (PFNGLCLEARPROC)glfwGetProcAddress("glClear");
	glClearColor = (PFNGLCLEARCOLORPROC)glfwGetProcAddress("glClearColor");
	glViewport = (PFNGLVIEWPORTPROC)glfwGetProcAddress("glViewport");

	return true && glViewport;
}
