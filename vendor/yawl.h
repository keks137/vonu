#ifndef INCLUDE_YAWL_YAWL_H_
#define INCLUDE_YAWL_YAWL_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(_WIN32)
#define YAWL_WIN32
#include <windows.h>
#elif defined(__ANDROID__)
#define YAWL_ANDROID
#define YAWL_EGL
#include <android/native_window.h>
#include <android/native_activity.h>
#include <android/native_window_jni.h>
#include <android/looper.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <fcntl.h>
#include <unistd.h>
#elif defined(__linux__)
#define YAWL_EGL
#ifndef YAWL_WAYLAND
#define YAWL_X11
#define YAWL_EGL_EXTENDED
#endif // YAWL_WAYLAND
#endif // _WIN32

#if defined(__GNUC__)
#define YW_EXPORT __attribute__((visibility("default")))
#elif defined(_MSC_VER)
#define YW_EXPORT __declspec(dllexport)
#else
#define YW_EXPORT
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define YW_STATIC_ASSERT(expr, msg) _Static_assert(expr, msg)

#elif defined(__cplusplus) && __cplusplus >= 201103L
#define YW_STATIC_ASSERT(expr, msg) static_assert(expr, msg)

#else
#define YW_STATIC_ASSERT(expr, msg) \
	typedef char VASSERT_STATIC_##__LINE__[(expr) ? 1 : -1]
#endif

#ifdef YAWL_WIN32

struct _YwWGLFuncs {
	bool loaded;
	bool ext_loaded;
	PROC(WINAPI *get_proc_address)(LPCSTR);
	HGLRC(WINAPI *create_context)(HDC);
	BOOL(WINAPI *delete_context)(HGLRC);
	BOOL(WINAPI *make_current)(HDC, HGLRC);
	BOOL(WINAPI *share_lists)(HGLRC, HGLRC);
	BOOL(WINAPI *swap_interval)(int);
};

#endif // YAWL_WIN32

typedef enum {
	YW_KEY_UNKNOWN = 0,
	YW_KEY_A,
	YW_KEY_B,
	YW_KEY_C,
	YW_KEY_D,
	YW_KEY_E,
	YW_KEY_F,
	YW_KEY_G,
	YW_KEY_H,
	YW_KEY_I,
	YW_KEY_J,
	YW_KEY_K,
	YW_KEY_L,
	YW_KEY_M,
	YW_KEY_N,
	YW_KEY_O,
	YW_KEY_P,
	YW_KEY_Q,
	YW_KEY_R,
	YW_KEY_S,
	YW_KEY_T,
	YW_KEY_U,
	YW_KEY_V,
	YW_KEY_W,
	YW_KEY_X,
	YW_KEY_Y,
	YW_KEY_Z,
	YW_KEY_0,
	YW_KEY_1,
	YW_KEY_2,
	YW_KEY_3,
	YW_KEY_4,
	YW_KEY_5,
	YW_KEY_6,
	YW_KEY_7,
	YW_KEY_8,
	YW_KEY_9,
	YW_KEY_SPACE,
	YW_KEY_ENTER,
	YW_KEY_TAB,
	YW_KEY_ESCAPE,
	YW_KEY_BACKSPACE,
	YW_KEY_UP,
	YW_KEY_DOWN,
	YW_KEY_LEFT,
	YW_KEY_RIGHT,
	YW_KEY_LSHIFT,
	YW_KEY_RSHIFT,
	YW_KEY_LCTRL,
	YW_KEY_RCTRL,
	YW_KEY_LALT,
	YW_KEY_RALT,
	YW_KEY_LSUPER,
	YW_KEY_RSUPER, // super = win/cmd
	YW_KEY_FRAME_END, // control character for macros
	YW_KEY_COUNT,
} YwKey;

enum {
	YW_KEY_PRESSED = 1 << 0,
	YW_KEYMOD_SHIFT = 1 << 1,
	YW_KEYMOD_CTRL = 1 << 2,
	YW_KEYMOD_ALT = 1 << 3,
	YW_KEYMOD_SUPER = 1 << 4,
};
typedef uint8_t YwKeyState;
typedef struct {
	YwKey key;
	YwKeyState pressed; // bitmask: 1=pressed + mods: 2=shift, 4=ctrl, 8=alt, 16=super
} YwKeyEvent;

#ifndef YW_KEYBUF_SIZE
#define YW_KEYBUF_SIZE YW_KEY_COUNT
#endif //YW_KEYBUF_SIZE

struct YwKeyBuffer {
	YwKeyEvent events[YW_KEYBUF_SIZE];
	uint8_t write;
	uint8_t read;
};

#ifdef YAWL_EGL
#include <EGL/egl.h>
#ifdef YAWL_EGL_EXTENDED
#include <EGL/eglext.h>
#endif // YAWL_EGL_EXTENDED

struct _YwEGLFuncs {
	bool loaded;

	void *(EGLAPIENTRY *get_proc_address)(const char *);
	EGLDisplay (*get_display)(EGLNativeDisplayType);
	EGLBoolean (*initialize)(EGLDisplay, EGLint *, EGLint *);
	EGLBoolean (*terminate)(EGLDisplay);
	EGLContext (*create_context)(EGLDisplay, EGLConfig, EGLContext, const EGLint *);
	EGLBoolean (*make_current)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
	EGLBoolean (*swap_buffers)(EGLDisplay, EGLSurface);
	EGLSurface (*create_window_surface)(EGLDisplay, EGLConfig, EGLNativeWindowType, const EGLint *);
	EGLBoolean (*bind_api)(EGLenum);
	EGLBoolean (*choose_config)(EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *);
	EGLint (*get_error)(void);
	EGLBoolean (*swap_interval)(EGLDisplay, EGLint);
	PFNEGLDESTROYSURFACEPROC destroy_surface;
#ifdef YAWL_EGL_EXTENDED
	PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display;
#endif // YAWL_EGL_EXTENDED
};

#endif // YAWL_EGL

#ifdef YAWL_X11
#include <linux/input-event-codes.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xkb.h>
#include <X11/XKBlib.h>
struct _X11Display;
typedef struct _X11Display X11Display;
typedef unsigned long X11ID;
typedef X11ID X11Window;

struct _YwX11Funcs {
	bool loaded;

	xcb_connection_t *(*connect)(const char *displayname, int *screenp);
	void (*disconnect)(xcb_connection_t *c);
	xcb_screen_iterator_t (*setup_roots_iterator)(xcb_setup_t *R);
	int (*flush)(xcb_connection_t *c);
	uint32_t (*generate_id)(xcb_connection_t *c);
	xcb_void_cookie_t (*create_window)(
		xcb_connection_t *c,
		uint8_t depth,
		xcb_window_t wid,
		xcb_window_t parent,
		int16_t x,
		int16_t y,
		uint16_t width,
		uint16_t height,
		uint16_t border_width,
		uint16_t _class,
		xcb_visualid_t visual,
		uint32_t value_mask,
		const void *value_list);
	xcb_void_cookie_t (*map_window)(xcb_connection_t *c, xcb_window_t window);
	xcb_void_cookie_t (*change_property)(
		xcb_connection_t *c,
		uint8_t mode,
		xcb_window_t window,
		xcb_atom_t property,
		xcb_atom_t type,
		uint8_t format,
		uint32_t data_len,
		const void *data);
	xcb_intern_atom_cookie_t (*intern_atom)(
		xcb_connection_t *c,
		uint8_t only_if_exists,
		uint16_t name_len,
		const char *name);
	xcb_intern_atom_reply_t *(*intern_atom_reply)(
		xcb_connection_t *c,
		xcb_intern_atom_cookie_t cookie,
		xcb_generic_error_t **e);
	xcb_generic_event_t *(*poll_for_event)(xcb_connection_t *c);
	void (*request_check)(xcb_connection_t *c, xcb_void_cookie_t cookie);
	struct xcb_setup_t *(*get_setup)(xcb_connection_t *c);
	void (*screen_next)(xcb_screen_iterator_t *i);
	int (*connection_has_error)(xcb_connection_t *c);
};

