#include "logs.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#define STRB_IMPLEMENTATION
#include "strb.h"
static const char *level_strings[6] = { "[FATAL]: ", "[ERROR]: ", "[WARN]: ", "[INFO]: ", "[DEBUG]: ", "[TRACE]: " };

#define LOG_MAX_MESSAGE_LEN 1024

void log_assertion_failure(const char *expression, const char *message, const char *file, int32_t line, const char *func)
{
	if (message == NULL) {
		log_msgn(LOG_LEVEL_FATAL, "%s:%d: %s: Assertion '%s' failed", file, line, func, expression);
	} else {
		log_msgn(LOG_LEVEL_FATAL, "%s:%d: %s: Assertion '%s' failed: '%s'", file, line, func, expression, message);
	}
}
void log_assertion_warn(const char *expression, const char *message, const char *file, int32_t line, const char *func)
{
	if (message == NULL) {
		log_msgn(LOG_LEVEL_WARN, "%s:%d: %s: Assertion '%s' failed", file, line, func, expression);
	} else {
		log_msgn(LOG_LEVEL_WARN, "%s:%d: %s: Assertion '%s' failed: '%s'", file, line, func, expression, message);
	}
}
void log_msgn(LOG_LEVEL level, const char *message, ...)
{
	bool is_error = level < 2;
	char buffer[LOG_MAX_MESSAGE_LEN];

	size_t pos = strblcpy(buffer, level_strings[level], sizeof(buffer));

	va_list arg_ptr;
	va_start(arg_ptr, message);
	vsnprintf(buffer + pos, sizeof(buffer) - pos, message, arg_ptr);
	va_end(arg_ptr);

	FILE *outfile = is_error ? stderr : stdout;
	fprintf(outfile, "%s\n", buffer);
}
