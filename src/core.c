#include "core.h"
#ifdef __ANDROID__
#include "platform/android_core.c"
#else
#include "image.h"
#include "loadopengl.h"
#include "profiling.h"
#include "logs.h"
#include "game.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../vendor/stb_image.h"

#include "platform/glfw_core.c"
#endif