#endif //YAWL_X11

typedef struct {
	bool initialized;
#ifdef YAWL_X11
	void *lgl;
	int screen_num;
	xcb_atom_t wm_protocols;
	xcb_atom_t wm_delete_window;
	struct _YwX11Funcs x;
#endif // YAWL_X11
#ifdef YAWL_EGL
	struct _YwEGLFuncs e;
#endif //YAWL_EGL
#ifdef YAWL_WIN32
	HINSTANCE hinstance;
	HMODULE opengl32;
	struct _YwWGLFuncs wgl;
#endif
} YwState;

typedef struct {
	YwKeyState current[YW_KEY_COUNT];
	YwKeyState prev[YW_KEY_COUNT];
} YwKeysState;
typedef struct {
	size_t width;
	size_t height;
	bool should_close;
	bool resized;
	struct YwKeyBuffer key_buf;
	YwState *state;
	YwKeysState keys;
	bool focused;

#ifdef YAWL_X11
	xcb_connection_t *conn;
	xcb_window_t win;
	xcb_screen_t *screen;

	uint8_t tmp_x11_mod_state;
#endif // YAWL_X11
#ifdef YAWL_WIN32
	HWND hwnd;
	HDC hdc;
	HGLRC gl_context;
#endif // YAWL_WIN32
#ifdef YAWL_ANDROID
	ANativeWindow *native_window;
	bool egl_context_bound;
	ANativeActivity *activity;
	AInputQueue *input_queue;
	ALooper *looper;
	int msgread;
	int msgwrite;
	bool window_ready;

#endif // YAWL_ANDROID
#ifdef YAWL_EGL
	EGLSurface egl_surface;
	EGLContext egl_context;
	EGLConfig egl_config;
	EGLDisplay egl_display;
	bool resized_internal;
#endif // YAWL_EGL
} YwWindowData;
#include <string.h>
#ifndef YwMemset
#define YwMemset(ptr, val, n_bytes) memset(ptr, val, n_bytes)
#endif //YwMemset
#ifndef YwMemcpy
#define YwMemcpy(dst, src, n_bytes) memcpy(dst, src, n_bytes)
#endif // YwMemcpy

#if defined(_MSC_VER)
#include <malloc.h>
#define YwAlloca(size) _alloca(size)
#elif defined(__GNUC__)
#define YwAlloca(size) __builtin_alloca(size)
#else
#include <alloca.h>
#define YwAlloca(size) alloca(size)
#endif

YW_EXPORT bool YwInitWindow(YwState *s, YwWindowData *w, const char *name);
YW_EXPORT void YwBeginDrawing(YwWindowData *w);
YW_EXPORT void YwEndDrawing(YwWindowData *w);
YW_EXPORT bool YwGLLoadProc(YwState *s, void **proc, const char *name);
YW_EXPORT void YwGLMakeCurrent(YwWindowData *w);
YW_EXPORT void YwSetVSync(YwWindowData *w, bool enabled);
YW_EXPORT bool YwNextKeyEvent(YwWindowData *w, YwKeyEvent *out);
YW_EXPORT void YwPollEvents(YwWindowData *w);
YW_EXPORT bool YwKeyPressed(YwWindowData *w, YwKey key);
YW_EXPORT bool YwKeyReleased(YwWindowData *w, YwKey key);
YW_EXPORT bool YwKeyDown(YwWindowData *w, YwKey key);
YW_EXPORT bool YwKeyPressedMods(YwWindowData *w, YwKey key, YwKeyState mod_mask);
YW_EXPORT bool YwKeyReleasedMods(YwWindowData *w, YwKey key, YwKeyState mod_mask);
YW_EXPORT bool YwKeyDownMods(YwWindowData *w, YwKey key, YwKeyState mod_mask);

#ifdef YAWL_ANDROID
YW_EXPORT void YwAndroidSetActivity(YwState *s, YwWindowData *w, ANativeActivity *activity);
#endif

#endif // INCLUDE_YAWL_YAWL_H_
#ifdef YAWL_IMPLEMENTATION
YW_EXPORT bool YwKeyPressedMods(YwWindowData *w, YwKey key, YwKeyState mod_mask)
{
	if (w->keys.current[key] && !w->keys.prev[key] && (w->keys.current[key] & mod_mask)) {
		return true;
	}
	return false;
}
YW_EXPORT bool YwKeyReleasedMods(YwWindowData *w, YwKey key, YwKeyState mod_mask)
{
	if (!w->keys.current[key] && w->keys.prev[key] && (w->keys.prev[key] & mod_mask)) { // NOTE: we check mask on prev because not pressed keys dont hold state
		return true;
	}
	return false;
}
YW_EXPORT bool YwKeyDownMods(YwWindowData *w, YwKey key, YwKeyState mod_mask)
{
	if (w->keys.current[key] && (w->keys.current[key] & mod_mask)) {
		return true;
	}
	return false;
}
YW_EXPORT bool YwKeyPressed(YwWindowData *w, YwKey key)
{
	if (w->keys.current[key] && !w->keys.prev[key]) {
		return true;
	}
	return false;
}
YW_EXPORT bool YwKeyReleased(YwWindowData *w, YwKey key)
{
	if (!w->keys.current[key] && w->keys.prev[key]) {
		return true;
	}
	return false;
}
YW_EXPORT bool YwKeyDown(YwWindowData *w, YwKey key)
{
	if (w->keys.current[key]) {
		return true;
	}
	return false;
}
#ifdef __linux__
#include <dlfcn.h>
#define YW_LOAD_SYMBOL(sym, lib, name)                                   \
	do {                                                             \
		void *tmp_symbol = dlsym(lib, name);                     \
		if (tmp_symbol == NULL) {                                \
			fprintf(stderr, "symbol not found: %s\n", name); \
			return false;                                    \
		}                                                        \
		*(void **)(&sym) = tmp_symbol;                           \
	} while (0)
#endif //__linux__
#define YW_KEYBUF_PUSH(buf, ev)                                     \
	do {                                                        \
		uint8_t _next = (buf)->write + 1;                   \
		if (_next >= YW_KEYBUF_SIZE)                        \
			_next = 0;                                  \
		if (_next == (buf)->read)                           \
			(buf)->read = (_next + 1) % YW_KEYBUF_SIZE; \
		(buf)->events[(buf)->write] = (ev);                 \
		(buf)->write = _next;                               \
	} while (0)

static void _YwUnfocusInputRelease(YwWindowData *w)
{
	YwKeyEvent e = { 0 };
	for (size_t i = 0; i < sizeof(w->keys.current); i++) {
		if (w->keys.current[i]) {
			e.key = i;
			YW_KEYBUF_PUSH(&w->key_buf, e);
			w->keys.current[i] = 0;
		}
	}
}

