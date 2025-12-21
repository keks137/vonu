#include "atlas.h"
#include <assert.h>
#include <stddef.h>

void atlas_register(Atlas *atlas)
{
	glGenTextures(1, &atlas->tex);
	glBindTexture(GL_TEXTURE_2D, atlas->tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas->img.width, atlas->img.height,
		     0, GL_RGBA, GL_UNSIGNED_BYTE, atlas->img.data);
}

// bool atlas_add(Atlas *atlas, Image *image, AtlasRegion *region)
// {}

// void atlas_bind(Atlas *atlas)
// {
// }
