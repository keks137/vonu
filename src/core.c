#include "core.h"
#ifdef __ANDROID__
#include "platform/android_core.c"
#elif defined(PLATFORM_GLFW)
#include "image.h"
#include "loadopengl.h"
#include "profiling.h"
#include "logs.h"
#include "game.h"

#include "types.h"


#include "platform/time.c"
#include "platform/glfw_core.c"
#else
#define YAWL_IMPLEMENTATION
#include <yawl.h>
#include "game.h"
#include "loadopengl.h"
#include "profiling.h"
#include "platform/time.c"
#include "platform/yawl_core.c"
#endif