#ifdef YAWL_EGL
static bool _YwEGLLoad(YwState *s)
{
	void *egl = dlopen("libEGL.so.1", RTLD_NOW | RTLD_LOCAL);
	if (!egl)
		egl = dlopen("libEGL.so", RTLD_NOW | RTLD_LOCAL);
	if (!egl) {
		fprintf(stderr, "no EGL: %s\n", dlerror());
		return false;
	}

	YW_LOAD_SYMBOL(s->e.get_proc_address, egl, "eglGetProcAddress");
	YW_LOAD_SYMBOL(s->e.get_display, egl, "eglGetDisplay");
	YW_LOAD_SYMBOL(s->e.initialize, egl, "eglInitialize");
	YW_LOAD_SYMBOL(s->e.terminate, egl, "eglTerminate");
	YW_LOAD_SYMBOL(s->e.create_context, egl, "eglCreateContext");
	YW_LOAD_SYMBOL(s->e.make_current, egl, "eglMakeCurrent");
	YW_LOAD_SYMBOL(s->e.swap_buffers, egl, "eglSwapBuffers");
	YW_LOAD_SYMBOL(s->e.create_window_surface, egl, "eglCreateWindowSurface");
	YW_LOAD_SYMBOL(s->e.bind_api, egl, "eglBindAPI");
	YW_LOAD_SYMBOL(s->e.choose_config, egl, "eglChooseConfig");
	YW_LOAD_SYMBOL(s->e.get_error, egl, "eglGetError");
	YW_LOAD_SYMBOL(s->e.swap_interval, egl, "eglSwapInterval");
	YW_LOAD_SYMBOL(s->e.destroy_surface, egl, "eglDestroySurface");
#ifdef YAWL_EGL_EXTENDED
	YW_LOAD_SYMBOL(s->e.get_platform_display, egl, "eglGetPlatformDisplay");
#endif // YAWL_EGL_EXTENDED
	s->e.loaded = true;

	return true;
}

static bool _YwLoadGLProcEGL(YwState *s, void **proc, const char *name)
{
	void *p = NULL;
	p = s->e.get_proc_address(name);
	*proc = p;
	return p != NULL;
}

#endif //YAWL_EGL

#ifdef YAWL_ANDROID
#include <pthread.h>
// Pipe commands: 1=window created, 2=window destroyed, 3=input available, 4=destroy
enum { CMD_WINDOW_CREATED = 1,
       CMD_WINDOW_DESTROYED,
       CMD_INPUT_AVAILABLE,
       CMD_DESTROY };

static YwKey _YwAndroidKeycodeToKey(int32_t keycode)
{
	switch (keycode) {
	case AKEYCODE_A:
		return YW_KEY_A;
	case AKEYCODE_B:
		return YW_KEY_B;
	case AKEYCODE_C:
		return YW_KEY_C;
	case AKEYCODE_D:
		return YW_KEY_D;
	case AKEYCODE_E:
		return YW_KEY_E;
	case AKEYCODE_F:
		return YW_KEY_F;
	case AKEYCODE_G:
		return YW_KEY_G;
	case AKEYCODE_H:
		return YW_KEY_H;
	case AKEYCODE_I:
		return YW_KEY_I;
	case AKEYCODE_J:
		return YW_KEY_J;
	case AKEYCODE_K:
		return YW_KEY_K;
	case AKEYCODE_L:
		return YW_KEY_L;
	case AKEYCODE_M:
		return YW_KEY_M;
	case AKEYCODE_N:
		return YW_KEY_N;
	case AKEYCODE_O:
		return YW_KEY_O;
	case AKEYCODE_P:
		return YW_KEY_P;
	case AKEYCODE_Q:
		return YW_KEY_Q;
	case AKEYCODE_R:
		return YW_KEY_R;
	case AKEYCODE_S:
		return YW_KEY_S;
	case AKEYCODE_T:
		return YW_KEY_T;
	case AKEYCODE_U:
		return YW_KEY_U;
	case AKEYCODE_V:
		return YW_KEY_V;
	case AKEYCODE_W:
		return YW_KEY_W;
	case AKEYCODE_X:
		return YW_KEY_X;
	case AKEYCODE_Y:
		return YW_KEY_Y;
	case AKEYCODE_Z:
		return YW_KEY_Z;
	case AKEYCODE_0:
		return YW_KEY_0;
	case AKEYCODE_1:
		return YW_KEY_1;
	case AKEYCODE_2:
		return YW_KEY_2;
	case AKEYCODE_3:
		return YW_KEY_3;
	case AKEYCODE_4:
		return YW_KEY_4;
	case AKEYCODE_5:
		return YW_KEY_5;
	case AKEYCODE_6:
		return YW_KEY_6;
	case AKEYCODE_7:
		return YW_KEY_7;
	case AKEYCODE_8:
		return YW_KEY_8;
	case AKEYCODE_9:
		return YW_KEY_9;
	case AKEYCODE_SPACE:
		return YW_KEY_SPACE;
	case AKEYCODE_ENTER:
		return YW_KEY_ENTER;
	case AKEYCODE_TAB:
		return YW_KEY_TAB;
	case AKEYCODE_ESCAPE:
		return YW_KEY_ESCAPE;
	case AKEYCODE_DEL:
		return YW_KEY_BACKSPACE;
	case AKEYCODE_DPAD_UP:
		return YW_KEY_UP;
	case AKEYCODE_DPAD_DOWN:
		return YW_KEY_DOWN;
	case AKEYCODE_DPAD_LEFT:
		return YW_KEY_LEFT;
	case AKEYCODE_DPAD_RIGHT:
		return YW_KEY_RIGHT;
	case AKEYCODE_SHIFT_LEFT:
		return YW_KEY_LSHIFT;
	case AKEYCODE_SHIFT_RIGHT:
		return YW_KEY_RSHIFT;
	case AKEYCODE_CTRL_LEFT:
		return YW_KEY_LCTRL;
	case AKEYCODE_CTRL_RIGHT:
		return YW_KEY_RCTRL;
	case AKEYCODE_ALT_LEFT:
		return YW_KEY_LALT;
	case AKEYCODE_ALT_RIGHT:
		return YW_KEY_RALT;
	case AKEYCODE_META_LEFT:
		return YW_KEY_LSUPER;
	case AKEYCODE_META_RIGHT:
		return YW_KEY_RSUPER;
	default:
		return YW_KEY_UNKNOWN;
	}
}

static void _YwAndroidOnDestroy(ANativeActivity *activity)
{
	YwWindowData *w = (YwWindowData *)activity->instance;
	if (w) {
		char cmd = CMD_DESTROY;
		write(w->msgwrite, &cmd, 1);
	}
}

static void _YwAndroidOnStart(ANativeActivity *activity)
{
	(void)activity;
}
static void _YwAndroidOnResume(ANativeActivity *activity)
{
	(void)activity;
}

static void _YwAndroidOnPause(ANativeActivity *activity)
{
	(void)activity;
}
static void _YwAndroidOnStop(ANativeActivity *activity)
{
	(void)activity;
}

static void _YwAndroidOnWindowFocusChanged(ANativeActivity *activity, int focused)
{
	YwWindowData *w = (YwWindowData *)activity->instance;
	if (w) {
		if (focused) {
			w->focused = true;

		} else {
#ifndef YAWL_NO_UNFOCUS_INPUT_RELEASE
			_YwUnfocusInputRelease(w);
#endif // YAWL_NO_UNFOCUS_INPUT_RELEASE
			w->focused = false;
		}
	}
}

static void _YwAndroidOnNativeWindowCreated(ANativeActivity *activity, ANativeWindow *window)
{
	YwWindowData *w = (YwWindowData *)activity->instance;
	if (w) {
		w->native_window = window;
		char cmd = CMD_WINDOW_CREATED;
		write(w->msgwrite, &cmd, 1);
	}
}

static void _YwAndroidOnNativeWindowDestroyed(ANativeActivity *activity, ANativeWindow *window)
{
	(void)window;
	YwWindowData *w = (YwWindowData *)activity->instance;
	if (w) {
		char cmd = CMD_WINDOW_DESTROYED;
		write(w->msgwrite, &cmd, 1);
	}
}

