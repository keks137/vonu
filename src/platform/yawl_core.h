#ifndef INCLUDE_PLATFORM_YAWL_CORE_H_
#define INCLUDE_PLATFORM_YAWL_CORE_H_

#include <yawl.h>


extern YwState glob_state;
extern YwWindowData glob_win;

typedef struct {
	YwWindowData w;
	int width;
	int height;
	bool should_close;
	f64 freq;
} WindowData;

static inline bool WindowShouldClose(WindowData *window)
{
	return window->w.should_close;
}
static inline void platform_poll_events()
{
	YwPollEvents(&glob_win);
}
static inline void platform_swap_buffers(WindowData *window)
{
	YwEndDrawing(&glob_win);
}
#endif // INCLUDE_PLATFORM_YAWL_CORE_H_
