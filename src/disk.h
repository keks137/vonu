#ifndef INCLUDE_SRC_DISK_H_
#define INCLUDE_SRC_DISK_H_

#include "chunk.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#define CHUNK_HEADER_MAGIC 0x4B4E4843
#define CHUNK_VERSION 1

#pragma pack(push, 1)
typedef struct {
	uint32_t magic;
	uint32_t version;
	ChunkCoord coord;
	uint64_t chunk_total_x;
	uint64_t chunk_total_y;
	uint64_t chunk_total_z;
	uint64_t timestamp;
	uint8_t reserved[16];
} ChunkFileHeader;
#pragma pack(pop)

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define PATH_SEPARATOR '\\'
#define PATH_SEPARATOR_STR "\\"
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#include <dirent.h>
#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR_STR "/"
#endif

typedef struct {
	char worlds_dir[256];
	char chunks_dir[256];
} WorldPaths;

bool create_directory_recursive(const char *path);
bool world_paths_init(WorldPaths *paths, const char *world_name);
const char *get_world_chunk_path(const char *world_name, int x, int y, int z, char *buffer, size_t size);
bool file_exists(const char *path);

bool disk_save(Chunk *chunk, const char *world_uid);
bool disk_init(const char *world_uid);
bool disk_load(Chunk *chunk, const char *world_uid);
bool disk_find(const ChunkCoord *coord, const char *world_uid);

#endif // INCLUDE_SRC_DISK_H_