static void _YwAndroidOnInputQueueCreated(ANativeActivity *activity, AInputQueue *queue)
{
	YwWindowData *w = (YwWindowData *)activity->instance;
	if (w) {
		w->input_queue = queue;
		AInputQueue_attachLooper(queue, w->looper, ALOOPER_POLL_CALLBACK, NULL, NULL);
		char cmd = CMD_INPUT_AVAILABLE;
		write(w->msgwrite, &cmd, 1);
	}
}

static void _YwAndroidOnInputQueueDestroyed(ANativeActivity *activity, AInputQueue *queue)
{
	(void)activity;
	AInputQueue_detachLooper(queue);
}

static int _YwAndroidPipeCallback(int fd, int events, void *data)
{
	(void)fd;
	(void)events;
	YwWindowData *w = (YwWindowData *)data;
	YwState *s = w->state;
	if (!s)
		return 1;

	char cmd;
	if (read(w->msgread, &cmd, 1) == 1) {
		switch (cmd) {
		case CMD_WINDOW_CREATED: {
			if (w->egl_display != EGL_NO_DISPLAY && w->egl_context != EGL_NO_CONTEXT) {
				// Destroy old surface if any (should be NULL already, but be safe)
				if (w->egl_surface != EGL_NO_SURFACE) {
					s->e.destroy_surface(w->egl_display, w->egl_surface);
					w->egl_surface = EGL_NO_SURFACE;
				}
				// Create new surface from the current native window
				w->egl_surface = s->e.create_window_surface(w->egl_display,
									    w->egl_config, w->native_window, NULL);
				if (w->egl_surface != EGL_NO_SURFACE) {
					s->e.make_current(w->egl_display, w->egl_surface,
							  w->egl_surface, w->egl_context);
				} else {
					// Log error
				}
			}
		} break;
		case CMD_WINDOW_DESTROYED:
			if (w->egl_display != EGL_NO_DISPLAY) {
				s->e.make_current(w->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
				if (w->egl_surface != EGL_NO_SURFACE) {
					s->e.destroy_surface(w->egl_display, w->egl_surface);
					w->egl_surface = EGL_NO_SURFACE;
				}
			}
			break;
		case CMD_DESTROY:
			w->should_close = true;
			break;
		}
	}
	return 1;
}

YW_EXPORT void YwAndroidSetActivity(YwState *s, YwWindowData *w, ANativeActivity *activity)
{
	w->state = s;
	w->activity = activity;
	activity->instance = w;
	activity->callbacks->onDestroy = _YwAndroidOnDestroy;
	activity->callbacks->onStart = _YwAndroidOnStart;
	activity->callbacks->onResume = _YwAndroidOnResume;
	activity->callbacks->onPause = _YwAndroidOnPause;
	activity->callbacks->onStop = _YwAndroidOnStop;
	activity->callbacks->onWindowFocusChanged = _YwAndroidOnWindowFocusChanged;
	activity->callbacks->onNativeWindowCreated = _YwAndroidOnNativeWindowCreated;
	activity->callbacks->onNativeWindowDestroyed = _YwAndroidOnNativeWindowDestroyed;
	activity->callbacks->onInputQueueCreated = _YwAndroidOnInputQueueCreated;
	activity->callbacks->onInputQueueDestroyed = _YwAndroidOnInputQueueDestroyed;

	int pipes[2];
	pipe(pipes);
	w->msgread = pipes[0];
	w->msgwrite = pipes[1];
	fcntl(w->msgread, F_SETFL, O_NONBLOCK);
}

static bool _YwInitWindowAndroid(YwWindowData *w, const char *name)
{
	YwState *s = w->state;
	(void)name;
	if (!w->activity) {
		fprintf(stderr, "Android: call YwAndroidSetActivity() before YwInitWindow()\n");
		return false;
	}
	if (!s->e.loaded && !_YwEGLLoad(s))
		return false;

	// Wait for window via pipe
	while (!w->native_window && !w->should_close) {
		ALooper_pollOnce(-1, NULL, NULL, NULL);
	}
	if (!s->e.loaded && !_YwEGLLoad(s))
		return false;
	if (!w->native_window)
		return false;
	w->egl_display = s->e.get_display(EGL_DEFAULT_DISPLAY);
	if (w->egl_display == EGL_NO_DISPLAY)
		return false;
	if (!s->e.initialize(w->egl_display, NULL, NULL))
		return false;
	if (!s->e.bind_api(EGL_OPENGL_ES_API))
		return false;
	EGLint attribs[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8,
		EGL_NONE
	};
	EGLint ncfg;
	if (!s->e.choose_config(w->egl_display, attribs, &w->egl_config, 1, &ncfg) || ncfg < 1)
		return false;
	w->egl_context = s->e.create_context(w->egl_display, w->egl_config, EGL_NO_CONTEXT,
					     (EGLint[]){ EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE });
	if (w->egl_context == EGL_NO_CONTEXT)
		return false;
	w->egl_surface = s->e.create_window_surface(w->egl_display, w->egl_config, w->native_window, NULL);
	if (w->egl_surface == EGL_NO_SURFACE)
		return false;
	if (!s->e.make_current(w->egl_display, w->egl_surface, w->egl_surface, w->egl_context))
		return false;
	w->egl_context_bound = true;

	return true;
}

static void _YwPollEventsAndroid(YwWindowData *w)
{
	ALooper_pollOnce(0, NULL, NULL, NULL);
	if (w->input_queue) {
		AInputEvent *event = NULL;
		while (AInputQueue_getEvent(w->input_queue, &event) >= 0) {
			if (AInputQueue_preDispatchEvent(w->input_queue, event))
				continue;
			int32_t type = AInputEvent_getType(event);
			if (type == AINPUT_EVENT_TYPE_KEY) {
				int32_t action = AKeyEvent_getAction(event);
				int32_t keycode = AKeyEvent_getKeyCode(event);
				// int32_t repeat = AKeyEvent_getRepeatCount(event);
				int32_t meta = AKeyEvent_getMetaState(event);
				bool pressed = (action == AKEY_EVENT_ACTION_DOWN);
				uint8_t pressed_data = pressed | ((meta & AMETA_SHIFT_ON) ? YW_KEYMOD_SHIFT : 0) |
						       ((meta & AMETA_CTRL_ON) ? YW_KEYMOD_CTRL : 0) |
						       ((meta & AMETA_ALT_ON) ? YW_KEYMOD_ALT : 0) |
						       ((meta & AMETA_META_ON) ? YW_KEYMOD_SUPER : 0);

				YwKey key = _YwAndroidKeycodeToKey(keycode);
				w->keys.current[key] = pressed ? pressed_data : 0;
				YwKeyEvent e = {
					.key = key,
					.pressed = pressed ? pressed_data : 0,
					// .repeat = (repeat > 0),
				};
				YW_KEYBUF_PUSH(&w->key_buf, e);
			}
			// TODO: handle motion events (touch)
			AInputQueue_finishEvent(w->input_queue, event, 1);
		}
	}
}
#endif //YAWL_ANDROID //YAWL_ANDROID

#ifdef YAWL_X11
static YwKey _YwX11KeycodeToKey(uint8_t code)
{
	// Convert X11 keycode to Linux evdev scancode
	uint16_t scancode = code - 8;
	switch (scancode) {
	case KEY_A:
		return YW_KEY_A;
	case KEY_B:
		return YW_KEY_B;
	case KEY_C:
		return YW_KEY_C;
	case KEY_D:
		return YW_KEY_D;
	case KEY_E:
		return YW_KEY_E;
	case KEY_F:
		return YW_KEY_F;
	case KEY_G:
		return YW_KEY_G;
	case KEY_H:
		return YW_KEY_H;
	case KEY_I:
		return YW_KEY_I;
	case KEY_J:
		return YW_KEY_J;
	case KEY_K:
		return YW_KEY_K;
	case KEY_L:
		return YW_KEY_L;
	case KEY_M:
		return YW_KEY_M;
	case KEY_N:
		return YW_KEY_N;
	case KEY_O:
		return YW_KEY_O;
	case KEY_P:
		return YW_KEY_P;
	case KEY_Q:
		return YW_KEY_Q;
	case KEY_R:
		return YW_KEY_R;
	case KEY_S:
		return YW_KEY_S;
	case KEY_T:
		return YW_KEY_T;
	case KEY_U:
		return YW_KEY_U;
	case KEY_V:
		return YW_KEY_V;
	case KEY_W:
		return YW_KEY_W;
	case KEY_X:
		return YW_KEY_X;
	case KEY_Y:
		return YW_KEY_Y;
	case KEY_Z:
		return YW_KEY_Z;

	case KEY_0:
		return YW_KEY_0;
	case KEY_1:
		return YW_KEY_1;
	case KEY_2:
		return YW_KEY_2;
	case KEY_3:
		return YW_KEY_3;
	case KEY_4:
		return YW_KEY_4;
	case KEY_5:
		return YW_KEY_5;
	case KEY_6:
		return YW_KEY_6;
	case KEY_7:
		return YW_KEY_7;
	case KEY_8:
		return YW_KEY_8;
	case KEY_9:
		return YW_KEY_9;

	case KEY_SPACE:
		return YW_KEY_SPACE;
	case KEY_ENTER:
		return YW_KEY_ENTER;
	case KEY_TAB:
		return YW_KEY_TAB;
	case KEY_ESC:
		return YW_KEY_ESCAPE;
	case KEY_BACKSPACE:
		return YW_KEY_BACKSPACE;

	case KEY_UP:
		return YW_KEY_UP;
	case KEY_DOWN:
		return YW_KEY_DOWN;
	case KEY_LEFT:
		return YW_KEY_LEFT;
	case KEY_RIGHT:
		return YW_KEY_RIGHT;

	case KEY_LEFTSHIFT:
		return YW_KEY_LSHIFT;
	case KEY_RIGHTSHIFT:
		return YW_KEY_RSHIFT;
	case KEY_LEFTCTRL:
		return YW_KEY_LCTRL;
	case KEY_RIGHTCTRL:
		return YW_KEY_RCTRL;
	case KEY_LEFTALT:
		return YW_KEY_LALT;
	case KEY_RIGHTALT:
		return YW_KEY_RALT;
	case KEY_LEFTMETA:
		return YW_KEY_LSUPER;
	case KEY_RIGHTMETA:
		return YW_KEY_RSUPER;
	default:
		return YW_KEY_UNKNOWN;
	}
}

static bool _YwX11Load(YwState *s)
{
	void *lib = dlopen("libxcb.so.1", RTLD_NOW | RTLD_LOCAL);
	if (!lib) {
		lib = dlopen("libxcb.so", RTLD_NOW | RTLD_LOCAL);
	}
	if (!lib) {
		fprintf(stderr, "no xcb: %s\n", dlerror());
		return false;
	}

	YW_LOAD_SYMBOL(s->x.connect, lib, "xcb_connect");
	YW_LOAD_SYMBOL(s->x.disconnect, lib, "xcb_disconnect");
	YW_LOAD_SYMBOL(s->x.setup_roots_iterator, lib, "xcb_setup_roots_iterator");
	YW_LOAD_SYMBOL(s->x.flush, lib, "xcb_flush");
	YW_LOAD_SYMBOL(s->x.generate_id, lib, "xcb_generate_id");
	YW_LOAD_SYMBOL(s->x.create_window, lib, "xcb_create_window");
	YW_LOAD_SYMBOL(s->x.map_window, lib, "xcb_map_window");
	YW_LOAD_SYMBOL(s->x.change_property, lib, "xcb_change_property");
	YW_LOAD_SYMBOL(s->x.intern_atom, lib, "xcb_intern_atom");
	YW_LOAD_SYMBOL(s->x.intern_atom_reply, lib, "xcb_intern_atom_reply");
	YW_LOAD_SYMBOL(s->x.poll_for_event, lib, "xcb_poll_for_event");
	YW_LOAD_SYMBOL(s->x.request_check, lib, "xcb_request_check");
	YW_LOAD_SYMBOL(s->x.get_setup, lib, "xcb_get_setup");
	YW_LOAD_SYMBOL(s->x.screen_next, lib, "xcb_screen_next");
	YW_LOAD_SYMBOL(s->x.connection_has_error, lib, "xcb_connection_has_error");
	s->x.loaded = true;

	return true;
}

static xcb_screen_t *_YwGetScreen(YwState *s, xcb_connection_t *c)
{
	xcb_screen_iterator_t iter = s->x.setup_roots_iterator(s->x.get_setup(c));
	for (; iter.rem; --s->screen_num, s->x.screen_next(&iter)) {
		if (s->screen_num == 0)
			return iter.data;
	}
	return NULL;
}

static bool _YwEGLCreateContextX11(YwState *s, YwWindowData *w)
{
#ifdef YAWL_X11
	w->egl_display = s->e.get_platform_display(EGL_PLATFORM_XCB_EXT, w->conn, NULL);
#else
	w->egl_display = s->e.get_display(w->conn);
#endif // YAWL_X11

	if (w->egl_display == EGL_NO_DISPLAY)
		return false;

	EGLint major, minor;
	if (!s->e.initialize(w->egl_display, &major, &minor))
		return false;

	s->e.bind_api(EGL_OPENGL_API);

	EGLint attribs[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_NONE
	};

	EGLint num_configs;
	if (!s->e.choose_config(w->egl_display, attribs, &w->egl_config, 1, &num_configs) || num_configs < 1) {
		fprintf(stderr, "No EGL config found\n");
		return false;
	}

	w->egl_surface = s->e.create_window_surface(w->egl_display, w->egl_config, w->win, NULL);
	if (w->egl_surface == EGL_NO_SURFACE) {
		EGLint err = s->e.get_error();
		fprintf(stderr, "eglCreateWindowSurface failed: 0x%x (%d)\n", err, err);
		return false;
	}

	EGLint ctx_attribs[] = { EGL_NONE };
	w->egl_context = s->e.create_context(w->egl_display, w->egl_config, EGL_NO_CONTEXT, ctx_attribs);
	if (w->egl_context == EGL_NO_CONTEXT)
		return false;

	s->e.make_current(w->egl_display, w->egl_surface, w->egl_surface, w->egl_context);
	s->e.swap_interval(w->egl_display, 0); // disable VSYNC
	return true;
}
static bool _YwInitWindowX11(YwWindowData *w, const char *name)
{
	YwState *s = w->state;
	if (!s->x.loaded)
		if (!_YwX11Load(s))
			return false;

	w->conn = s->x.connect(NULL, &s->screen_num);
	if (!w->conn || s->x.connection_has_error(w->conn))
		return false;

	w->screen = _YwGetScreen(s, w->conn);
	if (!w->screen)
		return false;
	if (w->width == 0 || w->height == 0) {
		w->width = w->screen->width_in_pixels;
		w->height = w->screen->height_in_pixels;
	}

	w->win = s->x.generate_id(w->conn);
	uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	uint32_t values[2] = {
		w->screen->black_pixel,
		XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_FOCUS_CHANGE
	};

	s->x.create_window(
		w->conn,
		XCB_COPY_FROM_PARENT,
		w->win,
		w->screen->root,
		0, 0,
		(uint16_t)w->width,
		(uint16_t)w->height,
		1,
		XCB_WINDOW_CLASS_INPUT_OUTPUT,
		w->screen->root_visual,
		mask,
		values);

	xcb_intern_atom_cookie_t proto_cookie = s->x.intern_atom(w->conn, 0,
								 sizeof("WM_PROTOCOLS") - 1, "WM_PROTOCOLS");
	xcb_intern_atom_cookie_t del_cookie = s->x.intern_atom(w->conn, 0,
							       sizeof("WM_DELETE_WINDOW") - 1, "WM_DELETE_WINDOW");
	xcb_intern_atom_reply_t *proto_reply = s->x.intern_atom_reply(w->conn, proto_cookie, NULL);
	xcb_intern_atom_reply_t *del_reply = s->x.intern_atom_reply(w->conn, del_cookie, NULL);

	if (proto_reply) {
		s->wm_protocols = proto_reply->atom;
		free(proto_reply);
	}
	if (del_reply) {
		s->wm_delete_window = del_reply->atom;
		free(del_reply);
	}

	if (s->wm_protocols && s->wm_delete_window) {
		s->x.change_property(w->conn, XCB_PROP_MODE_REPLACE, w->win,
				     s->wm_protocols, XCB_ATOM_ATOM, 32, 1, &s->wm_delete_window);
	}

	s->x.change_property(w->conn, XCB_PROP_MODE_REPLACE, w->win,
			     XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(name), name);

	s->x.map_window(w->conn, w->win);

	if (!s->e.loaded)
		if (!_YwEGLLoad(s))
			return false;

	if (!_YwEGLCreateContextX11(s, w))
		return false;

	s->e.swap_buffers(w->egl_display, w->egl_surface);

	return true;
}

static void _YwPollEventsX11(YwWindowData *w)
{
	YwState *s = w->state;
	xcb_generic_event_t *ev;
	while ((ev = s->x.poll_for_event(w->conn))) {
		uint8_t response_type = ev->response_type & ~0x80;
		switch (response_type) {
		case XCB_EXPOSE:
			break;
		case XCB_CLIENT_MESSAGE: {
			xcb_client_message_event_t *cm = (xcb_client_message_event_t *)ev;
			if (cm->data.data32[0] == s->wm_delete_window) {
				w->should_close = true;
			}
			break;
		}
		case XCB_CONFIGURE_NOTIFY: {
			xcb_configure_notify_event_t *cn = (xcb_configure_notify_event_t *)ev;
			uint16_t new_width = cn->width;
			uint16_t new_height = cn->height;
			if (new_width != w->width || new_height != w->height) {
				w->resized = true;
				w->resized_internal = true;
			}
			w->width = new_width;
			w->height = new_height;
		} break;
		case XCB_FOCUS_IN:
			w->focused = true;
			break;
		case XCB_FOCUS_OUT: {
#ifndef YAWL_NO_UNFOCUS_INPUT_RELEASE
			_YwUnfocusInputRelease(w);
#endif // YAWL_NO_UNFOCUS_INPUT_RELEASE
			w->focused = false;
		} break;
		case XCB_KEY_PRESS:
		case XCB_KEY_RELEASE: {
			xcb_key_press_event_t *ke = (xcb_key_press_event_t *)ev;
			bool pressed = (response_type == XCB_KEY_PRESS);
			YwKey key = _YwX11KeycodeToKey(ke->detail);

			switch (key) {
			case YW_KEY_LSHIFT:
			case YW_KEY_RSHIFT:
				if (pressed)
					w->tmp_x11_mod_state |= XCB_MOD_MASK_SHIFT;
				else
					w->tmp_x11_mod_state &= ~XCB_MOD_MASK_SHIFT;
				break;
			case YW_KEY_LCTRL:
			case YW_KEY_RCTRL:
				if (pressed)
					w->tmp_x11_mod_state |= XCB_MOD_MASK_CONTROL;
				else
					w->tmp_x11_mod_state &= ~XCB_MOD_MASK_CONTROL;
				break;
			case YW_KEY_LALT:
			case YW_KEY_RALT:
				if (pressed)
					w->tmp_x11_mod_state |= XCB_MOD_MASK_1;
				else
					w->tmp_x11_mod_state &= ~XCB_MOD_MASK_1;
				break;
			case YW_KEY_LSUPER:
			case YW_KEY_RSUPER:
				if (pressed)
					w->tmp_x11_mod_state |= XCB_MOD_MASK_4;
				else
					w->tmp_x11_mod_state &= ~XCB_MOD_MASK_4;
				break;
			default:
				break;
			}

			YwKeyState pressed_data = 0;
			if (pressed) {
				pressed_data = YW_KEY_PRESSED;
				if (w->tmp_x11_mod_state & XCB_MOD_MASK_SHIFT)
					pressed_data |= YW_KEYMOD_SHIFT;
				if (w->tmp_x11_mod_state & XCB_MOD_MASK_CONTROL)
					pressed_data |= YW_KEYMOD_CTRL;
				if (w->tmp_x11_mod_state & XCB_MOD_MASK_1)
					pressed_data |= YW_KEYMOD_ALT;
				if (w->tmp_x11_mod_state & XCB_MOD_MASK_4)
					pressed_data |= YW_KEYMOD_SUPER;
			}

			w->keys.current[key] = pressed_data;
			YwKeyEvent e = { .key = key, .pressed = pressed_data };
			YW_KEYBUF_PUSH(&w->key_buf, e);
			break;
		}
		}
		free(ev);
	}
#ifdef YAWL_EGL
	if (w->resized_internal) {
		w->resized_internal = false;
		if (w->egl_surface != EGL_NO_SURFACE) {
			s->e.make_current(w->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
			s->e.destroy_surface(w->egl_display, w->egl_surface);
		}

		w->egl_surface = s->e.create_window_surface(w->egl_display,
							    w->egl_config,
							    w->win, NULL);

		s->e.make_current(w->egl_display, w->egl_surface, w->egl_surface, w->egl_context);
	}
#endif //YAWL_EGL
}
#endif // YAWL_X11

#if defined(YAWL_WIN32)
#define YW_LOAD_SYMBOL(sym, lib, name)                                   \
	do {                                                             \
		FARPROC tmp = GetProcAddress(lib, name);                 \
		if (!tmp) {                                              \
			fprintf(stderr, "symbol not found: %s\n", name); \
			return false;                                    \
		}                                                        \
		*(void **)(&sym) = (void *)(uintptr_t)tmp;               \
	} while (0)

static bool _YwLoadGLProcWin32(YwState *s, void **proc, const char *name)
{
	void *p = (void *)(uintptr_t)s->wgl.get_proc_address(name);
	if (!p)
		p = (void *)(uintptr_t)(FARPROC)GetProcAddress(s->opengl32, name);
	*proc = p;
	return p != NULL;
}
#define YW_LOAD_WGL_FUNC(sym, name)                                          \
	do {                                                                 \
		void *_p = (void *)(uintptr_t)s->wgl.get_proc_address(name); \
		if (!_p) {                                                   \
			fprintf(stderr, "%s not available\n", name);         \
			return false;                                        \
		}                                                            \
		*(void **)(&sym) = _p;                                       \
	} while (0)
static bool _YwWGLLoadExtensions(YwState *s)
{
	if (!s->wgl.loaded)
		return false;

	YW_LOAD_WGL_FUNC(s->wgl.swap_interval, "wglSwapIntervalEXT");

	s->wgl.ext_loaded = true;
	return true;
}
static YwKey _YwScancodeToKey(uint16_t scancode)
{
	switch (scancode) {
	// Standard scancodes (no prefix)
	case 0x001e:
		return YW_KEY_A;
	case 0x0030:
		return YW_KEY_B;
	case 0x002e:
		return YW_KEY_C;
	case 0x0020:
		return YW_KEY_D;
	case 0x0012:
		return YW_KEY_E;
	case 0x0021:
		return YW_KEY_F;
	case 0x0022:
		return YW_KEY_G;
	case 0x0023:
		return YW_KEY_H;
	case 0x0017:
		return YW_KEY_I;
	case 0x0024:
		return YW_KEY_J;
	case 0x0025:
		return YW_KEY_K;
	case 0x0026:
		return YW_KEY_L;
	case 0x0032:
		return YW_KEY_M;
	case 0x0031:
		return YW_KEY_N;
	case 0x0018:
		return YW_KEY_O;
	case 0x0019:
		return YW_KEY_P;
	case 0x0010:
		return YW_KEY_Q;
	case 0x0013:
		return YW_KEY_R;
	case 0x001f:
		return YW_KEY_S;
	case 0x0014:
		return YW_KEY_T;
	case 0x0016:
		return YW_KEY_U;
	case 0x002f:
		return YW_KEY_V;
	case 0x0011:
		return YW_KEY_W;
	case 0x002d:
		return YW_KEY_X;
	case 0x0015:
		return YW_KEY_Y;
	case 0x002c:
		return YW_KEY_Z;

	case 0x000b:
		return YW_KEY_0;
	case 0x0002:
		return YW_KEY_1;
	case 0x0003:
		return YW_KEY_2;
	case 0x0004:
		return YW_KEY_3;
	case 0x0005:
		return YW_KEY_4;
	case 0x0006:
		return YW_KEY_5;
	case 0x0007:
		return YW_KEY_6;
	case 0x0008:
		return YW_KEY_7;
	case 0x0009:
		return YW_KEY_8;
	case 0x000a:
		return YW_KEY_9;

	case 0x0039:
		return YW_KEY_SPACE;
	case 0x001c:
		return YW_KEY_ENTER;
	case 0x000f:
		return YW_KEY_TAB;
	case 0x0001:
		return YW_KEY_ESCAPE;
	case 0x000e:
		return YW_KEY_BACKSPACE;

	case 0x0048:
		return YW_KEY_UP; // E0 prefix handled below, this is numpad without numlock
	case 0x0050:
		return YW_KEY_DOWN;
	case 0x004b:
		return YW_KEY_LEFT;
	case 0x004d:
		return YW_KEY_RIGHT;

	case 0x002a:
		return YW_KEY_LSHIFT;
	case 0x0036:
		return YW_KEY_RSHIFT;
	case 0x001d:
		return YW_KEY_LCTRL;
	case 0x0038:
		return YW_KEY_LALT;

	// Extended scancodes (E0 prefix)
	case 0xe048:
		return YW_KEY_UP;
	case 0xe050:
		return YW_KEY_DOWN;
	case 0xe04b:
		return YW_KEY_LEFT;
	case 0xe04d:
		return YW_KEY_RIGHT;

	case 0xe01d:
		return YW_KEY_RCTRL;
	case 0xe038:
		return YW_KEY_RALT;
	case 0xe05b:
		return YW_KEY_LSUPER; // Left Windows
	case 0xe05c:
		return YW_KEY_RSUPER; // Right Windows
	case 0xe05d:
		return YW_KEY_RSUPER; // App/Menu key (map to RSuper or add YW_KEY_MENU)
	default:
		return YW_KEY_UNKNOWN;
	}
}
static bool _YwWGLLoad(YwState *s)
{
	if (!s->opengl32) {
		s->opengl32 = LoadLibraryA("opengl32.dll");
		if (!s->opengl32)
			return false;
	}

	YW_LOAD_SYMBOL(s->wgl.get_proc_address, s->opengl32, "wglGetProcAddress");
	YW_LOAD_SYMBOL(s->wgl.create_context, s->opengl32, "wglCreateContext");
	YW_LOAD_SYMBOL(s->wgl.delete_context, s->opengl32, "wglDeleteContext");
	YW_LOAD_SYMBOL(s->wgl.make_current, s->opengl32, "wglMakeCurrent");
	YW_LOAD_SYMBOL(s->wgl.share_lists, s->opengl32, "wglShareLists");

	s->wgl.loaded = true;
	return true;
}

static LRESULT CALLBACK _YwWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	YwWindowData *w = (YwWindowData *)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

	switch (msg) {
	case WM_CREATE: {
		CREATESTRUCTA *cs = (CREATESTRUCTA *)lparam;
		SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
		return 0;
	}
	case WM_DESTROY:
		if (w)
			w->should_close = true;
		PostQuitMessage(0);
		return 0;
	case WM_SIZE:
		if (w) {
			size_t newwidth = LOWORD(lparam);
			size_t newheight = HIWORD(lparam);

			if (newwidth != w->width || newheight != w->height) {
				w->resized = true;
			}
			w->width = newwidth;
			w->height = newheight;
		}
		return 0;
	case WM_INPUT: {
		if (!w)
			return 0;

		UINT size = 0;
		// First call gets required size
		GetRawInputData((HRAWINPUT)lparam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));

// Stack alloc or use persistent buffer to avoid malloc per frame
// For simplicity, stack alloc with max known size (sizeof(RAWINPUT) is ~48 bytes)
#ifndef YW_RI_STACK_SIZE
#define YW_RI_STACK_SIZE 64
#endif // YW_RI_STACK_SIZE
		YW_STATIC_ASSERT(sizeof(RAWINPUT) <= YW_RI_STACK_SIZE, "RAWINPUT too big for stack");
		char stack_buf[YW_RI_STACK_SIZE];

		if (GetRawInputData((HRAWINPUT)lparam, RID_INPUT, stack_buf, &size, sizeof(RAWINPUTHEADER)) != size) {
			return 0;
		}

		RAWINPUT *ri = (RAWINPUT *)stack_buf;
		if (ri->header.dwType != RIM_TYPEKEYBOARD)
			return 0; // Not keyboard input

		RAWKEYBOARD *rk = &ri->data.keyboard;

		// Flags analysis
		bool is_e0 = (rk->Flags & RI_KEY_E0) != 0; // Extended key (right modifiers, arrows, numpad)
		bool is_e1 = (rk->Flags & RI_KEY_E1) != 0; // Extended key 1 (Pause/Break only)
		bool make = (rk->Flags & RI_KEY_BREAK) == 0; // Key press (not release)
		// bool was_down = (rk->Flags & RI_KEY_MAKE) == 0; // Only valid for some keyboards

		// Build scancode: standard (8-bit) or extended (0xe0 prefix)
		uint16_t scancode = rk->MakeCode;
		if (is_e0)
			scancode |= 0xe000;
		if (is_e1)
			scancode |= 0xe100; // Pause key

		// Map to your key enum using scancode tables (not VK)
		YwKey key = _YwScancodeToKey(scancode);

		// Get current modifier state from our own tracking (not GetKeyState)
		// We track these ourselves to avoid querying Windows
		bool shift_down = w->keys.current[YW_KEY_LSHIFT] || w->keys.current[YW_KEY_RSHIFT];
		bool ctrl_down = w->keys.current[YW_KEY_LCTRL] || w->keys.current[YW_KEY_RCTRL];
		bool alt_down = w->keys.current[YW_KEY_LALT] || w->keys.current[YW_KEY_RALT];
		bool win_down = w->keys.current[YW_KEY_LSUPER] || w->keys.current[YW_KEY_RSUPER];

		// Update modifier tracking for this specific key
		switch (key) {
		case YW_KEY_LSHIFT:
			shift_down = make;
			break;
		case YW_KEY_RSHIFT:
			shift_down = make;
			break;
		case YW_KEY_LCTRL:
			ctrl_down = make;
			break;
		case YW_KEY_RCTRL:
			ctrl_down = make;
			break;
		case YW_KEY_LALT:
			alt_down = make;
			break;
		case YW_KEY_RALT:
			alt_down = make;
			break;
		case YW_KEY_LSUPER:
			win_down = make;
			break;
		case YW_KEY_RSUPER:
			win_down = make;
			break;
		default:
			break;
		}

		// Build state
		YwKeyState pressed_data = 0;
		if (make) {
			pressed_data = YW_KEY_PRESSED;
			if (shift_down)
				pressed_data |= YW_KEYMOD_SHIFT;
			if (ctrl_down)
				pressed_data |= YW_KEYMOD_CTRL;
			if (alt_down)
				pressed_data |= YW_KEYMOD_ALT;
			if (win_down)
				pressed_data |= YW_KEYMOD_SUPER;
		}

		w->keys.current[key] = pressed_data;

		YwKeyEvent e = {
			.key = key,
			.pressed = pressed_data,
		};
		YW_KEYBUF_PUSH(&w->key_buf, e);

		// DefWindowProc not needed for WM_INPUT if NOLEGACY, but call anyway for cleanliness
		return DefWindowProcA(hwnd, msg, wparam, lparam);
	}
	case WM_SETFOCUS:
		if (w)
			w->focused = true;
		return 0;
	case WM_KILLFOCUS: {
		if (w) {
#ifndef YAWL_NO_UNFOCUS_INPUT_RELEASE
			_YwUnfocusInputRelease(w);
#endif // YAWL_NO_UNFOCUS_INPUT_RELEASE
			w->focused = false;
		}
	}
		return 0;
	}
	return DefWindowProcA(hwnd, msg, wparam, lparam);
}
#include <hidusage.h>
static bool _YwInitWindowWin32(YwWindowData *w, const char *name)
{
	YwState *s = w->state;
	s->hinstance = GetModuleHandleA(NULL);

	if (w->width == 0 || w->height == 0) {
		HMONITOR hmon = MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY);
		MONITORINFO mi = { sizeof(mi) };
		if (GetMonitorInfoA(hmon, &mi)) {
			w->width = mi.rcWork.right - mi.rcWork.left;
			w->height = mi.rcWork.bottom - mi.rcWork.top;
		} else {
			w->width = GetSystemMetrics(SM_CXSCREEN);
			w->height = GetSystemMetrics(SM_CYSCREEN);
		}
	}
	WNDCLASSA wc = { 0 };
	wc.lpfnWndProc = _YwWndProc;
	wc.hInstance = s->hinstance;
	wc.lpszClassName = "YwWindowClass";
	// wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	RegisterClassA(&wc);

	w->hwnd = CreateWindowExA(
		0, "YwWindowClass", name,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		(int)w->width, (int)w->height,
		NULL, NULL, s->hinstance, w);

	if (!w->hwnd)
		return false;
	RAWINPUTDEVICE rid = { 0 };
	rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
	rid.usUsage = HID_USAGE_GENERIC_KEYBOARD;
	rid.dwFlags = RIDEV_NOLEGACY; // No WM_KEY* messages, no hotkeys, no key repeat
	rid.hwndTarget = w->hwnd;

	if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
		fprintf(stderr, "Failed to register raw input\n");
		return false;
	}

	ShowWindow(w->hwnd, SW_SHOW);
	UpdateWindow(w->hwnd);
	w->hdc = GetDC(w->hwnd);
	if (!_YwWGLLoad(s))
		return false;
	PIXELFORMATDESCRIPTOR pfd = { 0 };
	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 24;
	pfd.cStencilBits = 8;
	pfd.iLayerType = PFD_MAIN_PLANE;

	int pf = ChoosePixelFormat(w->hdc, &pfd);
	SetPixelFormat(w->hdc, pf, &pfd);

	w->gl_context = s->wgl.create_context(w->hdc);
	s->wgl.make_current(w->hdc, w->gl_context);

	if (!s->wgl.ext_loaded)
		if (!_YwWGLLoadExtensions(s))
			return false;

	s->wgl.swap_interval(0);
	return true;
}

