#define NOB_IMPLEMENTATION
#define NOB_EXPERIMENTAL_DELETE_OLD
#include "nob.h"

#define SRC_FOLDER "src"
#define BUILD_FOLDER "build"
#define OBJ_FOLDER BUILD_FOLDER "/win_obj"
#define OUTPUT BUILD_FOLDER "/flint_win.exe"
#define CROSS_CC "x86_64-w64-mingw32-gcc"

#define CC_FLAGS "-std=gnu17", "-DCOBJMACROS", "-DWINVER=0x0601", "-D_WIN32_WINNT=0x0601", "-I/usr/x86_64-w64-mingw32/include", "-I", "include", "-MMD", "-MP","-O3","-Wall","-Wextra","-Wno-sign-compare","-Wno-unused-parameter"
#define LINK_FLAGS "-lcomctl32", "-lgdiplus", "-lgdi32", "-lshlwapi", "-lshell32", "-lwinmm", "-lole32", "-luuid", "-lmsimg32", "-ldwmapi", "-lpsapi", "-lcrypt32", "-lcomdlg32", "-ladvapi32", "-ldxva2", "-luser32", "-lwinhttp", "-Lbuild/winlibs","-liphlpapi"

static Nob_File_Paths sources = {0};
static Nob_File_Paths feature_sources = {0};
static Nob_File_Paths test_sources = {0};

static bool collect_source(Nob_Walk_Entry entry) {
    if (entry.type == NOB_FILE_REGULAR) {
        Nob_String_View path = nob_sv_from_cstr(entry.path);
        if (nob_sv_ends_with_cstr(path, ".c")) {
            if (nob_sv_starts_with(path, nob_sv_from_cstr("tests/")) ||
                nob_sv_starts_with(path, nob_sv_from_cstr("tests\\"))) {
                const char* base = nob_path_name(path.data);
                if (strncmp(base, "test_", 5) == 0) nob_da_append(&test_sources, strdup(path.data));
                return true;
            }
            if (nob_sv_ends_with_cstr(path, "_x11.c")) return true;
            if (nob_sv_ends_with_cstr(path, "_wayland.c")) return true;
            if (nob_sv_ends_with_cstr(path, "wm.c")) return true;
            bool is_feature = false;
            if (nob_sv_starts_with(path, nob_sv_from_cstr("src/features/")) ||
                nob_sv_starts_with(path, nob_sv_from_cstr("src\\features\\"))) {
                    is_feature = true;
            }
            if (is_feature) {
                nob_da_append(&feature_sources, strdup(path.data));
            } else {
                nob_da_append(&sources, strdup(path.data));
            }
        }
    }
    return true;
}

static const char* obj_path(const char* src) {
    return nob_temp_sprintf(OBJ_FOLDER "/%s.o", nob_path_name(src));
}

static const char* dep_path(const char* src) {
    return nob_temp_sprintf(OBJ_FOLDER "/%s.d", nob_path_name(src));
}

static const char* err_path(const char* src) {
    return nob_temp_sprintf(OBJ_FOLDER "/%s.err", nob_path_name(src));
}

static void unset_include_env(void) {
    unsetenv("CPATH");
    unsetenv("C_INCLUDE_PATH");
    unsetenv("CPLUS_INCLUDE_PATH");
    unsetenv("OBJC_INCLUDE_PATH");
    unsetenv("LIBRARY_PATH");
}

static bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static bool stale(const char* src, const char* obj) {
    if (!nob_file_exists(obj)) return true;
    if (nob_needs_rebuild1(obj, src) != 0) return true;

    Nob_String_Builder dep = {0};
    if (!nob_read_entire_file(dep_path(src), &dep)) return true;

    Nob_String_View sv = nob_sv_from_parts(dep.items, dep.count);
    size_t i = 0;
    while (i < sv.count && sv.data[i] != ':') i++;
    if (i >= sv.count) return true;
    sv = nob_sv_from_parts(sv.data + i + 1, sv.count - i - 1);

    while (sv.count > 0) {
        size_t k = 0;
        while (k < sv.count && is_space(sv.data[k])) k++;
        sv = nob_sv_from_parts(sv.data + k, sv.count - k);
        if (sv.count == 0) break;

        size_t j = 0;
        while (j < sv.count && !is_space(sv.data[j])) j++;
        Nob_String_View tok = nob_sv_from_parts(sv.data, j);
        sv = nob_sv_from_parts(sv.data + j, sv.count - j);

        if (tok.count == 0) continue;
        if (tok.data[tok.count - 1] == '\\') continue;
        if (tok.data[tok.count - 1] == ':') continue;

        char* dep_file = nob_temp_sprintf("%.*s", (int)tok.count, tok.data);
        if (nob_needs_rebuild1(obj, dep_file) != 0) return true;
    }
    return false;
}

