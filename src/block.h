#ifndef INCLUDE_SRC_BLOCK_H_
#define INCLUDE_SRC_BLOCK_H_

#include <stdbool.h>
#include "../vendor/cglm/cglm.h"
#include <stdint.h>
typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
} Color;

#define BLOCKTYPE_NAMES   \
	X(BlocktypeAir)   \
	X(BlocktypeGrass) \
	X(BlocktypeStone)

typedef enum {
#define X(name) name,
	BLOCKTYPE_NAMES
#undef X
} BLOCKTYPE;



extern const char *BlockTypeString[];

typedef uint32_t BlockLight; // rgb 1 byte each, range only 4 bits
typedef struct {
	BLOCKTYPE type;
	bool obstructing;
	bool light_source;
	BlockLight light;
} Block;
typedef struct {
	int8_t x;
	int8_t y;
	int8_t z;
} BlockPos;


BlockLight color_and_range_to_blocklight(Color color, uint8_t range);
void block_make_light(Block *block, Color color, uint8_t range);

#endif  // INCLUDE_SRC_BLOCK_H_
