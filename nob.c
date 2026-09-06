#define NOB_IMPLEMENTATION
#define NOB_EXPERIMENTAL_DELETE_OLD
#include "nob.h"

#define SRC_FOLDER "src"
#define BUILD_FOLDER "build"
#define OBJ_FOLDER BUILD_FOLDER "/obj"
#if defined(_WIN32)
#define CONSOLE_OUTPUT BUILD_FOLDER "/main.exe"
#define ACTUAL_OUTPUT BUILD_FOLDER "/flint.exe"
#else
#define CONSOLE_OUTPUT BUILD_FOLDER "/flint"
#endif

#if defined(_WIN32)
     #define CC_FLAGS "-std=gnu17", "-DCOBJMACROS", "-I", "include", "-MMD", "-MP", "-w"
     #define LINK_FLAGS "-lcomctl32", "-lgdiplus", "-lgdi32", "-lshlwapi", "-lshell32", "-lwinmm", "-lole32", "-luuid", "-lmsimg32", "-ldbghelp", "-ldwmapi", "-lpsapi", "-lcrypt32", "-lcomdlg32", "-ladvapi32", "-ldxva2","-user32","-lwinhttp"
#else
     #define CC_FLAGS "-I", "include", "-I/usr/include/freetype2", "-MMD", "-MP", "-w","-g"
     #define LINK_FLAGS "-lX11", "-lXft", "-lfontconfig", "-lXrender", "-lXrandr", "-ldl"
#endif

static Nob_File_Paths sources = {0};
static Nob_File_Paths feature_sources = {0};
static bool g_want_static = false;
static bool has_musl_gcc(void){
    return nob_file_exists("/usr/bin/musl-gcc") || nob_file_exists("/bin/musl-gcc");
}

