#define NOB_IMPLEMENTATION
#define NOB_NO_ECHO
#define NOB_EXPERIMENTAL_DELETE_OLD
#include "nob.h"
#include <glob.h>

#define SRC_FOLDER "src"
#define BUILD_FOLDER "build"
#define OBJ_FOLDER BUILD_FOLDER "/obj"
#if defined(_WIN32)
#define CONSOLE_OUTPUT BUILD_FOLDER "/flint.exe"
#else
#define CONSOLE_OUTPUT BUILD_FOLDER "/flint"
#endif


#if defined(_WIN32)
     #define CC_FLAGS "-std=gnu17", "-DCOBJMACROS", "-I", "include", "-MMD", "-MP","-O3","-Wall","-Wextra","-Wno-unused-parameter","-Wno-sign-compare","-Wno-discarded-qualifiers","-Wno-stringop-truncation","-Wno-sign-compare","-Wno-unused-parameter","-Wno-misleading-indentation"
     #define LINK_FLAGS "-lcomctl32", "-lgdiplus", "-lgdi32", "-lshlwapi", "-lshell32", "-lwinmm", "-lole32", "-luuid", "-lmsimg32", "-ldwmapi", "-lpsapi", "-lcrypt32", "-lcomdlg32", "-ladvapi32", "-ldxva2", "-luser32", "-lgdi32"
#else
     #define CC_FLAGS "-I", "include","-MMD", "-MP","-O3","-Wall","-Wextra","-Wno-unused-parameter","-Wno-sign-compare","-Wno-discarded-qualifiers","-Wno-stringop-truncation","-Wno-sign-compare","-Wno-unused-parameter","-Wno-misleading-indentation"
     #define LINK_FLAGS "-lm", "-ldl", "-Wl,--export-dynamic"

     #define PKG_CFLAGS_PKGS_STR "x11 xft fontconfig freetype2 glib-2.0 gio-unix-2.0 libpng xext xrandr xrender xtst xi wayland-client"
     #define PKG_LIBS_PKGS_STR   "x11 xft fontconfig libpng xext xrandr xrender xtst xi gio-2.0 gobject-2.0 glib-2.0"
#endif

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
#if defined(_WIN32)
            if (nob_sv_ends_with_cstr(path, "_x11.c")) return true;
            if (nob_sv_ends_with_cstr(path, "wm.c")) return true;
#else
            if (nob_sv_ends_with_cstr(path, "_win32.c")) return true;
#endif
            if (!getenv("WAYLAND_DISPLAY"))  if (nob_sv_ends_with_cstr(path, "_wayland.c")) return true;
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

static const char* feature_so_path(const char* src) {
    const char* base = nob_path_name(src);
    size_t len = strlen(base);
    char* no_ext = malloc(len+1);
    strcpy(no_ext, base);
    char* dot = strrchr(no_ext, '.');
    if (dot) *dot = '\0';
#if defined(_WIN32)
    const char* ext = ".dll";
#elif defined(__APPLE__)
    const char* ext = ".dylib";
#else
    const char* ext = ".so";
#endif
    return nob_temp_sprintf("build/features/%s%s", no_ext, ext);
}

static const char* feature_dep_path(const char* src) {
    const char* base = nob_path_name(src);
    size_t len = strlen(base);
    char* no_ext = malloc(len+1);
    strcpy(no_ext, base);
    char* dot = strrchr(no_ext, '.');
    if (dot) *dot = '\0';
    return nob_temp_sprintf("build/features/%s.d", no_ext);
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

#if !defined(_WIN32)
static Nob_File_Paths dep_cflags = {0};
static Nob_File_Paths dep_ldflags = {0};

static void append_ws_split(Nob_File_Paths* da, const char* s) {
    if (!s) return;
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s", s);
    char* p = buf;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;
        char* start = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
        if (*p) *p++ = '\0';
        nob_da_append(da, nob_temp_strdup(start));
    }
}

