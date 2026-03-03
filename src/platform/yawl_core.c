YwState glob_state;
YwWindowData glob_win;

void platform_init(WindowData *window, size_t width, size_t height)
{
#ifdef WIN32
	QueryPerformanceFrequency((LARGE_INTEGER *)&g_freq);
#endif // WIN32

	YwInitWindow(&glob_state, &glob_win, "vonu");
	window->freq = 1.0 / 60;

	if (!load_gl_functions())
		exit(1);
}
void platform_destroy(WindowData *window)
{
}
void process_input(WindowData *window, Player *player)
{
	// VINFO("ip");
	BEGIN_FUNC();
	static bool escapeKeyPressedLastFrame = false;

	bool escapeKeyPressedThisFrame = YwKeyPressed(&window->w, YW_KEY_ESCAPE);

	if (escapeKeyPressedThisFrame && !escapeKeyPressedLastFrame) {
		set_paused(window, !game_state.paused);
	}

	escapeKeyPressedLastFrame = escapeKeyPressedThisFrame;

	Camera *camera = &player->camera;
	const float camera_speed = player->movement_speed * game_state.delta_time;
	if (YwKeyPressed(&window->w, YW_KEY_SPACE)) {
		camera->pos[1] += camera_speed;
	}
	if (YwKeyPressed(&window->w, YW_KEY_LSHIFT)) {
		camera->pos[1] -= camera_speed;
	}
	// static bool sprinting;
	// float forward_speed = camera_speed;
	// if (glfwGetKey(window->glfw, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
	// 	sprinting = true;
	// }
	// if (sprinting)
	// 	forward_speed *= 2;
	//

	// if (glfwGetKey(window->glfw, GLFW_KEY_W) == GLFW_PRESS) {
	// 	glm_vec3_muladds(horizontal_front, forward_speed, camera->pos);
	// } else {
	// 	sprinting = false;
	// }
	//
	// if (glfwGetKey(window->glfw, GLFW_KEY_S) == GLFW_PRESS) {
	// 	glm_vec3_mulsubs(horizontal_front, camera_speed, camera->pos);
	// }
	// if (glfwGetKey(window->glfw, GLFW_KEY_A) == GLFW_PRESS) {
	// 	vec3 right;
	// 	glm_cross(horizontal_front, camera->up, right);
	// 	glm_normalize(right);
	// 	glm_vec3_mulsubs(right, camera_speed, camera->pos);
	// }
	// if (glfwGetKey(window->glfw, GLFW_KEY_D) == GLFW_PRESS) {
	// 	vec3 right;
	// 	glm_cross(horizontal_front, camera->up, right);
	// 	glm_normalize(right);
	// 	glm_vec3_muladds(right, camera_speed, camera->pos);
	// }
	// if (glfwGetMouseButton(window->glfw, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
	// 	player->breaking = true;
	// }
	// if (glfwGetMouseButton(window->glfw, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
	// 	player->placing = true;
	// }
	END_FUNC();
}
void set_paused(WindowData *window, bool val)
{
}
