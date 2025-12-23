#ifndef INCLUDE_SRC_ASSERTS_H_
#define INCLUDE_SRC_ASSERTS_H_


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

#include <stdint.h>
void log_assertion_failure(const char *expression, const char *message, const char *file, int32_t line);

#define VASSERT(expr)                                                           \
	do {                                                                    \
		if (expr) {                                                     \
		} else {                                                        \
			log_assertion_failure(#expr, NULL, __FILE__, __LINE__); \
			VABORT();                                               \
		}                                                               \
	} while (0)
#define VASSERT_MSG(expr, msg)                                                 \
	do {                                                                   \
		if (expr) {                                                    \
		} else {                                                       \
			log_assertion_failure(#expr, msg, __FILE__, __LINE__); \
			VABORT();                                              \
		}                                                              \
	} while (0)

#endif // INCLUDE_SRC_ASSERTS_H_
