#include "vassert.h"
#include "block.h"

BlockLight color_to_blocklight(Color color)
{
	BlockLight light = 0;
	light |= (color.r / 16 & 0xF) << 4 * 0;
	light |= (color.g / 16 & 0xF) << 4 * 1;
	light |= (color.b / 16 & 0xF) << 4 * 2;
	return light;
}

void block_make_light(Block *block, Color color)
{
	block->light_source = true;
	block->light = 0;
	block->light = color_to_blocklight(color);
}
