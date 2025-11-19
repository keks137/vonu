#include <stdlib.h>
#define NOB_IMPLEMENTATION
#include "nob.h"

#define SRC_DIR "src/"
#define BUILD_DIR "build/"
#define BIN_DIR "bin/"

void append_linux_glfw_flags(Nob_Cmd *cmd)
{
	nob_cmd_append(cmd, "-ggdb");
	nob_cmd_append(cmd, "-DPLATFORM_DESKTOP");
	nob_cmd_append(cmd, "-D_GLFW_X11");
	nob_cmd_append(cmd, "-I", "./vendor/glfw/include/");
	nob_cmd_append(cmd, "-lm");
}

bool linux_build_glfw(Nob_Cmd *cmd)
{
	nob_cc(cmd);
	append_linux_glfw_flags(cmd);
	nob_cmd_append(cmd, "-c");
	nob_cc_inputs(cmd, "vendor/rglfw.c");
	nob_cc_output(cmd, BUILD_DIR "rglfw.o");

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

	if (!linux_build_glfw(&cmd))
		exit(1);


	nob_cc(&cmd);
	nob_cc_flags(&cmd);
	append_linux_glfw_flags(&cmd);
	nob_cc_inputs(&cmd, SRC_DIR "main.c");
	nob_cc_inputs(&cmd,BUILD_DIR"rglfw.o");
	nob_cc_output(&cmd, BIN_DIR "vonu");
	nob_cmd_run(&cmd);
}