static Nob_File_Paths to_build = {0};
static Nob_File_Paths err_paths = {0};
static Nob_File_Paths printed_sigs = {0};

static char* err_signature(const char* err) {
    Nob_String_Builder sb = {0};
    if (!nob_read_entire_file(err, &sb) || sb.count == 0) return NULL;
    nob_sb_append_null(&sb);

    Nob_String_Builder sig = {0};
    char* save = NULL;
    char* line = strtok_r(sb.items, "\n", &save);
    while (line) {
        if (strstr(line, "error:")) {
            nob_sb_append_buf(&sig, line, strlen(line));
            nob_sb_append(&sig, '\n');
        }
        line = strtok_r(NULL, "\n", &save);
    }
    if (sig.count == 0) return NULL;
    nob_sb_append_null(&sig);
    return strdup(sig.items);
}

static bool print_failures(void) {
    Nob_String_Builder failed_names = {0};
    size_t failed_count = 0;

    for (size_t i = 0; i < to_build.count; ++i) {
        char* sig = err_signature(err_paths.items[i]);
        if (!sig) continue;

        failed_count++;
        if (failed_names.count > 0) nob_sb_append_buf(&failed_names, ", ", 2);
        nob_sb_append_buf(&failed_names, to_build.items[i], strlen(to_build.items[i]));

        bool already_printed = false;
        for (size_t k = 0; k < printed_sigs.count; ++k) {
            if (strcmp(printed_sigs.items[k], sig) == 0) {
                already_printed = true;
                break;
            }
        }
        if (!already_printed) {
            nob_da_append(&printed_sigs, sig);
            Nob_String_Builder block = {0};
            if (nob_read_entire_file(err_paths.items[i], &block)) {
                nob_log(NOB_ERROR, "%s:", to_build.items[i]);
                fwrite(block.items, 1, block.count, stdout);
                printf("\n");
            }
        } else {
            free(sig);
        }
    }

    if (failed_count > 0) {
        nob_sb_append_null(&failed_names);
        nob_log(NOB_ERROR, "Compilation failed (%zu file%s): %s",
                failed_count, failed_count == 1 ? "" : "s", failed_names.items);
        return false;
    }
    return true;
}

static bool compile_all(bool* any_compiled) {
    *any_compiled = to_build.count > 0;
    if (to_build.count == 0) return true;

    bool ok = true;
    Nob_Procs procs = {0};
    size_t max_procs = (size_t)nob_nprocs() + 1;

    for (size_t i = 0; i < to_build.count; ++i) {
        Nob_Cmd cmd = {0};
        nob_cc(&cmd);

        cmd.items[0] = (char*)CROSS_CC;
        nob_cmd_append(&cmd, CC_FLAGS, "-c", to_build.items[i], "-o", obj_path(to_build.items[i]));
        if (!nob_cmd_run(&cmd, .async = &procs, .max_procs = max_procs,
                         .stderr_path = err_paths.items[i])) {
            ok = false;
        }
    }

    if (!nob_procs_wait_and_reset(&procs)) ok = false;
    return print_failures() && ok;
}

static bool run_tests_cross(void) {
    if (!nob_mkdir_if_not_exists("build/win_tests")) return false;
    if (test_sources.count == 0) {
        nob_log(NOB_INFO, "No tests found in tests/");
        return true;
    }
    const char* support_obj = NULL;
    if (nob_file_exists("tests/support.c")) {
        support_obj = "build/win_tests/support.o";
        Nob_Cmd scmd = {0};
        nob_cmd_append(&scmd, CROSS_CC);
        nob_cmd_append(&scmd, "-I", "include", "-I", "tests", "-O2", "-c", "tests/support.c", "-o", support_obj);
        nob_log(NOB_INFO, "Building test support %s", support_obj);
        if (!nob_cmd_run(&scmd)) {
            nob_log(NOB_ERROR, "Failed to build test support");
            return false;
        }
    }
    size_t failed = 0;
    size_t built = 0;
    for (size_t t = 0; t < test_sources.count; ++t) {
        const char* src = test_sources.items[t];
        const char* base = nob_path_name(src);
        char name[128];
        snprintf(name, sizeof(name), "%s", base);
        char* dot = strrchr(name, '.');
        if (dot) *dot = '\0';
        const char* out = nob_temp_sprintf("build/win_tests/%s.exe", name);
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, CROSS_CC);
        nob_cmd_append(&cmd, "-I", "include", "-I", "tests", "-O2", "-Wall", src);
        if (support_obj) nob_cmd_append(&cmd, support_obj);
        for (size_t i = 0; i < sources.count; ++i) {
            if (strcmp(sources.items[i], "src/main.c") == 0) continue;
            if (strcmp(sources.items[i], "src\\main.c") == 0) continue;
            nob_cmd_append(&cmd, obj_path(sources.items[i]));
        }
        nob_cmd_append(&cmd, LINK_FLAGS, "-o", out);
        nob_log(NOB_INFO, "Building test %s", out);
        if (!nob_cmd_run(&cmd)) {
            nob_log(NOB_ERROR, "Failed to build test %s", src);
            failed++;
            continue;
        }
        built++;
    }
    if (failed > 0) {
        nob_log(NOB_ERROR, "%zu test(s) failed to build", failed);
        return false;
    }
    nob_log(NOB_INFO, "Built %zu test(s), not running (cross build)", built);
    return true;
}

