#ifndef TEXTURE_ATLAS_H
#define TEXTURE_ATLAS_H

<<<<<<< HEAD
#include <GL/gl.h>
=======
#include "loadopengl.h"
>>>>>>> ORIG_HEAD
#include "image.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
	GLuint tex;
	Image img;
	size_t regions;
} Atlas;

typedef struct {
	float u1, v1; /* top-left */
	float u2, v2; /* bottom-right */
	size_t width, height;
} AtlasRegion;

void atlas_register(Atlas *atlas);

bool atlas_add(Atlas *atlas, Image *image, AtlasRegion *region);

void atlas_bind(Atlas *atlas);
#endif // TEXTURE_ATLAS_H
