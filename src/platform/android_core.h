#ifndef INCLUDE_PLATFORM_ANDROID_CORE_H_
#define INCLUDE_PLATFORM_ANDROID_CORE_H_
#ifndef __GLIBC_USE
#define __GLIBC_USE(x) 0
#endif
#include <android_native_app_glue.h>
typedef struct {
	struct android_app *app;
	int width;
	int height;
} WindowData;

static inline bool window_should_close(WindowData *window)
{
	return window->app->destroyRequested;
}

static inline double platform_get_time()
{
	return 3;
}

void platform_swap_buffers(WindowData *window);
void platform_poll_events();
#endif // INCLUDE_PLATFORM_ANDROID_CORE_H_