static void _YwPollEventsWin32(YwWindowData *w)
{
	MSG msg;
	while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) {
			w->should_close = true;
		}
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}
}
#endif //YAWL_WIN32

static bool _YwInit(YwState *s)
{
	s->initialized = true;
	return true;
}

YW_EXPORT void YwPollEvents(YwWindowData *w)
{
	YW_STATIC_ASSERT(sizeof(w->keys.prev) == sizeof(w->keys.current), "the hell did you do");
	YwMemcpy(w->keys.prev, w->keys.current, sizeof(w->keys.prev));
#ifdef YAWL_X11
	_YwPollEventsX11(w);
#elif defined(YAWL_WIN32)
	_YwPollEventsWin32(w);
#elif defined(YAWL_ANDROID)
	_YwPollEventsAndroid(w);
#else
#warning Unsupported platform
	fprintf(stderr, "Yawl: Unsupported platform\n");
#endif
}

YW_EXPORT bool YwInitWindow(YwState *s, YwWindowData *w, const char *name)
{
	if (!s->initialized) {
		if (!_YwInit(s))
			return false;
	}
	w->state = s;
#ifdef YAWL_X11
	return _YwInitWindowX11(w, name);
#elif defined(YAWL_WIN32)
	return _YwInitWindowWin32(w, name);
#elif defined(YAWL_ANDROID)
	return _YwInitWindowAndroid(w, name);
#else
#warning Unsupported platform
	fprintf(stderr, "Yawl: Unsupported platform\n");
	return false;
#endif
}
YW_EXPORT void YwBeginDrawing(YwWindowData *w)
{
	(void)w;
}
YW_EXPORT void YwGLMakeCurrent(YwWindowData *w)
{
	YwState *s = w->state;
#ifdef YAWL_EGL
	s->e.make_current(w->egl_display, w->egl_surface, w->egl_surface, w->egl_context);
#elif defined(YAWL_WIN32)
	s->wgl.make_current(w->hdc, w->gl_context);
#else
#warning Unsupported platform
	fprintf(stderr, "Yawl: Unsupported platform\n");
#endif // YAWL_WIN32
}
YW_EXPORT void YwEndDrawing(YwWindowData *w)
{
	YwState *s = w->state;
	// s->x.flush(w->conn);
#ifdef YAWL_EGL
	s->e.swap_buffers(w->egl_display, w->egl_surface);
#elif defined(YAWL_WIN32)
	(void)s;
	SwapBuffers(w->hdc);
#else
#warning Unsupported platform
	fprintf(stderr, "Yawl: Unsupported platform\n");
#endif // YAWL_WIN32
}

