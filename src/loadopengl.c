#ifdef __ANDROID__
#elif defined(PLATFORM_GLFW)

#include "loadopengl.h"
#include "logs.h"

#define LOAD_GL_FUNC(type, name)                      \
	name = (type)glfwGetProcAddress(#name);       \
	if (!name) {                                  \
		VERROR("Failed to load %s\n", #name); \
		return false;                         \
	}

#include "platform/loadopengl.c"
#else
#include "loadopengl.h"
#include "core.h"
#include "logs.h"
#include <yawl.h>
#define LOAD_GL_FUNC(type, name)                                             \
	do {                                                                 \
		bool ret = YwGLLoadProc(&glob_state, (void **)&name, #name); \
		if (!ret) {                                                  \
			VERROR("Failed to load %s\n", #name);                \
			return false;                                        \
		}                                                            \
	} while (0)

#include "platform/loadopengl.c"

#endif
