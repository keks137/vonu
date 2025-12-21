<<<<<<< HEAD
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
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
PFNGLGETERRORPROC glGetError;
PFNGLUNIFORM1IPROC glUniform1i;
PFNGLACTIVETEXTUREPROC glActiveTexture;
PFNGLENABLEPROC glEnable;
PFNGLCULLFACEPROC glCullFace;
PFNGLENABLEPROC glEnable;
bool load_gl_functions()
{
	glEnable = (PFNGLENABLEPROC)glfwGetProcAddress("glEnable");
	glCullFace = (PFNGLCULLFACEPROC)glfwGetProcAddress("glCullFace");
	glEnable = (PFNGLENABLEPROC)glfwGetProcAddress("glEnable");
	glActiveTexture = (PFNGLACTIVETEXTUREPROC)glfwGetProcAddress("glActiveTexture");
	glUniform1i = (PFNGLUNIFORM1IPROC)glfwGetProcAddress("glUniform1i");
	glGetError = (PFNGLGETERRORPROC)glfwGetProcAddress("glGetError");
	glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)glfwGetProcAddress("glUniformMatrix4fv");
	glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)glfwGetProcAddress("glGetUniformLocation");
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
=======
#include "loadopengl.h"
#include "logs.h"
#define LOAD_GL_FUNC(type, name)                      \
	name = (type)glfwGetProcAddress(#name);       \
	if (!name) {                                  \
		VERROR("Failed to load %s\n", #name); \
		return false;                         \
	}


PFNGLVIEWPORTPROC glViewport = NULL;
PFNGLCLEARCOLORPROC glClearColor = NULL;
PFNGLCLEARPROC glClear = NULL;
PFNGLGENBUFFERSPROC glGenBuffers = NULL;
PFNGLBINDBUFFERPROC glBindBuffer = NULL;
PFNGLBUFFERDATAPROC glBufferData = NULL;
PFNGLCREATESHADERPROC glCreateShader = NULL;
PFNGLSHADERSOURCEPROC glShaderSource = NULL;
PFNGLCOMPILESHADERPROC glCompileShader = NULL;
PFNGLGETSHADERIVPROC glGetShaderiv = NULL;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = NULL;
PFNGLCREATEPROGRAMPROC glCreateProgram = NULL;
PFNGLATTACHSHADERPROC glAttachShader = NULL;
PFNGLLINKPROGRAMPROC glLinkProgram = NULL;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = NULL;
PFNGLGETPROGRAMIVPROC glGetProgramiv = NULL;
PFNGLUSEPROGRAMPROC glUseProgram = NULL;
PFNGLDELETESHADERPROC glDeleteShader = NULL;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = NULL;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = NULL;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray = NULL;
PFNGLDRAWARRAYSPROC glDrawArrays = NULL;
PFNGLDRAWELEMENTSPROC glDrawElements = NULL;
PFNGLPOLYGONMODEPROC glPolygonMode = NULL;
PFNGLGENTEXTURESPROC glGenTextures = NULL;
PFNGLBINDTEXTUREPROC glBindTexture = NULL;
PFNGLTEXIMAGE2DPROC glTexImage2D = NULL;
PFNGLGENERATEMIPMAPPROC glGenerateMipmap = NULL;
PFNGLTEXPARAMETERIPROC glTexParameteri = NULL;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = NULL;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv = NULL;
PFNGLGETERRORPROC glGetError = NULL;
PFNGLUNIFORM1IPROC glUniform1i = NULL;
PFNGLACTIVETEXTUREPROC glActiveTexture = NULL;
PFNGLENABLEPROC glEnable = NULL;
PFNGLCULLFACEPROC glCullFace = NULL;

bool load_gl_functions()
{
	LOAD_GL_FUNC(PFNGLENABLEPROC, glEnable);
	LOAD_GL_FUNC(PFNGLCULLFACEPROC, glCullFace);
	LOAD_GL_FUNC(PFNGLACTIVETEXTUREPROC, glActiveTexture);
	LOAD_GL_FUNC(PFNGLUNIFORM1IPROC, glUniform1i);
	LOAD_GL_FUNC(PFNGLGETERRORPROC, glGetError);
	LOAD_GL_FUNC(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv);
	LOAD_GL_FUNC(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation);
	LOAD_GL_FUNC(PFNGLTEXPARAMETERIPROC, glTexParameteri);
	LOAD_GL_FUNC(PFNGLGENERATEMIPMAPPROC, glGenerateMipmap);
	LOAD_GL_FUNC(PFNGLTEXIMAGE2DPROC, glTexImage2D);
	LOAD_GL_FUNC(PFNGLBINDTEXTUREPROC, glBindTexture);
	LOAD_GL_FUNC(PFNGLGENTEXTURESPROC, glGenTextures);
	LOAD_GL_FUNC(PFNGLPOLYGONMODEPROC, glPolygonMode);
	LOAD_GL_FUNC(PFNGLDRAWELEMENTSPROC, glDrawElements);
	LOAD_GL_FUNC(PFNGLDRAWARRAYSPROC, glDrawArrays);
	LOAD_GL_FUNC(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray);
	LOAD_GL_FUNC(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays);
	LOAD_GL_FUNC(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray);
	LOAD_GL_FUNC(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer);
	LOAD_GL_FUNC(PFNGLDELETESHADERPROC, glDeleteShader);
	LOAD_GL_FUNC(PFNGLUSEPROGRAMPROC, glUseProgram);
	LOAD_GL_FUNC(PFNGLGETPROGRAMIVPROC, glGetProgramiv);
	LOAD_GL_FUNC(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog);
	LOAD_GL_FUNC(PFNGLLINKPROGRAMPROC, glLinkProgram);
	LOAD_GL_FUNC(PFNGLATTACHSHADERPROC, glAttachShader);
	LOAD_GL_FUNC(PFNGLCREATEPROGRAMPROC, glCreateProgram);
	LOAD_GL_FUNC(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog);
	LOAD_GL_FUNC(PFNGLGETSHADERIVPROC, glGetShaderiv);
	LOAD_GL_FUNC(PFNGLCOMPILESHADERPROC, glCompileShader);
	LOAD_GL_FUNC(PFNGLSHADERSOURCEPROC, glShaderSource);
	LOAD_GL_FUNC(PFNGLCREATESHADERPROC, glCreateShader);
	LOAD_GL_FUNC(PFNGLBUFFERDATAPROC, glBufferData);
	LOAD_GL_FUNC(PFNGLBINDBUFFERPROC, glBindBuffer);
	LOAD_GL_FUNC(PFNGLGENBUFFERSPROC, glGenBuffers);
	LOAD_GL_FUNC(PFNGLCLEARPROC, glClear);
	LOAD_GL_FUNC(PFNGLCLEARCOLORPROC, glClearColor);
	LOAD_GL_FUNC(PFNGLVIEWPORTPROC, glViewport);
>>>>>>> ORIG_HEAD

	return true;
}
