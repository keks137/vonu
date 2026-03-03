#include <stdlib.h>
#ifdef WIN32
#include <windows.h>

static u64 g_freq;
#endif // WIN32

static void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
	(void)window;
	glViewport(0, 0, width, height);
}
static void set_paused_glfw(GLFWwindow *window, bool val)
{
	game_state.paused = val;
	if (game_state.paused) {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	} else {
		// reset_mouse = true; // TODO: check if need to make global again
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
}

void set_paused(WindowData *window, bool val)
{
	set_paused_glfw(window->glfw, val);
}
static void window_focus_callback(GLFWwindow *window, int focused)
{
	if (focused == GLFW_TRUE) {
	} else {
		set_paused_glfw(window, true);
	}
}
static void glfw_init(WindowData *window, int width, int height)
{
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
	glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);

	if (!glfwInit()) {
		exit(1);
	}
	window->width = width;
	window->height = height;
	window->glfw = glfwCreateWindow(window->width, window->height, "Vonu", NULL, NULL);
	GLFWmonitor *monitor = glfwGetPrimaryMonitor();
	if (monitor) {
		const GLFWvidmode *mode = glfwGetVideoMode(monitor);
		if (mode) {
			int refresh_rate = mode->refreshRate;
			window->freq = 1.0 / refresh_rate;
			// VINFO("Refresh: %i", refresh_rate);
		}
	}
	if (!window->glfw) {
		glfwTerminate();
		exit(1);
	}

	glfwMakeContextCurrent(window->glfw);
	glfwSetFramebufferSizeCallback(window->glfw, framebuffer_size_callback);
	glfwSetInputMode(window->glfw, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetWindowFocusCallback(window->glfw, window_focus_callback);
	glfwSwapInterval(0); // no vsync
}
void process_input(WindowData *window, Player *player)
{
	// VINFO("ip");
	BEGIN_FUNC();
	static bool escapeKeyPressedLastFrame = false;
	static double last_x = 400;
	static double last_y = 300;
	static bool reset_mouse = true;

	bool escapeKeyPressedThisFrame = glfwGetKey(window->glfw, GLFW_KEY_ESCAPE) == GLFW_PRESS;

	if (escapeKeyPressedThisFrame && !escapeKeyPressedLastFrame) {
		set_paused(window, !game_state.paused);
	}

	escapeKeyPressedLastFrame = escapeKeyPressedThisFrame;

	if (game_state.paused) {
		if (glfwGetMouseButton(window->glfw, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
			set_paused(window, false);
		}
		if (glfwGetMouseButton(window->glfw, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
			set_paused(window, false);
		}

		END_FUNC();
		return;
	}
	double xpos, ypos;
	glfwGetCursorPos(window->glfw, &xpos, &ypos);
	if (reset_mouse) {
		last_x = xpos;
		last_y = ypos;
		reset_mouse = false;
	}

	Camera *camera = &player->camera;

	float xoffset = (xpos - last_x) * 0.1f;
	float yoffset = (last_y - ypos) * 0.1f;
	last_x = xpos;
	last_y = ypos;

	camera->yaw += xoffset;
	camera->pitch += yoffset;

	if (camera->pitch > 89.0f)
		camera->pitch = 89.0f;
	if (camera->pitch < -89.0f)
		camera->pitch = -89.0f;

	vec3 direction = {
		cos(glm_rad(camera->yaw)) * cos(glm_rad(camera->pitch)),
		sin(glm_rad(camera->pitch)),
		sin(glm_rad(camera->yaw)) * cos(glm_rad(camera->pitch))
	};
	glm_normalize(direction);
	glm_vec3_copy(direction, camera->front);

	// const float camera_speed = player->movement_speed;
	// const float camera_speed = player->movement_speed * INPUT_FREQ;
	const float camera_speed = player->movement_speed * game_state.delta_time;
	// const float camera_speed = player->movement_speed * game_state.last_input * INPUT_FREQ;
	if (glfwGetKey(window->glfw, GLFW_KEY_SPACE) == GLFW_PRESS) {
		camera->pos[1] += camera_speed;
	}
	if (glfwGetKey(window->glfw, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
		camera->pos[1] -= camera_speed;
	}
	static bool sprinting;
	float forward_speed = camera_speed;
	if (glfwGetKey(window->glfw, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
		sprinting = true;
	}
	if (sprinting)
		forward_speed *= 2;

	vec3 horizontal_front;
	glm_vec3_copy(camera->front, horizontal_front);
	horizontal_front[1] = 0.0f;
	glm_normalize(horizontal_front);

	if (glfwGetKey(window->glfw, GLFW_KEY_W) == GLFW_PRESS) {
		glm_vec3_muladds(horizontal_front, forward_speed, camera->pos);
	} else {
		sprinting = false;
	}

	if (glfwGetKey(window->glfw, GLFW_KEY_S) == GLFW_PRESS) {
		glm_vec3_mulsubs(horizontal_front, camera_speed, camera->pos);
	}
	if (glfwGetKey(window->glfw, GLFW_KEY_A) == GLFW_PRESS) {
		vec3 right;
		glm_cross(horizontal_front, camera->up, right);
		glm_normalize(right);
		glm_vec3_mulsubs(right, camera_speed, camera->pos);
	}
	if (glfwGetKey(window->glfw, GLFW_KEY_D) == GLFW_PRESS) {
		vec3 right;
		glm_cross(horizontal_front, camera->up, right);
		glm_normalize(right);
		glm_vec3_muladds(right, camera_speed, camera->pos);
	}
	if (glfwGetMouseButton(window->glfw, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
		player->breaking = true;
	}
	if (glfwGetMouseButton(window->glfw, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
		player->placing = true;
	}
	END_FUNC();
}

void platform_init(WindowData *window, size_t width, size_t height)
{
#ifdef WIN32
	QueryPerformanceFrequency((LARGE_INTEGER *)&g_freq);
#endif // WIN32

	glfw_init(window, width, height);
	if (!load_gl_functions())
		exit(1);
}
void platform_destroy(WindowData *window)
{
	glfwDestroyWindow(window->glfw);
	glfwTerminate();
}