static char* run_capture(const char* cmdline) {
    FILE* f = popen(cmdline, "r");
    if (!f) return NULL;
    static char out[8192];
    size_t n = fread(out, 1, sizeof(out) - 1, f);
    out[n] = '\0';
    pclose(f);
    return out;
}

// On NixOS the .pc files of profile packages are not in pkg-config's
// compiled-in search path and PKG_CONFIG_PATH is normally unset outside a
// nix-shell. Some packages (xorgproto) ship .pc under share/pkgconfig while
// others use lib/pkgconfig, and the profile aggregation only merges the
// latter. So scan the store for whichever dirs actually provide the modules we
// need and hand those to pkg-config. On non-Nix systems this finds nothing and
// leaves the normal search path untouched.
static void augment_pkg_config_path(void) {
    const char* need[] = {
        "x11", "xft", "fontconfig", "freetype2", "glib-2.0", "gio-2.0",
        "gobject-2.0", "libpng", "xext", "xrandr", "xrender", "xtst", "xi",
        "wayland-client",
        // transitive requirements pulled in by the above .pc files
        "xproto", "kbproto", "xextproto", "xcb", "xau", "xdmcp", "sm", "ice",
        NULL
    };
    const char* patterns[] = {
        "/nix/store/*/lib/pkgconfig", "/nix/store/*/share/pkgconfig", NULL
    };
    char found[64][512];
    size_t found_n = 0;
    for (int p = 0; patterns[p]; ++p) {
        glob_t g;
        memset(&g, 0, sizeof(g));
        if (glob(patterns[p], 0, NULL, &g) != 0) { globfree(&g); continue; }
        for (size_t i = 0; i < g.gl_pathc; ++i) {
            for (int n = 0; need[n]; ++n) {
                char probe[1024];
                snprintf(probe, sizeof(probe), "%s/%s.pc", g.gl_pathv[i], need[n]);
                if (!nob_file_exists(probe)) continue;
                bool dup = false;
                for (size_t f = 0; f < found_n; ++f)
                    if (strcmp(found[f], g.gl_pathv[i]) == 0) { dup = true; break; }
                if (!dup && found_n < 64) {
                    snprintf(found[found_n], sizeof(found[0]), "%s", g.gl_pathv[i]);
                    found_n++;
                }
                break;
            }
        }
        globfree(&g);
    }
    if (found_n == 0) return;
    char path[8192];
    path[0] = '\0';
    for (size_t f = 0; f < found_n; ++f) {
        if (path[0]) strncat(path, ":", sizeof(path) - strlen(path) - 1);
        strncat(path, found[f], sizeof(path) - strlen(path) - 1);
    }
    const char* cur = getenv("PKG_CONFIG_PATH");
    char full[16384];
    if (cur && cur[0]) snprintf(full, sizeof(full), "%s:%s", path, cur);
    else snprintf(full, sizeof(full), "%s", path);
    setenv("PKG_CONFIG_PATH", full, 1);
    nob_log(NOB_INFO, "pkg-config search dirs: %zu", found_n);
}