int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    unset_include_env();

    int do_test = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-test") == 0) do_test = 1;
    }

    if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return 1;
    if (!nob_mkdir_if_not_exists(OBJ_FOLDER)) return 1;
    if (!nob_mkdir_if_not_exists("build/winlibs")) return 1;
    {
        Nob_Cmd sh = {0};
        nob_cmd_append(&sh, "sh", "-c",
            "set -- /nix/store/*mcfgthread*mingw32*/lib/libmcfgthread.a; "
            "[ -f \"$1\" ] && ln -sf \"$1\" build/winlibs/libmcfgthread.a");
        if (!nob_cmd_run(&sh)) nob_log(NOB_ERROR, "Failed to stage libmcfgthread");
    }
    if (!nob_walk_dir(SRC_FOLDER, collect_source)) {
        nob_log(NOB_ERROR, "Failed to walk " SRC_FOLDER);
        return 1;
    }
    if (!nob_walk_dir("tests", collect_source)) {
        nob_log(NOB_ERROR, "Failed to walk tests");
        return 1;
    }

    for (size_t i = 0; i < sources.count; ++i) {
        const char* src = sources.items[i];
        if (stale(src, obj_path(src))) {
            nob_da_append(&to_build, src);
            nob_da_append(&err_paths, strdup(err_path(src)));
        }
    }

    bool any_compiled = false;
    if (!compile_all(&any_compiled)) return 1;

    if (do_test && !run_tests_cross()) return 1;
    if (do_test) return 0;

    const char* output = OUTPUT;
    if (!any_compiled && nob_file_exists(output) &&
        nob_needs_rebuild(output, (const char**)to_build.items, to_build.count) == 0) {
        nob_log(NOB_INFO, "No changes, skipping link");
        return 0;
    }

    Nob_File_Paths objs = {0};
    for (size_t i = 0; i < sources.count; ++i) {
        nob_da_append(&objs, strdup(obj_path(sources.items[i])));
    }

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, CROSS_CC, "-o", output);
    for (size_t i = 0; i < objs.count; ++i) {
        nob_cmd_append(&cmd, objs.items[i]);
    }
    nob_cmd_append(&cmd, "-Wl,--export-all-symbols", "-Wl,--out-implib,build/win_obj/main.lib.a");
    nob_cmd_append(&cmd, LINK_FLAGS);

    nob_log(NOB_INFO, "Linking %s", output);
    if (!nob_cmd_run(&cmd)) {
        nob_log(NOB_ERROR, "Failed to link %s", output);
        return 1;
    }

    {
        if (!nob_mkdir_if_not_exists("build/features")) return 1;
        for (size_t i = 0; i < feature_sources.count; ++i) {
            const char* src = feature_sources.items[i];
            const char* base = nob_path_name(src);
            size_t len = strlen(base);
            char* no_ext = malloc(len + 1);
            strcpy(no_ext, base);
            char* dot = strrchr(no_ext, '.');
            if (dot) *dot = '\0';
            const char* so = nob_temp_sprintf("build/features/%s.dll", no_ext);
            bool need_so = !nob_file_exists(so) || nob_needs_rebuild1(so, src) != 0;
            if (nob_file_exists("build/win_obj/main.lib.a") && nob_needs_rebuild1(so, "build/win_obj/main.lib.a") != 0) need_so = true;
            if (!need_so) continue;
            Nob_Cmd fcmd = {0};
            nob_cmd_append(&fcmd, CROSS_CC, CC_FLAGS, "-shared", "-o", so, src, "build/win_obj/main.lib.a", LINK_FLAGS);
            if (!nob_cmd_run(&fcmd)) {
                nob_log(NOB_ERROR, "Failed to build feature %s", so);
                free(no_ext);
                return 1;
            }
            free(no_ext);
            nob_log(NOB_INFO, "Built feature %s", so);
        }
    }

    nob_log(NOB_INFO, "Done: %s (copy to a Windows machine and run)", output);
    return 0;
}
