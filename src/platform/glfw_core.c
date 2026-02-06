#include <stdlib.h>

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

	const float camera_speed = player->movement_speed * game_state.delta_time;
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

void ogl_init(unsigned int *texture)
{
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_DEPTH_CLAMP);

	glGenTextures(1, texture);
	glBindTexture(GL_TEXTURE_2D, *texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	float borderColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	// glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); //wireframe
}

void platform_init(WindowData *window, size_t width, size_t height)
{
	glfw_init(window, width, height);
	if (!load_gl_functions())
		exit(1);
}
void platform_destroy(WindowData *window)
{
	glfwDestroyWindow(window->glfw);
	glfwTerminate();
}

static bool load_image(Image *img, const char *path)
{
	int width = 0;
	int height = 0;
	int nrChannels = 0;
	// stbi_set_flip_vertically_on_load(true);
	FILE *file = fopen(path, "rb");
	if (file == NULL) {
		VERROR("Could not open file: %s", path);
		return false;
	}
	unsigned char *data = stbi_load_from_file(file, &width, &height, &nrChannels, 0);
	fclose(file);
	img->width = width;
	img->height = height;
	img->n_chan = nrChannels;
	img->data = data;
	if (data == NULL)
		return false;

	return true;
}

void assets_init(Image *image_data)
{
	if (!load_image(image_data, "assets/atlas.png"))
		exit(1);
	GLenum format;
	if (image_data->n_chan == 3) {
		format = GL_RGB;
	} else if (image_data->n_chan == 4) {
		format = GL_RGBA;
	} else {
		format = GL_RGB;
	}

	glTexImage2D(GL_TEXTURE_2D, 0, format, image_data->width, image_data->height, 0, format, GL_UNSIGNED_BYTE, image_data->data);
	glGenerateMipmap(GL_TEXTURE_2D);
	stbi_image_free(image_data->data);
}

#ifdef WIN32
#include <windows.h>

static u64 g_freq;
static void time_init()
{
	QueryPerformanceFrequency((LARGE_INTEGER *)&g_freq);
}

f64 time_now()
{
	u64 now;
	QueryPerformanceCounter((LARGE_INTEGER *)&now);
	return (f64)now / (f64)g_freq;
}
void precise_sleep(f64 seconds)
{
	HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
	LARGE_INTEGER due;
	due.QuadPart = -(LONGLONG)(seconds * 1e7); // 100ns units, negative = relative
	SetWaitableTimer(timer, &due, 0, NULL, NULL, 0);
	WaitForSingleObject(timer, INFINITE);
	CloseHandle(timer);
}
#else

#include <time.h>

f64 time_now()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec * 1e-9;
}

void precise_sleep(f64 seconds)
{
	struct timespec ts = {
		.tv_sec = (time_t)seconds,
		.tv_nsec = (long)((seconds - (time_t)seconds) * 1e9)
	};
	nanosleep(&ts, NULL);
}

#endif //WIN32