static void discover_dep_flags(void) {
    char cmdline[512];
    augment_pkg_config_path();
    snprintf(cmdline, sizeof(cmdline), "pkg-config --cflags %s 2>/dev/null", PKG_CFLAGS_PKGS_STR);
    const char* cflags = run_capture(cmdline);
    if (cflags && cflags[0]) {
        append_ws_split(&dep_cflags, cflags);
        nob_log(NOB_INFO, "pkg-config cflags: %zu flag(s)", dep_cflags.count);
    } else {
        const char* fb[] = {
            "-I/usr/include/freetype2", "-I/usr/include/glib-2.0",
            "-I/usr/lib/glib-2.0/include", "-I/usr/include/gio-unix-2.0",
            "-I/usr/include/X11", "-I/usr/include/freetype2",
        };
        for (size_t i = 0; i < sizeof(fb)/sizeof(fb[0]); i++) nob_da_append(&dep_cflags, nob_temp_strdup(fb[i]));
        nob_log(NOB_INFO, "pkg-config unavailable, using default include paths");
    }
    snprintf(cmdline, sizeof(cmdline), "pkg-config --libs %s 2>/dev/null", PKG_LIBS_PKGS_STR);
    const char* ldflags = run_capture(cmdline);
    if (ldflags && ldflags[0]) {
        append_ws_split(&dep_ldflags, ldflags);
        for (size_t i = 0; i < dep_ldflags.count; ++i) {
            const char* it = dep_ldflags.items[i];
            if (strncmp(it, "-L", 2) == 0 && it[2]) {
                char rpath[512];
                snprintf(rpath, sizeof(rpath), "-Wl,-rpath,%s", it + 2);
                nob_da_append(&dep_ldflags, nob_temp_strdup(rpath));
            }
        }
        nob_log(NOB_INFO, "pkg-config libs: %zu flag(s)", dep_ldflags.count);
    } else {
        const char* fb[] = {
            "-lX11", "-lXft", "-lfontconfig", "-lXrender", "-lXrandr",
            "-lXext", "-lXtst", "-lpng", "-lgio-2.0", "-lglib-2.0",
            "-lgobject-2.0",
        };
        for (size_t i = 0; i < sizeof(fb)/sizeof(fb[0]); i++)         nob_da_append(&dep_ldflags, nob_temp_strdup(fb[i]));
        nob_log(NOB_INFO, "pkg-config unavailable, using default link libs");
    }
}

static void append_dep_cflags(Nob_Cmd* cmd) {
    for (size_t i = 0; i < dep_cflags.count; ++i) nob_cmd_append(cmd, dep_cflags.items[i]);
}

static void append_dep_ldflags(Nob_Cmd* cmd) {
    for (size_t i = 0; i < dep_ldflags.count; ++i) nob_cmd_append(cmd, dep_ldflags.items[i]);
}
#else
static void append_dep_cflags(Nob_Cmd* cmd) { (void)cmd; }
static void append_dep_ldflags(Nob_Cmd* cmd) { (void)cmd; }
#endif


static void write_json_string(FILE* f, const char* s) {
    fputc('"', f);
    for (const char* p = s; *p; ++p) {
        if (*p == '"') {
            fputc('\\', f);
            fputc('"', f);
        } else if (*p == '\\') {
#if defined(_WIN32)
            fputc('/', f);
#else
            fputc('\\', f);
            fputc('\\', f);
#endif
        } else {
            fputc(*p, f);
        }
    }
    fputc('"', f);
}

static bool write_compile_commands(void) {
    const char* cwd = nob_get_current_dir_temp();
    if (!cwd) return false;

    FILE* f = fopen("compile_commands.json", "w");
    if (!f) {
        nob_log(NOB_ERROR, "Failed to write compile_commands.json");
        return false;
    }

    fprintf(f, "[\n");
    for (size_t i = 0; i < sources.count; ++i) {
        if (i > 0) fprintf(f, ",\n");

        Nob_Cmd cmd = {0};
        nob_cc(&cmd);
        nob_cc_flags(&cmd);
        nob_cmd_append(&cmd, CC_FLAGS, "-c", sources.items[i]);
        append_dep_cflags(&cmd);

        fprintf(f, "  {\n");
        fprintf(f, "    \"directory\": ");
        write_json_string(f, cwd);
        fprintf(f, ",\n");
        fprintf(f, "    \"file\": ");
        write_json_string(f, sources.items[i]);
        fprintf(f, ",\n");
        fprintf(f, "    \"arguments\": [\n");
        for (size_t j = 0; j < cmd.count; ++j) {
            fprintf(f, "      ");
            write_json_string(f, cmd.items[j]);
            fprintf(f, "%s\n", j + 1 < cmd.count ? "," : "");
        }
        fprintf(f, "    ]\n");
        fprintf(f, "  }");
    }
    fprintf(f, "\n]\n");
    fclose(f);

    nob_log(NOB_INFO, "Wrote compile_commands.json (%zu files)", sources.count);
    return true;
}

static bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static bool stale_with_dep(const char* src, const char* obj, const char* dep_file_path) {
    if (!nob_file_exists(obj)) return true;
    if (nob_needs_rebuild1(obj, src) != 0) return true;

    Nob_String_Builder dep = {0};
    if (!nob_read_entire_file(dep_file_path, &dep)) return true;

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
        if (nob_needs_rebuild1(obj, dep_file) != 0) { nob_log(NOB_INFO, "stale %s due to %s", obj, dep_file); return true; }
    }
    return false;
}

static bool stale(const char* src, const char* obj) {
    return stale_with_dep(src, obj, dep_path(src));
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
    size_t _nprocs = (size_t)nob_nprocs();
    if (_nprocs == 0) _nprocs = 6;
    size_t max_procs = _nprocs + 1;
    nob_log(NOB_INFO, "Compiling %zu files with %zu parallel jobs (nprocs=%zu)", to_build.count, max_procs, _nprocs);

    for (size_t i = 0; i < to_build.count; ++i) {
        Nob_Cmd cmd = {0};
        nob_cc(&cmd);
        nob_cc_flags(&cmd);
        nob_cmd_append(&cmd, CC_FLAGS, "-c", to_build.items[i], "-o", obj_path(to_build.items[i]));
        append_dep_cflags(&cmd);
        if (!nob_cmd_run(&cmd, .async = &procs, .max_procs = max_procs,
                         .stderr_path = err_paths.items[i])) {
            
            
            
            ok = false;
        }
    }

    if (!nob_procs_wait_and_reset(&procs)) ok = false;
    return print_failures() && ok;
}





static bool instance_running(void) {
    
    const char* marker = "/tmp/flint_nob_check";
    Nob_Cmd chk = {0};
    nob_cmd_append(&chk, "sh", "-c",
        "if systemctl --user is-active --quiet flint.service 2>/dev/null "
        "|| pgrep -f 'build/[f]lint' >/dev/null 2>&1; then echo running; fi; "
        "exit 0");
    if (!nob_cmd_run(&chk, .stdout_path = marker)) return false;
    Nob_String_Builder sb = {0};
    bool running = false;
    if (nob_read_entire_file(marker, &sb)) {
        running = sb.count > 0;
    }
    nob_sb_free(sb);
    remove(marker);
    return running;
}

static void stop_running_instance(void) {
    if (!instance_running()) return;

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "sh", "-c",
        "systemctl --user stop flint.service 2>/dev/null; "
        "pkill -f 'build/[f]lint' 2>/dev/null; "
        "exit 0");
    if (nob_cmd_run(&cmd)) {
        nob_log(NOB_INFO, "Stopped previously running flint instance");
    }
}

