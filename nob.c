#define NOB_IMPLEMENTATION
#include "nob.h"

#define SRC_FOLDER "src"
#define BUILD_FOLDER "build"
#define OBJ_FOLDER BUILD_FOLDER "/obj"
#define CONSOLE_OUTPUT BUILD_FOLDER "/main.exe"
#define ACTUAL_OUTPUT BUILD_FOLDER "/myprogram.exe"

#define CC_FLAGS "-std=gnu17", "-DCOBJMACROS", "-I", "include", "-MMD", "-MP", "-Wno-multichar", "-Wunused-parameter","-Wunused-variable", "-fdiagnostics-color=always"
#define LINK_FLAGS "-lcomctl32", "-lgdiplus", "-lgdi32", "-lshlwapi", "-lshell32", "-lwinmm", "-lole32", "-luuid", "-lmsimg32", "-ldbghelp", "-ldwmapi", "-lpsapi", "-lcrypt32", "-lcomdlg32", "-ladvapi32"

Nob_File_Paths sources = {0};

static bool ends_with(const char* str, const char* suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return false;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

static bool collect(Nob_Walk_Entry entry) {
    if (entry.type == NOB_FILE_REGULAR && ends_with(entry.path, ".c")) {
        nob_da_append(&sources, strdup(entry.path));
    }
    return true;
}

static const char* obj_path(const char* src) {
    return nob_temp_sprintf(OBJ_FOLDER "/%s.o", nob_path_name(src));
}

static bool compile_all(bool* any_compiled) {
    if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return false;
    if (!nob_mkdir_if_not_exists(OBJ_FOLDER)) return false;

    Nob_File_Paths to_build = {0};
    for (size_t i = 0; i < sources.count; ++i) {
        const char* src = sources.items[i];
        const char* obj = obj_path(src);
        if (!nob_file_exists(obj) || nob_needs_rebuild1(obj, src)) {
            nob_da_append(&to_build, src);
        }
    }
    *any_compiled = to_build.count > 0;
    if (to_build.count == 0) return true;

    size_t max_procs = (size_t)nob_nprocs() + 1;
    Nob_Procs procs = {0};
    bool failed = false;

    for (size_t i = 0; i < to_build.count; ++i) {
        if (failed) break;
        const char* src = to_build.items[i];
        Nob_Cmd cmd = {0};
        nob_cc(&cmd);
        nob_cc_flags(&cmd);
        nob_cmd_append(&cmd, CC_FLAGS, "-c", src, "-o", obj_path(src));
        if (!nob_cmd_run(&cmd, .async = &procs, .max_procs = max_procs)) {
            failed = true;
        }
    }

    if (failed) {
        for (size_t j = 0; j < procs.count; ++j) {
            TerminateProcess(procs.items[j], 1);
            WaitForSingleObject(procs.items[j], 1000);
            CloseHandle(procs.items[j]);
        }
        nob_log(NOB_ERROR, "Compilation failed");
        return false;
    }

    return true;
}

static bool link_all(const char* output, bool windowed, bool any_compiled) {
    Nob_File_Paths objs = {0};
    for (size_t i = 0; i < sources.count; ++i) {
        nob_da_append(&objs, obj_path(sources.items[i]));
    }

    if (!any_compiled && nob_file_exists(output) &&
        !nob_needs_rebuild(output, objs.items, objs.count)) {
        nob_log(NOB_INFO, "SKIP link %s", output);
        return true;
    }

    Nob_Cmd cmd = {0};
    nob_cc(&cmd);
    nob_cmd_append(&cmd, "-o", output);
    for (size_t i = 0; i < objs.count; ++i) {
        nob_cmd_append(&cmd, objs.items[i]);
    }
    nob_cmd_append(&cmd, LINK_FLAGS);
    if (windowed) nob_cmd_append(&cmd, "-mwindows");
    
    if (!nob_cmd_run(&cmd)) {
        nob_log(NOB_ERROR, "Failed to link %s", output);
        return false;
    }
    return true;
}

int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    bool windowed = false;
    bool do_run = false;

    if (argc > 1) {
        if (strcmp(argv[1], "-actual") == 0) {
            windowed = true;
        }
    }

    if (argc > 2 && strcmp(argv[2], "run") == 0) {
        do_run = true;
    }

    if (!nob_walk_dir(SRC_FOLDER, collect)) {
        nob_log(NOB_ERROR, "Failed to walk " SRC_FOLDER);
        return 1;
    }

    const char* output = windowed ? ACTUAL_OUTPUT : CONSOLE_OUTPUT;

    bool any_compiled = false;
    if (!compile_all(&any_compiled)) return 1;
    if (!link_all(output, windowed, any_compiled)) return 1;

    if (do_run) {
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, output);
        if (nob_cmd_run_async_and_reset(&cmd) == NOB_INVALID_PROC) return 1;
        nob_log(NOB_INFO, "Started %s", output);
    }

    return 0;
}
