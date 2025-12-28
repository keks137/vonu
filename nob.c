#include <stdlib.h>
#define NOB_IMPLEMENTATION
#include "nob.h"

#define SRC_DIR "src/"
#define BUILD_DIR "build/"
#define BIN_DIR "bin/"

void append_source_files(Nob_Cmd *cmd)
{
	nob_cc_inputs(cmd, SRC_DIR "main.c");
	nob_cc_inputs(cmd, SRC_DIR "atlas.c");
	nob_cc_inputs(cmd, SRC_DIR "oglpool.c");
	nob_cc_inputs(cmd, SRC_DIR "disk.c");
	nob_cc_inputs(cmd, SRC_DIR "pool.c");
	nob_cc_inputs(cmd, SRC_DIR "chunk.c");
	nob_cc_inputs(cmd, SRC_DIR "logs.c");
	nob_cc_inputs(cmd, SRC_DIR "map.c");
	nob_cc_inputs(cmd, SRC_DIR "loadopengl.c");
}
void linux_mingw_glfw_flags(Nob_Cmd *cmd)
{
	nob_cmd_append(cmd, "-ggdb");
	nob_cmd_append(cmd, "-DPLATFORM_DESKTOP");
	nob_cmd_append(cmd, "-I", "./vendor/glfw/include/");
}
void linux_glfw_flags(Nob_Cmd *cmd)
{
	nob_cmd_append(cmd, "-ggdb");
	// nob_cmd_append(cmd, "-std=c99");
	// nob_cmd_append(cmd, "-O2");
	// nob_cmd_append(cmd, "-fsanitize=address,undefined"); // soo useful
	nob_cmd_append(cmd, "-DPLATFORM_DESKTOP");
	nob_cmd_append(cmd, "-D_GLFW_X11");
	nob_cmd_append(cmd, "-I", "./vendor/glfw/include/");
	nob_cmd_append(cmd, "-lm");
}

bool linux_mingw_build_glfw(Nob_Cmd *cmd)
{
	nob_cmd_append(cmd, "x86_64-w64-mingw32-gcc");
	linux_mingw_glfw_flags(cmd);
	nob_cmd_append(cmd, "-c");
	nob_cc_inputs(cmd, "vendor/rglfw.c");
	nob_cc_output(cmd, BUILD_DIR "rglfwmingw.o");

	if (!nob_cmd_run(cmd))
		return false;
	return true;
}
bool linux_build_glfw(Nob_Cmd *cmd)
{
	nob_cc(cmd);
	linux_glfw_flags(cmd);
	nob_cmd_append(cmd, "-c");
	nob_cc_inputs(cmd, "vendor/rglfw.c");
	nob_cc_output(cmd, BUILD_DIR "rglfw.o");

	if (!nob_cmd_run(cmd))
		return false;
	return true;
}
bool linux_build(Nob_Cmd *cmd)
{
	if (!nob_file_exists(BUILD_DIR "rglfw.o")) {
		if (!linux_build_glfw(cmd))
			return false;
	}
	nob_cc(cmd);
	nob_cc_flags(cmd);

	linux_glfw_flags(cmd);
	// nob_cmd_append(cmd, "-fsanitize=address");
	// nob_cmd_append(cmd, "-fsanitize=undefined");
	append_source_files(cmd);
	nob_cc_inputs(cmd, BUILD_DIR "rglfw.o");
	nob_cc_output(cmd, BIN_DIR "vonu");
	if (!nob_cmd_run(cmd))
		return false;
	return true;
}
bool linux_mingw_build(Nob_Cmd *cmd)
{
	if (!nob_file_exists(BUILD_DIR "rglfwmingw.o")) {
		if (!linux_mingw_build_glfw(cmd))
			return false;
	}

	nob_cmd_append(cmd, "x86_64-w64-mingw32-gcc");
	// nob_cc(cmd);
	nob_cc_flags(cmd);

	linux_mingw_glfw_flags(cmd);
	nob_cmd_append(cmd, "-static");
	append_source_files(cmd);
	nob_cc_inputs(cmd, BUILD_DIR "rglfwmingw.o");
	nob_cmd_append(cmd, "-lgdi32");
	nob_cmd_append(cmd, "-luser32");
	nob_cmd_append(cmd, "-lshell32");
	nob_cc_output(cmd, BIN_DIR "vonu.exe");
	if (!nob_cmd_run(cmd))
		return false;
	return true;
}
int main(int argc, char *argv[])
{
	NOB_GO_REBUILD_URSELF(argc, argv);

	if (!nob_mkdir_if_not_exists(BUILD_DIR))
		exit(1);
	if (!nob_mkdir_if_not_exists(BIN_DIR))
		exit(1);
	Nob_Cmd cmd = { 0 };

	if (!linux_build(&cmd))
		exit(1);
	if (argc == 2) {
		if (!linux_mingw_build(&cmd))
			exit(1);
	}
}
