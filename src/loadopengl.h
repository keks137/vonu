#ifndef INCLUDE_SRC_LOADOPENGL_H_
#define INCLUDE_SRC_LOADOPENGL_H_
#ifdef __ANDROID__
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#elif defined(PLATFORM_GLFW)
#define GLFW_INCLUDE_NONE
#include "glfw/include/GLFW/glfw3.h"
#include "platform/loadopengl.h"

#else

#include "platform/loadopengl.h"
#endif
#endif // INCLUDE_SRC_LOADOPENGL_H_
