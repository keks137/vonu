#include "android_core.h"
#include "../core.h"
#include <android/window.h>
#include <android/log.h>
#include "../loadopengl.h"
#include "../logs.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../../vendor/stb_image.h"
extern int main(int argc, char *argv[]);

static struct android_app *g_app;
static struct android_poll_source *data;
static EGLDisplay display = EGL_NO_DISPLAY;
static EGLSurface surface = EGL_NO_SURFACE;
static EGLContext ctx = EGL_NO_CONTEXT;
static EGLConfig cfg;
static bool need_context_rebind;

void android_main(struct android_app *app)
{
	__android_log_print(ANDROID_LOG_ERROR, "Vonu", "hi");
	g_app = app;

	char arg0[] = "androtest";

	(void)main(1, (char *[]){ arg0, NULL });

	ANativeActivity_finish(app->activity);

	int pollResult = 0;
	int pollEvents = 0;

	while (!app->destroyRequested) {
		while ((pollResult = ALooper_pollOnce(0, NULL, &pollEvents, (void **)&data)) > ALOOPER_POLL_TIMEOUT) {
			if (data != NULL)
				data->process(app, data);
		}
	}
}

static void handle_cmd(struct android_app *app, int32_t cmd)
{
	switch (cmd) {
	case APP_CMD_INIT_WINDOW: {
		if (app->window) {
			if (need_context_rebind) {
				EGLint displayFormat = 0;
				eglGetConfigAttrib(display, cfg, EGL_NATIVE_VISUAL_ID, &displayFormat);
				need_context_rebind = false;
				surface = eglCreateWindowSurface(display, cfg, app->window, NULL);
				eglMakeCurrent(display, surface, surface, ctx);
			} else {
				display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
				eglInitialize(display, NULL, NULL);
				EGLint ncfg;
				eglChooseConfig(display, (EGLint[]){ EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE }, &cfg, 1, &ncfg);
				ctx = eglCreateContext(display, cfg, EGL_NO_CONTEXT, (EGLint[]){ EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE });
				surface = eglCreateWindowSurface(display, cfg, app->window, NULL);
				eglMakeCurrent(display, surface, surface, ctx);
			}
		}
	} break;
	case APP_CMD_TERM_WINDOW: {
		if (display != EGL_NO_DISPLAY) {
			eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

			if (surface != EGL_NO_SURFACE) {
				eglDestroySurface(display, surface);
				surface = EGL_NO_SURFACE;
			}

			need_context_rebind = true;
		}

	} break;
	}
}

void platform_init(WindowData *window, size_t width, size_t height)
{
	window->app = g_app;
	g_app->onAppCmd = handle_cmd;
	while (!g_app->window) {
		int pollResult = 0;
		int pollEvents = 0;
		while ((pollResult = ALooper_pollOnce(0, NULL, &pollEvents, ((void **)&data)) > ALOOPER_POLL_TIMEOUT)) {
			if (data != NULL)
				data->process(g_app, data);
		}
	}
	ANativeActivity_setWindowFlags(g_app->activity, AWINDOW_FLAG_FULLSCREEN, 0);
	AConfiguration_setOrientation(g_app->config, ACONFIGURATION_ORIENTATION_LAND);
}

void process_input(WindowData *window, Player *player)
{
}

void ogl_init(unsigned int *texture)
{
	glEnable(GL_DEPTH_TEST);

	glGenTextures(1, texture);
	glBindTexture(GL_TEXTURE_2D, *texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
}

static bool load_image_android(Image *img, const char *path)
{
	AAsset *asset = AAssetManager_open(g_app->activity->assetManager, path, AASSET_MODE_STREAMING);
	if (!asset)
		return false;

	off_t len = AAsset_getLength(asset);
	unsigned char *buf = malloc(len);
	AAsset_read(asset, buf, len);
	AAsset_close(asset);

	int w, h, n;
	unsigned char *data = stbi_load_from_memory(buf, len, &w, &h, &n, 0);
	free(buf);

	if (!data)
		return false;

	img->width = w;
	img->height = h;
	img->n_chan = n;
	img->data = data;
	return true;
}

void assets_init(Image *image_data)
{
	if (!load_image_android(image_data, "atlas.png")) // TODO: remember to add assets/atlas.png to the apk
		exit(1);

	GLenum format = (image_data->n_chan == 4) ? GL_RGBA : GL_RGB;

	glTexImage2D(GL_TEXTURE_2D, 0, format,
		     image_data->width, image_data->height, 0,
		     format, GL_UNSIGNED_BYTE, image_data->data);
	glGenerateMipmap(GL_TEXTURE_2D);
	stbi_image_free(image_data->data);
}
void platform_destroy(WindowData *window)
{
}

void platform_poll_events()
{
	int pollResult = 0;
	int pollEvents = 0;

	while ((pollResult = ALooper_pollOnce(0, NULL, &pollEvents, (void **)&data)) > ALOOPER_POLL_TIMEOUT) {
		if (data != NULL)
			data->process(g_app, data);
	}
}
void platform_swap_buffers(WindowData *window)
{
	eglSwapBuffers(display, surface);
}