YW_EXPORT bool YwGLLoadProc(YwState *s, void **proc, const char *name)
{
#ifdef YAWL_WIN32
	return _YwLoadGLProcWin32(s, proc, name);
#elif defined(YAWL_EGL)
	return _YwLoadGLProcEGL(s, proc, name);
#else
#warning Unsupported platform
	fprintf(stderr, "Yawl: Unsupported platform\n");
	return false;
#endif // YAWL_X11
}

YW_EXPORT bool YwNextKeyEvent(YwWindowData *w, YwKeyEvent *out)
{
	struct YwKeyBuffer *kb = &w->key_buf;
	if (kb->read == kb->write)
		return false;
	*out = kb->events[kb->read];
	kb->read = (kb->read + 1) % YW_KEYBUF_SIZE;
	return true;
}

YW_EXPORT void YwSetVSync(YwWindowData *w, bool enabled)
{
	YwState *s = w->state;
	int interval = enabled ? 1 : 0;
#ifdef YAWL_EGL
	if (s->e.loaded)
		s->e.swap_interval(w->egl_display, interval);
#elif defined(YAWL_WIN32)
	if (s->wgl.loaded)
		s->wgl.swap_interval(interval);
#else
#warning Unsupported platform
	fprintf(stderr, "Yawl: Unsupported platform\n");
#endif
}

#endif // YAWL_IMPLEMENTATION
