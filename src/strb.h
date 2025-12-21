/* This library is licensed under BSD-0. The license can be found at the end of this file. */
#ifndef STRB_H_
#define STRB_H_
/*
 * string library with a focus on null-terminated string buffers
 *
 * usage example:
   #include <stdlib.h>
   #include <stdio.h>
   #define STRB_IMPLEMENTATION
   #include "strb.h"

   int main()
   {
	const char *src = "World!";
	size_t dstbufsize = 20;
	char *dst = calloc(dstbufsize, sizeof(char));
	strbcpy(dst, "Hello, ", dstbufsize);
	strbcat(dst, src, dstbufsize);
	printf("dst: %s\n", dst);
   }
 *
 */
#include <stddef.h>
/*
 * strlen,
 * also includes the \0 character
 */
size_t strblen(const char *str);

/* 
 * strcpy, 
 * dstbufsize is the size of the dst buffer,
 * return val is the size of a buffer that could hold the whole string,
 * so if dstbufsize < return there was concatenation
 */
size_t strbcpy(char *dst, const char *src, size_t dstbufsize);

/* 
 * strcat, 
 * dstbufsize is the size of the dst buffer,
 * return val is the size of a buffer that could hold the whole string,
 * so if dstbufsize < return there was concatenation
 */
size_t strbcat(char *dst, const char *src, size_t dstbufsize);

/*
 * strblcpy,
 * replacement for BSD's strlcpy for any platform
 * just strbcpy but the return is -1
 */
size_t strblcpy(char *dst, const char *src, size_t dstbufsize);
/*
 * strblcat,
 * replacement for BSD's strlcat for any platform
 * just strbcat but the return is -1
 */
size_t strblcat(char *dst, const char *src, size_t dstbufsize);

#ifdef STRB_IMPLEMENTATION

size_t strblen(const char *str)
{
	const char *s = str;
	for (; *s != '\0'; ++s)
		;
	return (s - str + 1); // +1 for \0
}

size_t strbcpy(char *dst, const char *src, size_t dstbufsize)
{
	const char *osrc = src;
	if (dstbufsize == 0)
		return strblen(src);
	size_t dleft = dstbufsize - 1; // -1 for \0

	while (*src != '\0') {
		if (dleft > 0) {
			*dst = *src;
			++dst;
			--dleft;
		}
		++src;
	}

	*dst = '\0';
	const size_t srclen = src - osrc;
	size_t totlen = srclen + 1; // +1 for \0
	return totlen;
}
size_t strbcat(char *dst, const char *src, size_t dstbufsize)
{
	const char *osrc = src;
	const char *odst = dst;

	if (dstbufsize == 0)
		return strblen(src);
	size_t dleft = dstbufsize - 1; // -1 for \0

	while (*dst != '\0' && dleft != 0) {
		++dst;
		--dleft;
	}
	const size_t dstlen = dst - odst;

	while (*src != '\0') {
		if (dleft > 0) {
			*dst = *src;
			++dst;
			--dleft;
		}
		++src;
	}

	*dst = '\0';
	const size_t srclen = src - osrc;
	size_t totlen = srclen + dstlen + 1; // +1 for \0
	return totlen;
}

size_t strblcpy(char *dst, const char *src, size_t dstbufsize)
{
	return strbcpy(dst, src, dstbufsize) - 1;
}

size_t strblcat(char *dst, const char *src, size_t dstbufsize)
{
	return strbcat(dst, src, dstbufsize) - 1;
}

#endif //STRB_IMPLEMENTATION
#endif //STRB_H_
/*
Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
