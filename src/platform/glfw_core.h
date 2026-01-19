#ifndef INCLUDE_PLATFORM_GLFW_CORE_H_
#define INCLUDE_PLATFORM_GLFW_CORE_H_

typedef struct {
	GLFWwindow *glfw;
	int width;
	int height;
} WindowData;

static inline bool window_should_close(WindowData *window)
{
	return glfwWindowShouldClose(window->glfw);
}

static inline void platform_swap_buffers(WindowData *window)
{
	glfwSwapBuffers(window->glfw);
}
static inline void platform_poll_events()
{
	glfwPollEvents();
}
static inline double platform_get_time()
{
	return glfwGetTime();
}



#endif  // INCLUDE_PLATFORM_GLFW_CORE_H_