static bool collect_source(Nob_Walk_Entry entry) {
    if (entry.type == NOB_FILE_REGULAR) {
        Nob_String_View path = nob_sv_from_cstr(entry.path);
        if (nob_sv_ends_with_cstr(path, ".c")) {
#if defined(_WIN32)
            if (nob_sv_ends_with_cstr(path, "_x11.c")) return true;
#else
            if (nob_sv_ends_with_cstr(path, "_win32.c")) return true;
#endif
            if (nob_sv_starts_with(path, nob_sv_from_cstr("src/features/"))) {
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

static const char* obj_path(const char* src) {
    return nob_temp_sprintf(OBJ_FOLDER "/%s.o", nob_path_name(src));
}

static const char* dep_path(const char* src) {
    return nob_temp_sprintf(OBJ_FOLDER "/%s.d", nob_path_name(src));
}

static const char* err_path(const char* src) {
    return nob_temp_sprintf(OBJ_FOLDER "/%s.err", nob_path_name(src));
}

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
        nob_cc_flags(&cmd);
        nob_cmd_append(&cmd, CC_FLAGS, "-c", to_build.items[i], "-o", obj_path(to_build.items[i]));
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

int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF_PLUS(argc, argv, "nob.h");

    bool want_static = false;
    int do_run = 0;
    for (int i=1;i<argc;i++){
        if (strcmp(argv[i], "-run")==0) do_run=1;
    }
    g_want_static = want_static;
    if (g_want_static) nob_log(NOB_INFO, "Static build requested (%s)", has_musl_gcc() ? "musl-gcc" : "gcc -static");

    if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return 1;
    if (!nob_mkdir_if_not_exists(OBJ_FOLDER)) return 1;
    if (!nob_walk_dir(SRC_FOLDER, collect_source)) {
        nob_log(NOB_ERROR, "Failed to walk " SRC_FOLDER);
        return 1;
    }

    write_compile_commands();

    for (size_t i = 0; i < sources.count; ++i) {
        const char* src = sources.items[i];
        if (stale(src, obj_path(src))) {
            nob_da_append(&to_build, src);
            nob_da_append(&err_paths, strdup(err_path(src)));
        }
    }

    if (!nob_mkdir_if_not_exists("build/features")) return 1;
    for (size_t i = 0; i < feature_sources.count; ++i) {
        const char* src = feature_sources.items[i];
        const char* so = feature_so_path(src);
        if (!nob_file_exists(so) || nob_needs_rebuild1(so, src) != 0) {
            Nob_Cmd fcmd = {0};
            nob_cc(&fcmd);
            nob_cc_flags(&fcmd);
            nob_cmd_append(&fcmd, CC_FLAGS, "-fPIC", "-shared", "-o", so, src);
            if (!nob_cmd_run(&fcmd)) {
                nob_log(NOB_ERROR, "Failed to build feature %s", so);
                return 1;
            }
            nob_log(NOB_INFO, "Built feature %s", so);
        }
    }

    bool any_compiled = false;
    if (!compile_all(&any_compiled)) return 1;

    const char* output = CONSOLE_OUTPUT;
    if (!any_compiled && nob_file_exists(output) &&
        nob_needs_rebuild(output, (const char**)to_build.items, to_build.count) == 0) {
        nob_log(NOB_INFO, "No changes, skipping link");
        if (do_run) {
            stop_running_instance();
            Nob_Cmd cmd = {0};
            nob_cmd_append(&cmd, output);
            nob_log(NOB_INFO, "Started %s", output);
            if (!nob_cmd_run_async_and_reset(&cmd)) return 1;
        }
        return 0;
    }

    Nob_File_Paths objs = {0};
    for (size_t i = 0; i < sources.count; ++i) {
        nob_da_append(&objs, strdup(obj_path(sources.items[i])));
    }

    Nob_Cmd cmd = {0};
    if (g_want_static && has_musl_gcc()){
        nob_cmd_append(&cmd, "musl-gcc");
    } else {
        nob_cc(&cmd);
    }
    nob_cmd_append(&cmd, "-o", output);
    for (size_t i = 0; i < objs.count; ++i) {
        nob_cmd_append(&cmd, objs.items[i]);
    }
    if (g_want_static){
#if !defined(_WIN32)
        bool have_static = nob_file_exists("/usr/lib/libX11.a") || nob_file_exists("/usr/lib64/libX11.a") || nob_file_exists("/usr/lib/x86_64-linux-gnu/libX11.a") || nob_file_exists("/usr/lib/musl/lib/libX11.a");
        if (!have_static){
            nob_log(NOB_WARNING, "Static X11 libs not found (libX11.a) - true musl static needs .a");
            nob_log(NOB_INFO, "Building portable dynamic + bundling runtime deps (no UX hit, ship build/main + build/bundle/*.so)");
            nob_cmd_append(&cmd, LINK_FLAGS);
        } else {
            nob_cmd_append(&cmd, "-static");
            nob_cmd_append(&cmd, LINK_FLAGS, "-lXau", "-lXdmcp", "-lfontconfig", "-lfreetype", "-lexpat", "-lz", "-lbz2", "-lpng16", "-lbrotlidec", "-lbrotlicommon", "-lm", "-pthread");
        }
#else
        nob_cmd_append(&cmd, "-static");
        nob_cmd_append(&cmd, LINK_FLAGS);
#endif
    } else {
        nob_cmd_append(&cmd, LINK_FLAGS);
    }
    if (!nob_cmd_run(&cmd)) {
        nob_log(NOB_ERROR, "Failed to link %s", output);
        return 1;
    }
    {
        Nob_Cmd chmod = {0};
        nob_cmd_append(&chmod, "chmod", "+x", output);
        nob_cmd_run(&chmod);
    }
    stop_running_instance();
    if (g_want_static){
#if !defined(_WIN32)
        bool have_static2 = nob_file_exists("/usr/lib/libX11.a") || nob_file_exists("/usr/lib64/libX11.a") || nob_file_exists("/usr/lib/x86_64-linux-gnu/libX11.a") || nob_file_exists("/usr/lib/musl/lib/libX11.a");
        if (!have_static2){
            nob_log(NOB_INFO, "Bundling runtime deps for portable -static (ship build/main + build/bundle/)");
            Nob_Cmd bundle = {0};
            nob_cmd_append(&bundle, "sh", "-c", "mkdir -p build/bundle && ldd build/main | grep -o '/[^ ]*\\.so[^ ]*' | xargs -I{} cp -v {} build/bundle/ 2>/dev/null; echo \"Bundled $(ls build/bundle/*.so 2>/dev/null | wc -l) libs to build/bundle/ (ship with LD_LIBRARY_PATH=./build/bundle)\"");
            nob_cmd_run(&bundle);
            Nob_Cmd rpath = {0};
            nob_cmd_append(&rpath, "sh", "-c", "which patchelf >/dev/null 2>&1 && patchelf --set-rpath '$ORIGIN/bundle' build/main 2>/dev/null && echo \"Patched rpath to \\$ORIGIN/bundle\" || true");
            nob_cmd_run(&rpath);
        } else {
            Nob_Cmd check = {0};
            nob_cmd_append(&check, "sh", "-c", "ldd build/main 2>&1 | head -n 5; echo \"---\"; ls -lh build/main | awk '{print $9, $5}'");
            nob_cmd_run(&check);
        }
#endif
    }

    if (do_run) {
        Nob_Cmd run = {0};
        nob_cmd_append(&run, output);
        if (nob_cmd_run_async_and_reset(&run) == NOB_INVALID_PROC) return 1;
        nob_log(NOB_INFO, "Started %s", output);
    }
    nob_log(NOB_INFO,"Built Successfully");

    return 0;
}
