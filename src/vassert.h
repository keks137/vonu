#ifndef INCLUDE_SRC_ASSERTS_H_
#define INCLUDE_SRC_ASSERTS_H_

#include <stdint.h>
#include <stdlib.h>

#ifdef VABORT_DEBUG
#if _MSC_VER
#include <intrin.h>
#define VABORT() __debugBreak()
#else
#define VABORT() __builtin_trap()
#endif

#else

#define VABORT() abort()
#endif //VABORT_DEBUG

void log_assertion_failure(const char *expression, const char *message, const char *file, int32_t line, const char *func);
void log_assertion_warn(const char *expression, const char *message, const char *file, int32_t line, const char *func);

// for when you want to assert in release builds
#if defined(__GNUC__) || defined(__clang__)
#define VLIKELY(x) __builtin_expect(!!(x), 1)
#define VUNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define VLIKELY(x) (x)
#define VUNLIKELY(x) (x)
#endif

#define VPANIC(expr)                                                                      \
	do {                                                                              \
		if (VUNLIKELY(!(expr))) {                                                 \
			log_assertion_failure(#expr, NULL, __FILE__, __LINE__, __func__); \
			VABORT();                                                         \
		}                                                                         \
	} while (0)

#define VPANIC_MSG(expr, msg)                                                            \
	do {                                                                             \
		if (VUNLIKELY(!(expr))) {                                                \
			log_assertion_failure(#expr, msg, __FILE__, __LINE__, __func__); \
			VABORT();                                                        \
		}                                                                        \
	} while (0)

#ifndef NDEBUG
#define VASSERT(expr) VPANIC(expr)
#define VASSERT_MSG(expr, msg) VPANIC_MSG(expr, msg)
#define VASSERT_WARN(expr)                                                             \
	do {                                                                           \
		if (VUNLIKELY(!(expr))) {                                              \
			log_assertion_warn(#expr, NULL, __FILE__, __LINE__, __func__); \
		}                                                                      \
	} while (0)
#define VASSERT_WARN_MSG(expr, msg)                                                   \
	do {                                                                          \
		if (VUNLIKELY(!(expr))) {                                             \
			log_assertion_warn(#expr, msg, __FILE__, __LINE__, __func__); \
		}                                                                     \
	} while (0)

#else
#define VASSERT(expr) ((void)0)
#define VASSERT_MSG(expr, msg) ((void)0)
#define VASSERT_WARN(expr) ((void)0)
#define VASSERT_WARN_MSG(expr, msg) ((void)0)
#define VALWAYS(expr) expr
#define VNEVER(expr) expr

#endif //NDEBUG

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define VASSERT_STATIC(expr, msg) _Static_assert(expr, msg)

#elif defined(__cplusplus) && __cplusplus >= 201103L
#define VASSERT_STATIC(expr, msg) static_assert(expr, msg)

#else
#define VASSERT_STATIC(expr, msg) \
	typedef char VASSERT_STATIC_##__LINE__[(expr) ? 1 : -1]
#endif

#define VASSERT_STATIC_NOMSG(expr) VASSERT_STATIC(expr, "static assertion failed")

#ifdef NDEBUG
#define GL_CHECK(stmt) \
	do {           \
		stmt;  \
	} while (0)
#else
#define GL_CHECK(stmt)                                                                                            \
	do {                                                                                                      \
		stmt;                                                                                             \
		GLenum err = glGetError();                                                                        \
		if (err != GL_NO_ERROR) {                                                                         \
			VERROR("OpenGL error 0x%04X at %s:%d: %s: %s", err, __FILE__, __LINE__, __func__, #stmt); \
		}                                                                                                 \
	} while (0)
#endif // NDEBUG

#endif // INCLUDE_SRC_ASSERTS_H_
