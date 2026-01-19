#ifndef INCLUDE_SRC_LOADOPENGL_H_
#define INCLUDE_SRC_LOADOPENGL_H_
#ifdef __ANDROID__
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#else
#define GLFW_INCLUDE_NONE
#include "../vendor/glfw/include/GLFW/glfw3.h"
#include "platform/glfw_loadopengl.h"
#endif
#endif // INCLUDE_SRC_LOADOPENGL_H_
