#include "vassert.h"
#include "block.h"
static void color_to_vec3(Color c, vec3 out)
{
	out[0] = (float)c.r / 255.0f;
	out[1] = (float)c.g / 255.0f;
	out[2] = (float)c.b / 255.0f;
}

static void blocklight_to_vec3(BlockLight light, vec3 out)
{
	out[0] = (float)((light >> 8 * 3) & 0xFF) / 255.0f;
	out[1] = (float)((light >> 8 * 2) & 0xFF) / 255.0f;
	out[2] = (float)((light >> 8 * 1) & 0xFF) / 255.0f;
}

BlockLight color_and_range_to_blocklight(Color color, uint8_t range)
{
	BlockLight light = 0;
	light |= (range & 0xF) << 8 * 0; // stored as 4 bits
	light |= color.r << 8 * 1;
	light |= color.g << 8 * 2;
	light |= color.b << 8 * 3;
	return light;
}

void block_make_light(Block *block, Color color, uint8_t range)
{
	VASSERT(range < 16);
	block->light_source = true;
	block->light = 0;
	block->light = color_and_range_to_blocklight(color, range);
}