static bool run_tests(void) {
    if (!nob_mkdir_if_not_exists("build/tests")) return false;
    if (test_sources.count == 0) {
        nob_log(NOB_INFO, "No tests found in tests/");
        return true;
    }
    const char* support_obj = NULL;
    if (nob_file_exists("tests/support.c")) {
        support_obj = "build/tests/support.o";
        Nob_Cmd scmd = {0};
        nob_cc(&scmd);
        nob_cmd_append(&scmd, "-I", "include", "-I", "tests", "-O2", "-c", "tests/support.c", "-o", support_obj);
        nob_log(NOB_INFO, "Building test support %s", support_obj);
        if (!nob_cmd_run(&scmd)) {
            nob_log(NOB_ERROR, "Failed to build test support");
            return false;
        }
    }
    size_t failed = 0;
    size_t suite_count = test_sources.count;
    char (*suite_names)[128] = calloc(suite_count ? suite_count : 1, sizeof(char[128]));
    bool* suite_ok = calloc(suite_count ? suite_count : 1, sizeof(bool));
    Nob_Log_Level prev_level = nob_minimal_log_level;
    nob_minimal_log_level = NOB_ERROR;
    for (size_t t = 0; t < test_sources.count; ++t) {
        const char* src = test_sources.items[t];
        const char* base = nob_path_name(src);
        char name[128];
        snprintf(name, sizeof(name), "%s", base);
        char* dot = strrchr(name, '.');
        if (dot) *dot = '\0';
        const char* out = nob_temp_sprintf("build/tests/%s", name);
#if defined(_WIN32)
        if (!nob_sv_ends_with_cstr(nob_sv_from_cstr(out), ".exe")) out = nob_temp_sprintf("%s.exe", out);
#endif
        Nob_Cmd cmd = {0};
        nob_cc(&cmd);
        nob_cmd_append(&cmd, "-I", "include", "-I", "tests", "-O2", "-Wall", src);
        append_dep_cflags(&cmd);
        if (support_obj) nob_cmd_append(&cmd, support_obj);
        for (size_t i = 0; i < sources.count; ++i) {
            if (strcmp(sources.items[i], "src/main.c") == 0) continue;
            if (strcmp(sources.items[i], "src\\main.c") == 0) continue;
            nob_cmd_append(&cmd, obj_path(sources.items[i]));
        }
        append_dep_ldflags(&cmd);
        nob_cmd_append(&cmd, LINK_FLAGS, "-o", out);
        nob_log(NOB_INFO, "Building test %s", out);
        snprintf(suite_names[t], sizeof(suite_names[t]), "%s", name);
        if (!nob_cmd_run(&cmd)) {
            nob_log(NOB_ERROR, "Failed to build test %s", src);
            snprintf(suite_names[t], sizeof(suite_names[t]), "%s (BUILD FAILED)", name);
            suite_ok[t] = false;
            failed++;
            continue;
        }
        Nob_Cmd run = {0};
        nob_cmd_append(&run, out);
        nob_log(NOB_INFO, "Running test %s", out);
        suite_ok[t] = nob_cmd_run(&run);
        if (!suite_ok[t]) failed++;
    }
    nob_minimal_log_level = prev_level;

    nob_log(NOB_INFO, "");
    nob_log(NOB_INFO, "==================== TEST SUMMARY ====================");
    for (size_t t = 0; t < suite_count; t++) {
        nob_log(suite_ok[t] ? NOB_INFO : NOB_ERROR, "  %-4s %s",
                suite_ok[t] ? "PASS" : "FAIL", suite_names[t]);
    }
    nob_log(NOB_INFO, "=======================================================");
    if (failed > 0) {
        nob_log(NOB_ERROR, "%zu of %zu test suite(s) failed", failed, suite_count);
        free(suite_names);
        free(suite_ok);
        return false;
    }
    nob_log(NOB_INFO, "All %zu test suite(s) passed", suite_count);
    free(suite_names);
    free(suite_ok);
    return true;
}

int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF_PLUS(argc, argv, "nob.h");

    int do_run = 0;
    int do_actual = 0;
    int do_test = 0;
    for (int i=1;i<argc;i++){
        if (strcmp(argv[i], "-run")==0) do_run=1;
        if (strcmp(argv[i], "-actual")==0) do_actual=1;
        if (strcmp(argv[i], "-test")==0) do_test=1;
    }

    if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return 1;
    if (!nob_mkdir_if_not_exists(OBJ_FOLDER)) return 1;
    if (!nob_walk_dir(SRC_FOLDER, collect_source)) {
        nob_log(NOB_ERROR, "Failed to walk " SRC_FOLDER);
        return 1;
    }
    if (!nob_walk_dir("tests", collect_source)) {
        nob_log(NOB_ERROR, "Failed to walk tests");
        return 1;
    }

