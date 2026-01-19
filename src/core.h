#ifndef INCLUDE_SRC_CORE_H_
#define INCLUDE_SRC_CORE_H_

#include "game.h"
#include "image.h"
#include "loadopengl.h"

#if __ANDROID__
#include "platform/android_core.h"
#else
#include "platform/glfw_core.h"
#endif

// NOTE: consider storing window as global, it can only be on one thread anyways and necessity varies amongst platforms
void set_paused(WindowData *window, bool val);
void platform_init(WindowData *window, size_t width, size_t height);
void process_input(WindowData *window, Player *player);
void platform_destroy(WindowData *window);
void platform_swap_buffers(WindowData *window);
void ogl_init(unsigned int *texture);
void assets_init(Image *image_data);
#endif // INCLUDE_SRC_CORE_H_
