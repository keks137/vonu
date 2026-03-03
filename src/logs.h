#ifndef INCLUDE_SRC_LOGS_H_
#define INCLUDE_SRC_LOGS_H_

#define LOG_WARN_ENABLED 1
#define LOG_INFO_ENABLED 1
#define LOG_DEBUG_ENABLED 1
#define LOG_TRACE_ENABLED 1

typedef enum {
	LOG_LEVEL_FATAL = 0,
	LOG_LEVEL_ERROR = 1,
	LOG_LEVEL_WARN = 2,
	LOG_LEVEL_INFO = 3,
	LOG_LEVEL_DEBUG = 4,
	LOG_LEVEL_TRACE = 5,
} LOG_LEVEL;

void log_msgn(LOG_LEVEL level, const char *message, ...);

#define VFATAL(message, ...) \
	log_msgn(LOG_LEVEL_FATAL, message, ##__VA_ARGS__);
#define VERROR(message, ...) \
	log_msgn(LOG_LEVEL_ERROR, message, ##__VA_ARGS__);

#if LOG_WARN_ENABLED == 1
#define VWARN(message, ...) log_msgn(LOG_LEVEL_WARN, message, ##__VA_ARGS__);
#else
#define VWARN(message, ...)
#endif

#if LOG_INFO_ENABLED == 1
#define VINFO(message, ...) log_msgn(LOG_LEVEL_INFO, message, ##__VA_ARGS__);
#else
#define VINFO(message, ...)
#endif

#if LOG_DEBUG_ENABLED == 1
#define VDEBUG(message, ...) \
	log_msgn(LOG_LEVEL_DEBUG, message, ##__VA_ARGS__);
#else
#define VDEBUG(message, ...)
#endif

#if LOG_TRACE_ENABLED == 1
#define VTRACE(message, ...) \
	log_msgn(LOG_LEVEL_TRACE, message, ##__VA_ARGS__);
#else
#define VTRACE(message, ...)
#endif

#endif // INCLUDE_SRC_LOGS_H_