#if !defined(_WIN32)
    discover_dep_flags();
#endif

    write_compile_commands();

    for (size_t i = 0; i < sources.count; ++i) {
        const char* src = sources.items[i];
        if (stale(src, obj_path(src))) {
            nob_da_append(&to_build, src);
            nob_da_append(&err_paths, strdup(err_path(src)));
        }
    }

    if (!nob_mkdir_if_not_exists("build/features")) return 1;
#if defined(_WIN32)
    if (!nob_mkdir_if_not_exists("build/debug")) return 1;
#endif

    bool any_compiled = false;
    if (!compile_all(&any_compiled)) return 1;

#if defined(_WIN32)
    const char* output = do_actual ? "build/flint.exe" : "build/debug/flint.exe";
#else
    const char* output = CONSOLE_OUTPUT;
#endif
    Nob_File_Paths all_objs = {0};
    for (size_t i = 0; i < sources.count; ++i) {
        nob_da_append(&all_objs, strdup(obj_path(sources.items[i])));
    }
    bool needs_link = any_compiled || !nob_file_exists(output) ||
        nob_needs_rebuild(output, (const char**)all_objs.items, all_objs.count) != 0;
#if defined(_WIN32)
    {
        const char* other = do_actual ? "build/debug/flint.exe" : "build/flint.exe";
        if (nob_file_exists(other) &&
            nob_needs_rebuild(other, (const char**)all_objs.items, all_objs.count) != 0) {
            needs_link = true;
        }
    }
#endif
    nob_da_free(all_objs);
    if (!needs_link) {
        nob_log(NOB_INFO, "No changes, skipping link");
        {
            Nob_Procs fprocs = {0};
            size_t _fmax = (size_t)nob_nprocs();
            if (_fmax == 0) _fmax = 6;
            _fmax += 1;
            bool fok = true;
            for (size_t i = 0; i < feature_sources.count; ++i) {
                const char* src = feature_sources.items[i];
                const char* so = feature_so_path(src);
                bool need_so = !nob_file_exists(so) || stale_with_dep(src, so, feature_dep_path(src));
                if (!need_so) continue;
                Nob_Cmd fcmd = {0};
                nob_cc(&fcmd);
                nob_cc_flags(&fcmd);
#if defined(_WIN32)
                nob_cmd_append(&fcmd, CC_FLAGS, "-shared", "-o", so, src, "build/main.lib.a", "-MF", feature_dep_path(src), LINK_FLAGS);
#else
                nob_cmd_append(&fcmd, CC_FLAGS, "-fPIC", "-shared", "-o", so, src, "-MF", feature_dep_path(src));
                append_dep_cflags(&fcmd);
#endif
                if (!nob_cmd_run(&fcmd, .async = &fprocs, .max_procs = _fmax)) {
                    nob_log(NOB_ERROR, "Failed to start feature %s", so);
                    fok = false;
                }
            }
            if (!nob_procs_wait_and_reset(&fprocs)) fok = false;
            if (!fok) return 1;
            for (size_t i = 0; i < feature_sources.count; ++i) {
                const char* so = feature_so_path(feature_sources.items[i]);
            }
        }
        if (do_run) {
            stop_running_instance();
            Nob_Cmd cmd = {0};
            nob_cmd_append(&cmd, output);
            nob_log(NOB_INFO, "Started %s", output);
            if (!nob_cmd_run_async_and_reset(&cmd)) return 1;
        }
        if (do_test && !run_tests()) return 1;
        return 0;
    }

    Nob_File_Paths objs = {0};
    for (size_t i = 0; i < sources.count; ++i) {
        nob_da_append(&objs, strdup(obj_path(sources.items[i])));
    }

    Nob_Cmd cmd = {0};
    nob_cc(&cmd);
    nob_cmd_append(&cmd, "-o", output);
    for (size_t i = 0; i < objs.count; ++i) {
        nob_cmd_append(&cmd, objs.items[i]);
    }
