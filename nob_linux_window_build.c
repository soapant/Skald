#define NOB_IMPLEMENTATION
#define NOB_EXPERIMENTAL_DELETE_OLD
#include "nob.h"

#define SRC_FOLDER "src"
#define BUILD_FOLDER "build"
#define OBJ_FOLDER BUILD_FOLDER "/win_obj"
#define OUTPUT BUILD_FOLDER "/main_win.exe"
#define CROSS_CC "x86_64-w64-mingw32-gcc"

#define CC_FLAGS "-std=gnu17", "-DCOBJMACROS", "-I", "include", "-w"
#define LINK_FLAGS "-lcomctl32", "-lgdiplus", "-lgdi32", "-lshlwapi", "-lshell32", "-lwinmm", "-lole32", "-luuid", "-lmsimg32", "-ldbghelp", "-ldwmapi", "-lpsapi", "-lcrypt32", "-lcomdlg32", "-ladvapi32", "-ldxva2", "-luser32", "-lwinhttp"

static Nob_File_Paths sources = {0};

static bool collect_source(Nob_Walk_Entry entry) {
    if (entry.type == NOB_FILE_REGULAR) {
        Nob_String_View path = nob_sv_from_cstr(entry.path);
        if (nob_sv_ends_with_cstr(path, ".c")) {
            if (nob_sv_ends_with_cstr(path, "_x11.c")) return true;
            nob_da_append(&sources, strdup(path.data));
        }
    }
    return true;
}

static const char* obj_path(const char* src) {
    return nob_temp_sprintf(OBJ_FOLDER "/%s.o", nob_path_name(src));
}

static void unset_include_env(void) {
    unsetenv("CPATH");
    unsetenv("C_INCLUDE_PATH");
    unsetenv("CPLUS_INCLUDE_PATH");
    unsetenv("OBJC_INCLUDE_PATH");
    unsetenv("LIBRARY_PATH");
}

int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    unset_include_env();

    Nob_Cmd check = {0};
    nob_cmd_append(&check, CROSS_CC, "--version");
    if (!nob_cmd_run(&check)) {
        nob_log(NOB_ERROR, "Cross compiler '%s' not found. Install it first:", CROSS_CC);
        nob_log(NOB_INFO, "Arch:      pacman -S mingw-w64-gcc");
        nob_log(NOB_INFO, "Debian/Ubuntu: sudo apt install gcc-mingw-w64-x86-64");
        return 1;
    }

    if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return 1;
    if (!nob_mkdir_if_not_exists(OBJ_FOLDER)) return 1;
    if (!nob_walk_dir(SRC_FOLDER, collect_source)) {
        nob_log(NOB_ERROR, "Failed to walk " SRC_FOLDER);
        return 1;
    }

    for (size_t i = 0; i < sources.count; ++i) {
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, CROSS_CC, CC_FLAGS, "-c", sources.items[i], "-o", obj_path(sources.items[i]));
        nob_log(NOB_INFO, "CC %s", sources.items[i]);
        if (!nob_cmd_run(&cmd)) return 1;
    }

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, CROSS_CC, "-o", OUTPUT);
    for (size_t i = 0; i < sources.count; ++i) {
        nob_cmd_append(&cmd, obj_path(sources.items[i]));
    }
    nob_cmd_append(&cmd, LINK_FLAGS);

    nob_log(NOB_INFO, "Linking %s", OUTPUT);
    if (!nob_cmd_run(&cmd)) {
        nob_log(NOB_ERROR, "Failed to link %s", OUTPUT);
        return 1;
    }

    nob_log(NOB_INFO, "Done: %s (copy to a Windows machine and run)", OUTPUT);
    return 0;
}