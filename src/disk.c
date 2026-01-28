#include "chunk.h"
#include <inttypes.h>
#include "logs.h"
#include "vassert.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "disk.h"
#include "strb.h"
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define MKDIR(path) _mkdir(path)
#define ACCESS(path, mode) _access(path, mode)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0755)
#define ACCESS(path, mode) access(path, mode)
#endif

bool create_directory_recursive(const char *path)
{
	char tmp[256];
	char *p = NULL;
	size_t len;

	snprintf(tmp, sizeof(tmp), "%s", path);
	len = strlen(tmp);

	if (tmp[len - 1] == PATH_SEPARATOR) {
		tmp[len - 1] = 0;
	}

	for (p = tmp + 1; *p; p++) {
		if (*p == PATH_SEPARATOR) {
			*p = 0;

			if (ACCESS(tmp, 0) != 0) {
				if (MKDIR(tmp) != 0 && errno != EEXIST) {
					return false;
				}
			}

			*p = PATH_SEPARATOR;
		}
	}

	if (ACCESS(tmp, 0) != 0) {
		return MKDIR(tmp) == 0 || errno == EEXIST;
	}

	return true;
}

bool file_exists(const char *path)
{
	return ACCESS(path, 0) == 0;
}

void get_worlds_dir_path(char *buf, size_t buf_size)
{
	strbcpy(buf, "worlds", buf_size);
	strbcat(buf, PATH_SEPARATOR_STR, buf_size);
}

void get_chunks_dir_path(char *buf, size_t buf_size, const char *world_uid)
{
	get_worlds_dir_path(buf, buf_size);
	strbcat(buf, world_uid, buf_size);
	strbcat(buf, PATH_SEPARATOR_STR, buf_size);
	strbcat(buf, "chunks", buf_size);
	strbcat(buf, PATH_SEPARATOR_STR, buf_size);
}