#if defined(_WIN32)
    if (do_actual) nob_cmd_append(&cmd, "-mwindows");
    nob_cmd_append(&cmd, "-Wl,--export-all-symbols", "-Wl,--out-implib,build/main.lib.a");
#endif
    append_dep_ldflags(&cmd);
    nob_cmd_append(&cmd, LINK_FLAGS);
    if (!nob_cmd_run(&cmd)) {
        nob_log(NOB_ERROR, "Failed to link %s", output);
        return 1;
    }
    {
        Nob_Procs fprocs = {0};
        size_t _fmax2 = (size_t)nob_nprocs();
        if (_fmax2 == 0) _fmax2 = 6;
        _fmax2 += 1;
        bool fok = true;
        for (size_t i = 0; i < feature_sources.count; ++i) {
            const char* src = feature_sources.items[i];
            const char* so = feature_so_path(src);
            bool need_so = !nob_file_exists(so) || stale_with_dep(src, so, feature_dep_path(src));
            if (!need_so) continue;
            Nob_Cmd fcmd = {0};
            nob_cc(&fcmd);
            nob_cc_flags(&fcmd);
#if defined(_WIN32)
            nob_cmd_append(&fcmd, CC_FLAGS, "-shared", "-o", so, src, "build/main.lib.a", "-MF", feature_dep_path(src), LINK_FLAGS);
#else
            nob_cmd_append(&fcmd, CC_FLAGS, "-fPIC", "-shared", "-o", so, src, "-MF", feature_dep_path(src));
            append_dep_cflags(&fcmd);
#endif
            if (!nob_cmd_run(&fcmd, .async = &fprocs, .max_procs = _fmax2)) {
                nob_log(NOB_ERROR, "Failed to start feature %s", so);
                fok = false;
            }
        }
        if (!nob_procs_wait_and_reset(&fprocs)) fok = false;
        if (!fok) return 1;
        for (size_t i = 0; i < feature_sources.count; ++i) {
            const char* so = feature_so_path(feature_sources.items[i]);
            if (nob_file_exists(so)) nob_log(NOB_INFO, "Built feature %s", so);
        }
    }

#if !defined(_WIN32)
    {
        Nob_Cmd chmod = {0};
        nob_cmd_append(&chmod, "chmod", "+x", output);
        nob_cmd_run(&chmod);
    }
#endif
    stop_running_instance();
#if !defined(_WIN32)
    {
        nob_log(NOB_INFO, "Bundling runtime deps (ship build/flint + build/bundle/)");
        Nob_Cmd bundle = {0};
        nob_cmd_append(&bundle, "sh", "-c", "mkdir -p build/bundle && ldd build/flint | grep -o '/[^ ]*\\.so[^ ]*' | xargs -I{} cp -v {} build/bundle/ 2>/dev/null; echo \"Bundled $(ls build/bundle/*.so* 2>/dev/null | wc -l) libs to build/bundle/ (ship with LD_LIBRARY_PATH=./build/bundle)\"");
        nob_cmd_run(&bundle);
        Nob_Cmd rpath = {0};
        nob_cmd_append(&rpath, "sh", "-c", "which patchelf >/dev/null 2>&1 && patchelf --set-rpath '$ORIGIN/bundle' build/flint 2>/dev/null && echo \"Patched rpath to \\$ORIGIN/bundle\" || true");
        nob_cmd_run(&rpath);
    }
#endif
    if (do_run) {
        Nob_Cmd run = {0};
        nob_cmd_append(&run, output);
        if (nob_cmd_run_async_and_reset(&run) == NOB_INVALID_PROC) return 1;
        nob_log(NOB_INFO, "Started %s", output);
    }
    nob_log(NOB_INFO,"Built Successfully");
    if (do_test && !run_tests()) return 1;

    return 0;
}
