#ifndef INCLUDE_SRC_IMAGE_H_
#define INCLUDE_SRC_IMAGE_H_

#include <stddef.h>
typedef struct {
	unsigned char *data;
	size_t width;
	size_t height;
	size_t n_chan;

} Image;


#endif  // INCLUDE_SRC_IMAGE_H_