void ensure_chunk_dir(const char *world_uid)
{
	static bool checked = false;
	static bool found = false;
	if (found)
		return;
	if (!checked) {
		checked = true;
		char dir[256];
		get_chunks_dir_path(dir, sizeof(dir), world_uid);
		if (MKDIR(dir) == 0 || errno == EEXIST) {
			found = true;

			return;
		}
	}
	VERROR("Chunk dir not found: %s", world_uid);
}
static void build_chunk_filename(const ChunkCoord *coord, char *buf, size_t buf_size, const char *world_uid)
{
	get_chunks_dir_path(buf, buf_size, world_uid);
	char filename[69];
	snprintf(filename, sizeof(filename), "%" SCNd64 "_%" SCNd64 "_%" SCNd64 ".chunk", coord->x, coord->y, coord->z);
	strbcat(buf, filename, buf_size);
}
bool disk_find(const ChunkCoord *coord, const char *world_uid)
{
	char filename[256];
	build_chunk_filename(coord, filename, sizeof(filename), world_uid);
	return file_exists(filename);
}
bool disk_save(Chunk *chunk, const char *world_uid)
{
	// print_chunk(chunk);
	VASSERT(chunk != NULL);
	VASSERT(chunk->data != NULL);
	VASSERT(chunk->block_count > 0);
	VASSERT(chunk->modified);

	// chunk->modified = false; // NOTE: this might be a bad idea

	char filename[256];
	build_chunk_filename(&chunk->coord, filename, sizeof(filename), world_uid);

	FILE *file = fopen(filename, "wb");
	if (!file) {
		VERROR("Failed to open %s for writing: %s", filename, strerror(errno));
		return false;
	}

	ChunkFileHeader header = {
		.magic = CHUNK_HEADER_MAGIC,
		.version = CHUNK_VERSION,
		.coord = chunk->coord,
		.chunk_total_x = CHUNK_TOTAL_X,
		.chunk_total_y = CHUNK_TOTAL_Y,
		.chunk_total_z = CHUNK_TOTAL_Z,
		.timestamp = (uint64_t)time(NULL),
		.reserved = { 0 }
	};

	if (fwrite(&header, sizeof(header), 1, file) != 1) {
		VERROR("Failed to write header to %s", filename);
		fclose(file);
		return false;
	}

	size_t blocks_written = 0;
	for (size_t i = 0; i < CHUNK_TOTAL_BLOCKS; i++) {
		BLOCKTYPE data = chunk->data[i].type;
		blocks_written += fwrite(&data, sizeof(data), 1, file);
	}

	if (ferror(file)) {
		VERROR("Write error for %s: %s", filename, strerror(errno));
		fclose(file);
		return false;
	}

	fclose(file);

	if (blocks_written != CHUNK_TOTAL_BLOCKS) {
		VERROR("Partial write to %s: %zu/%u blocks",
		       filename, blocks_written, CHUNK_TOTAL_BLOCKS);
		return false;
	}

	// VINFO("Saved chunk %d,%d,%d to %s", chunk->coord.x, chunk->coord.y, chunk->coord.z, filename);

	return true;
}
bool disk_load(Chunk *chunk, const char *world_uid)
{
	VASSERT(chunk->data != NULL);
	char filename[256];
	build_chunk_filename(&chunk->coord, filename, sizeof(filename), world_uid);

	if (!file_exists(filename)) {
		return false;
	}

	FILE *file = fopen(filename, "rb");
	if (!file) {
		VERROR("Failed to open %s for reading: %s", filename, strerror(errno));
		return false;
	}

	ChunkFileHeader header;
	if (fread(&header, sizeof(header), 1, file) != 1) {
		VERROR("Failed to read header from %s", filename);
		fclose(file);
		return false;
	}

	if (header.magic != CHUNK_HEADER_MAGIC) {
		VERROR("Invalid chunk file magic %s: %X, expected %X",
		       filename, header.magic, CHUNK_HEADER_MAGIC);
		fclose(file);
		return false;
	}

	if (header.version != CHUNK_VERSION) {
		VWARN("Chunk %s version mismatch: %u (expected %u)",
		      filename, header.version, CHUNK_VERSION);
	}

	if (header.coord.x != chunk->coord.x ||
	    header.coord.y != chunk->coord.y ||
	    header.coord.z != chunk->coord.z) {
		VERROR("Chunk coordinate mismatch in %s: file has %d,%d,%d, requested %d,%d,%d",
		       filename, header.coord.x, header.coord.y, header.coord.z,
		       chunk->coord.x, chunk->coord.y, chunk->coord.z);
		fclose(file);
		return false;
	}

	if (header.chunk_total_x != CHUNK_TOTAL_X ||
	    header.chunk_total_y != CHUNK_TOTAL_Y ||
	    header.chunk_total_z != CHUNK_TOTAL_Z) {
		VWARN("Chunk dimensions mismatch in %s: %ux%ux%u (expected %ux%ux%u)",
		      filename, header.chunk_total_x, header.chunk_total_y, header.chunk_total_z,
		      CHUNK_TOTAL_X, CHUNK_TOTAL_Y, CHUNK_TOTAL_Z);
	}

	// TODO: not one at a time
	size_t blocks_read = 0;
	size_t block_count = 0;
	for (size_t i = 0; i < CHUNK_TOTAL_BLOCKS; i++) {
		BLOCKTYPE data = 0;
		blocks_read += fread(&data, sizeof(data), 1, file);
		chunk->data[i].type = data;
		// TODO: proper handling of block type metadata
		if (data == BlocktypeGrass || data == BlocktypeStone) {
			chunk->data[i].obstructing = true;
			block_count++;
		}
	}

	if (ferror(file)) {
		VERROR("Read error from %s: %s", filename, strerror(errno));
		fclose(file);
		return false;
	}

	fclose(file);

	if (blocks_read != CHUNK_TOTAL_BLOCKS) {
		VERROR("Incomplete read from %s: %zu/%u blocks",
		       filename, blocks_read, CHUNK_TOTAL_BLOCKS);
		return false;
	}

	chunk->block_count = block_count;
	chunk->terrain_generated = true;
	chunk->up_to_date = false;
	// chunk->last_used = time(NULL);

	return true;
}
bool disk_init(const char *world_uid)
{
	char chunks_dir[256];

	get_chunks_dir_path(chunks_dir, sizeof(chunks_dir), world_uid);

	if (!create_directory_recursive(chunks_dir)) {
		VERROR("Failed to create world directory for: %s", world_uid);
		return false;
	}
	return true;
}

static const char *get_pathless_file(const char *filename)
{
	const char *last_separator = filename;
	while (*filename) {
		if (*filename == PATH_SEPARATOR)
			last_separator = filename;
		filename++;
	}
	return last_separator + 1;
}

bool parse_chunk_filename(const char *filename, ChunkCoord *coord)
{
	ChunkCoord got = { 0 };
	const char *file = get_pathless_file(filename);

	if (sscanf(file, "%" SCNd64 "_%" SCNd64 "_%" SCNd64 ".chunk", &got.x, &got.y, &got.z) == 3) {
		coord->x = got.x;
		coord->y = got.y;
		coord->z = got.z;
		return true;
	}

	return false;
}
