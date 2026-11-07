#define NOB_IMPLEMENTATION
#define NOB_EXPERIMENTAL_DELETE_OLD
#define NOB_NO_ECHO
#include "nob.h"
#if !defined(_WIN32)
#include <glob.h>
#endif

#define SRC_FOLDER "src"
#define BUILD_FOLDER "build"
#define OBJ_FOLDER BUILD_FOLDER "/obj"
#if defined(_WIN32)
#define CONSOLE_OUTPUT BUILD_FOLDER "/flint.exe"
#else
#define CONSOLE_OUTPUT BUILD_FOLDER "/flint"
#endif

#define CC_COMMON_FLAGS                                                        \
  "-std=gnu17", "-DCOBJMACROS", "-I", "include", "-MMD", "-MP", "-Wall",       \
      "-Wextra", "-Wno-unused-parameter"
#if defined(_WIN32)
#define CC_FLAGS CC_COMMON_FLAGS
#define LINK_FLAGS                                                             \
  "-lcomctl32", "-lgdiplus", "-lgdi32", "-lshlwapi", "-lshell32", "-lwinmm",   \
      "-lole32", "-luuid", "-lmsimg32", "-ldwmapi", "-lpsapi", "-lcrypt32",    \
      "-lcomdlg32", "-ladvapi32", "-ldxva2", "-luser32", "-lwinhttp",          \
      "-Lbuild/winlibs", "-liphlpapi"
#elif defined(__APPLE__)
#define CC_FLAGS CC_COMMON_FLAGS
#define LINK_FLAGS                                                             \
  "-lm", "-ldl", "-Wl,-undefined,dynamic_lookup", "-framework", "Cocoa",       \
      "-framework", "CoreGraphics"
#define PKG_CFLAGS_PKGS_STR "libcurl"
#define PKG_LIBS_PKGS_STR " libcurl"
#else
#define CC_FLAGS CC_COMMON_FLAGS, "-Wformat", "-Wno-error=format-security"
#define LINK_FLAGS "-lm", "-ldl", "-Wl,--export-dynamic"
#define PKG_CFLAGS_PKGS_STR                                                    \
  "libcurl x11 xft fontconfig freetype2 gio-unix-2.0 xext xrandr xrender "     \
  "xtst xi"
#define PKG_LIBS_PKGS_STR                                                      \
  " libcurl x11 xft fontconfig xext xrandr xrender xtst xi gio-2.0 "           \
  "gobject-2.0"
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
        const char *base = nob_path_name(path.data);
        if (strncmp(base, "test_", 5) == 0)
          nob_da_append(&test_sources, strdup(path.data));
        return true;
      }
#if defined(_WIN32)
      if (nob_sv_ends_with_cstr(path, "_x11.c"))
        return true;
      if (nob_sv_ends_with_cstr(path, "wm.c"))
        return true;
      if (nob_sv_ends_with_cstr(path, "_macos.c"))
        return true;
#elif defined(__APPLE__)
      if (nob_sv_ends_with_cstr(path, "_win32.c"))
        return true;
      if (nob_sv_ends_with_cstr(path, "_x11.c"))
        return true;
      if (nob_sv_ends_with_cstr(path, "_wayland.c"))
        return true;
      if (nob_sv_ends_with_cstr(path, "wm.c"))
        return true;
      if (nob_sv_starts_with(path, nob_sv_from_cstr("src/platform/x11/")))
        return true;
#else
      if (nob_sv_ends_with_cstr(path, "_win32.c"))
        return true;
      if (nob_sv_ends_with_cstr(path, "_macos.c"))
        return true;
#endif
      if (!getenv("WAYLAND_DISPLAY"))
        if (nob_sv_ends_with_cstr(path, "_wayland.c"))
          return true;
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

static const char *feature_ext(void) {
#if defined(_WIN32)
  return ".dll";
#elif defined(__APPLE__)
  return ".dylib";
#else
  return ".so";
#endif
}

static const char *feature_base(const char *src) {
  const char *base = nob_path_name(src);
  size_t len = strlen(base);
  char *no_ext = malloc(len + 1);
  strcpy(no_ext, base);
  char *dot = strrchr(no_ext, '.');
  if (dot)
    *dot = '\0';
  const char *result = nob_temp_sprintf("build/features/%s", no_ext);
  free(no_ext);
  return result;
}

static const char *feature_so_path(const char *src) {
  return nob_temp_sprintf("%s%s", feature_base(src), feature_ext());
}

static const char *feature_dep_path(const char *src) {
  return nob_temp_sprintf("%s.d", feature_base(src));
}

static const char *obj_file_path(const char *src, const char *ext) {
  return nob_temp_sprintf(OBJ_FOLDER "/%s%s", nob_path_name(src), ext);
}

static const char *obj_path(const char *src) {
  return obj_file_path(src, ".o");
}

static const char *dep_path(const char *src) {
  return obj_file_path(src, ".d");
}

static const char *err_path(const char *src) {
  return obj_file_path(src, ".err");
}

#if !defined(_WIN32)
static Nob_File_Paths dep_cflags = {0};
static Nob_File_Paths dep_ldflags = {0};

static void append_ws_split(Nob_File_Paths *da, const char *s) {
  if (!s)
    return;
  char *buf = strdup(s);
  if (!buf)
    return;
  char *p = buf;
  while (*p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
      p++;
    if (!*p)
      break;
    char *start = p;
    while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
      p++;
    if (*p)
      *p++ = '\0';
    nob_da_append(da, nob_temp_strdup(start));
  }
  free(buf);
}

static char *run_capture(const char *cmdline) {
  FILE *f = popen(cmdline, "r");
  if (!f)
    return NULL;
  static char *out = NULL;
  static size_t cap = 0;
  if (!out) {
    cap = 16384;
    out = malloc(cap);
    if (!out) {
      pclose(f);
      return NULL;
    }
  }
  size_t len = 0;
  for (;;) {
    if (len + 1 >= cap) {
      size_t ncap = cap * 2;
      char *nout = realloc(out, ncap);
      if (!nout) {
        pclose(f);
        return NULL;
      }
      out = nout;
      cap = ncap;
    }
    size_t n = fread(out + len, 1, cap - len - 1, f);
    len += n;
    if (n == 0)
      break;
  }
  out[len] = '\0';
  pclose(f);
  return out;
}

static void augment_pkg_config_path(void) {
  const char *need[] = {"x11",
                        "xft",
                        "fontconfig",
                        "freetype2",
                        "gio-2.0",
                        "gio-unix-2.0",
                        "gobject-2.0",
                        "xext",
                        "xrandr",
                        "xrender",
                        "xtst",
                        "xi",
                        "wayland-client",
                        "xproto",
                        "kbproto",
                        "xextproto",
                        "xcb",
                        "xau",
                        "xdmcp",
                        "sm",
                        "ice",
                        NULL};
  const char *patterns[] = {"/nix/store/*/lib/pkgconfig",
                            "/nix/store/*/share/pkgconfig", NULL};
  char **found = NULL;
  size_t found_n = 0;
  size_t found_cap = 0;
  for (int p = 0; patterns[p]; ++p) {
    glob_t g;
    memset(&g, 0, sizeof(g));
    if (glob(patterns[p], 0, NULL, &g) != 0) {
      globfree(&g);
      continue;
    }
    for (size_t i = 0; i < g.gl_pathc; ++i) {
      for (int n = 0; need[n]; ++n) {
        char probe[1024];
        snprintf(probe, sizeof(probe), "%s/%s.pc", g.gl_pathv[i], need[n]);
        if (!nob_file_exists(probe))
          continue;
        bool dup = false;
        for (size_t f = 0; f < found_n; ++f)
          if (strcmp(found[f], g.gl_pathv[i]) == 0) {
            dup = true;
            break;
          }
        if (!dup) {
          if (found_n == found_cap) {
            size_t ncap = found_cap ? found_cap * 2 : 64;
            char **nfound = realloc(found, ncap * sizeof(char *));
            if (!nfound) {
              globfree(&g);
              free(found);
              return;
            }
            found = nfound;
            found_cap = ncap;
          }
          found[found_n++] = nob_temp_strdup(g.gl_pathv[i]);
        }
        break;
      }
    }
    globfree(&g);
  }
  if (found_n == 0) {
    free(found);
    return;
  }
  size_t total = 1;
  for (size_t f = 0; f < found_n; ++f)
    total += strlen(found[f]) + 1;
  const char *cur = getenv("PKG_CONFIG_PATH");
  if (cur && cur[0])
    total += strlen(cur) + 1;
  char *full = malloc(total);
  if (!full) {
    free(found);
    return;
  }
  full[0] = '\0';
  for (size_t f = 0; f < found_n; ++f) {
    if (full[0])
      strcat(full, ":");
    strcat(full, found[f]);
  }
  if (cur && cur[0]) {
    strcat(full, ":");
    strcat(full, cur);
  }
  setenv("PKG_CONFIG_PATH", full, 1);
  free(full);
  free(found);
}

static void discover_dep_flags(void) {
#if defined(__APPLE__)
  return;
#else
  char cmdline[512];
  augment_pkg_config_path();
  snprintf(cmdline, sizeof(cmdline), "pkg-config --cflags %s 2>/dev/null",
           PKG_CFLAGS_PKGS_STR);
  const char *cflags = run_capture(cmdline);
  if (cflags && cflags[0]) {
    append_ws_split(&dep_cflags, cflags);
  } else {
    const char *fb[] = {
        "-I/usr/include/freetype2",
        "-I/usr/include/gio-unix-2.0",
        "-I/usr/include/X11",
        "-I/usr/include/freetype2",
    };
    for (size_t i = 0; i < sizeof(fb) / sizeof(fb[0]); i++)
      nob_da_append(&dep_cflags, nob_temp_strdup(fb[i]));
  }
  snprintf(cmdline, sizeof(cmdline), "pkg-config --static --libs %s 2>/dev/null",
           PKG_LIBS_PKGS_STR);
  const char *ldflags = run_capture(cmdline);
  if (ldflags && ldflags[0]) {
    append_ws_split(&dep_ldflags, ldflags);
    for (size_t i = 0; i < dep_ldflags.count; ++i) {
      const char *it = dep_ldflags.items[i];
      if (strncmp(it, "-L", 2) == 0 && it[2]) {
        char rpath[512];
        snprintf(rpath, sizeof(rpath), "-Wl,-rpath,%s", it + 2);
        nob_da_append(&dep_ldflags, nob_temp_strdup(rpath));
      }
    }
  } else {
    const char *fb[] = {
        "-lX11",  "-lXft",  "-lfontconfig", "-lXrender",     "-lXrandr",
        "-lXext", "-lXtst", "-lgio-2.0",    "-lgobject-2.0",
    };
    for (size_t i = 0; i < sizeof(fb) / sizeof(fb[0]); i++)
      nob_da_append(&dep_ldflags, nob_temp_strdup(fb[i]));
  }
#endif
}

static void append_dep_cflags(Nob_Cmd *cmd) {
  for (size_t i = 0; i < dep_cflags.count; ++i)
    nob_cmd_append(cmd, dep_cflags.items[i]);
}

static void append_dep_ldflags(Nob_Cmd *cmd) {
  nob_cmd_append(cmd, "-Wl,-Bstatic");
  for (size_t i = 0; i < dep_ldflags.count; ++i) {
    if (strcmp(dep_ldflags.items[i], "-lm") == 0 ||
        strcmp(dep_ldflags.items[i], "-ldl") == 0 ||
        strcmp(dep_ldflags.items[i], "-lpthread") == 0 ||
        strcmp(dep_ldflags.items[i], "-lrt") == 0)
      continue;
    nob_cmd_append(cmd, dep_ldflags.items[i]);
  }
  nob_cmd_append(cmd, "-Wl,-Bdynamic");
}
#else
static void append_dep_cflags(Nob_Cmd *cmd) { (void)cmd; }
static void append_dep_ldflags(Nob_Cmd *cmd) { (void)cmd; }
#endif

static void append_wayland_session_flags(Nob_Cmd *cmd) {
#if !defined(_WIN32)
  if (getenv("WAYLAND_DISPLAY"))
    nob_cmd_append(cmd, "-DFLINT_WAYLAND_SESSION");
#else
  (void)cmd;
#endif
}

static void write_json_string(FILE *f, const char *s) {
  fputc('"', f);
  for (const char *p = s; *p; ++p) {
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
  const char *cwd = nob_get_current_dir_temp();
  if (!cwd)
    return false;

  FILE *f = fopen("compile_commands.json", "w");
  if (!f) {
    return false;
  }

  fprintf(f, "[\n");
  for (size_t i = 0; i < sources.count; ++i) {
    if (i > 0)
      fprintf(f, ",\n");

    Nob_Cmd cmd = {0};
    nob_cc(&cmd);
    nob_cc_flags(&cmd);
    append_wayland_session_flags(&cmd);
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
  return true;
}

static bool is_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static bool stale_with_dep(const char *src, const char *obj,
                           const char *dep_file_path) {
  if (!nob_file_exists(obj))
    return true;
  if (nob_needs_rebuild1(obj, src) != 0)
    return true;

  Nob_String_Builder dep = {0};
  if (!nob_read_entire_file(dep_file_path, &dep))
    return true;

  Nob_String_View sv = nob_sv_from_parts(dep.items, dep.count);
  size_t i = 0;
  while (i < sv.count && sv.data[i] != ':')
    i++;
  if (i >= sv.count)
    return true;
  sv = nob_sv_from_parts(sv.data + i + 1, sv.count - i - 1);

  while (sv.count > 0) {
    size_t k = 0;
    while (k < sv.count && is_space(sv.data[k]))
      k++;
    sv = nob_sv_from_parts(sv.data + k, sv.count - k);
    if (sv.count == 0)
      break;

    size_t j = 0;
    while (j < sv.count && !is_space(sv.data[j]))
      j++;
    Nob_String_View tok = nob_sv_from_parts(sv.data, j);
    sv = nob_sv_from_parts(sv.data + j, sv.count - j);

    if (tok.count == 0)
      continue;
    if (tok.data[tok.count - 1] == '\\')
      continue;
    if (tok.data[tok.count - 1] == ':')
      continue;

    char *dep_file = nob_temp_sprintf("%.*s", (int)tok.count, tok.data);
    if (nob_needs_rebuild1(obj, dep_file) != 0) {
      return true;
    }
  }
  return false;
}

static bool stale(const char *src, const char *obj) {
  return stale_with_dep(src, obj, dep_path(src));
}

static void append_feature_build_cmd(Nob_Cmd *cmd, const char *src) {
  const char *so = feature_so_path(src);
  nob_cc(cmd);
  nob_cc_flags(cmd);
#if defined(_WIN32)
  nob_cmd_append(cmd, CC_FLAGS, "-shared", "-o", so, src, "build/main.lib.a",
                 "-MF", feature_dep_path(src), LINK_FLAGS);
#else
  nob_cmd_append(cmd, CC_FLAGS, "-fPIC", "-shared", "-o", so, src, "-MF",
                 feature_dep_path(src));
  append_dep_cflags(cmd);
  nob_cmd_append(cmd, "-lX11");
#if defined(__APPLE__)
  nob_cmd_append(cmd, LINK_FLAGS);
#endif
#endif
}

static bool build_features(void) {
  size_t max_procs = (size_t)nob_nprocs();
  if (max_procs == 0)
    max_procs = 6;
  max_procs += 1;

  bool ok = true;
  Nob_Procs procs = {0};
  for (size_t i = 0; i < feature_sources.count; ++i) {
    const char *src = feature_sources.items[i];
    const char *so = feature_so_path(src);
    if (nob_file_exists(so) && !stale_with_dep(src, so, feature_dep_path(src)))
      continue;
    Nob_Cmd cmd = {0};
    append_feature_build_cmd(&cmd, src);
    if (!nob_cmd_run(&cmd, .async = &procs, .max_procs = max_procs))
      ok = false;
  }
  if (!nob_procs_wait_and_reset(&procs))
    ok = false;
  return ok;
}

static Nob_File_Paths to_build = {0};
static Nob_File_Paths err_paths = {0};
static Nob_File_Paths printed_sigs = {0};

static char *err_signature(const char *err) {
  Nob_String_Builder sb = {0};
  if (!nob_read_entire_file(err, &sb) || sb.count == 0)
    return NULL;
  nob_sb_append_null(&sb);

  Nob_String_Builder sig = {0};
  char *save = NULL;
  char *line = strtok_r(sb.items, "\n", &save);
  while (line) {
    if (strstr(line, "error:")) {
      nob_sb_append_buf(&sig, line, strlen(line));
      nob_sb_append(&sig, '\n');
    }
    line = strtok_r(NULL, "\n", &save);
  }
  if (sig.count == 0)
    return NULL;
  nob_sb_append_null(&sig);
  return strdup(sig.items);
}

static bool print_failures(void) {
  Nob_String_Builder failed_names = {0};
  size_t failed_count = 0;

  for (size_t i = 0; i < to_build.count; ++i) {
    char *sig = err_signature(err_paths.items[i]);
    if (!sig)
      continue;

    failed_count++;
    if (failed_names.count > 0)
      nob_sb_append_buf(&failed_names, ", ", 2);
    nob_sb_append_buf(&failed_names, to_build.items[i],
                      strlen(to_build.items[i]));

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
        fwrite(block.items, 1, block.count, stdout);
        printf("\n");
      }
    } else {
      free(sig);
    }
  }

  if (failed_count > 0) {
    nob_sb_append_null(&failed_names);
    return false;
  }
  return true;
}

static bool compile_all(bool *any_compiled) {
  *any_compiled = to_build.count > 0;
  if (to_build.count == 0)
    return true;

  bool ok = true;
  Nob_Procs procs = {0};
  size_t _nprocs = (size_t)nob_nprocs();
  if (_nprocs == 0)
    _nprocs = 6;
  size_t max_procs = _nprocs + 1;

  for (size_t i = 0; i < to_build.count; ++i) {
    Nob_Cmd cmd = {0};
    nob_cc(&cmd);
    nob_cc_flags(&cmd);
    append_wayland_session_flags(&cmd);
    nob_cmd_append(&cmd, CC_FLAGS, "-c", to_build.items[i], "-o",
                   obj_path(to_build.items[i]));
    append_dep_cflags(&cmd);
    if (!nob_cmd_run(&cmd, .async = &procs, .max_procs = max_procs,
                     .stderr_path = err_paths.items[i])) {

      ok = false;
    }
  }

  if (!nob_procs_wait_and_reset(&procs))
    ok = false;
  return print_failures() && ok;
}

static bool instance_running(void) {

  const char *marker = "/tmp/flint_nob_check";
  Nob_Cmd chk = {0};
  nob_cmd_append(
      &chk, "sh", "-c",
      "if systemctl --user is-active --quiet flint.service 2>/dev/null "
      "|| pgrep -f 'build/[f]lint' >/dev/null 2>&1; then echo running; fi; "
      "exit 0");
  if (!nob_cmd_run(&chk, .stdout_path = marker))
    return false;
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
  if (!instance_running())
    return;

  Nob_Cmd cmd = {0};
  nob_cmd_append(&cmd, "sh", "-c",
                 "systemctl --user stop flint.service 2>/dev/null; "
                 "pkill -f 'build/[f]lint' 2>/dev/null; "
                 "exit 0");
  if (nob_cmd_run(&cmd)) {
    nob_log(NOB_INFO, "Stopped previously running flint instance");
  }
}

static bool run_output(const char *output) {
  stop_running_instance();
  Nob_Cmd cmd = {0};
  nob_cmd_append(&cmd, output);
  return nob_cmd_run_async_and_reset(&cmd) != NOB_INVALID_PROC;
}

static bool run_tests(void) {
  if (!nob_mkdir_if_not_exists("build/tests"))
    return false;
  if (test_sources.count == 0) {
    return true;
  }
  const char *support_obj = NULL;
  if (nob_file_exists("tests/support.c")) {
    support_obj = "build/tests/support.o";
    Nob_Cmd scmd = {0};
    nob_cc(&scmd);
    nob_cmd_append(&scmd, "-I", "include", "-I", "tests", "-O2", "-c",
                   "tests/support.c", "-o", support_obj);
    if (!nob_cmd_run(&scmd)) {
      return false;
    }
  }
  size_t failed = 0;
  size_t suite_count = test_sources.count;
  char (*suite_names)[128] =
      calloc(suite_count ? suite_count : 1, sizeof(char[128]));
  bool *suite_ok = calloc(suite_count ? suite_count : 1, sizeof(bool));
  Nob_Log_Level prev_level = nob_minimal_log_level;
  nob_minimal_log_level = NOB_ERROR;
  for (size_t t = 0; t < test_sources.count; ++t) {
    const char *src = test_sources.items[t];
    const char *base = nob_path_name(src);
    char name[128];
    snprintf(name, sizeof(name), "%s", base);
    char *dot = strrchr(name, '.');
    if (dot)
      *dot = '\0';
    const char *out = nob_temp_sprintf("build/tests/%s", name);
#if defined(_WIN32)
    if (!nob_sv_ends_with_cstr(nob_sv_from_cstr(out), ".exe"))
      out = nob_temp_sprintf("%s.exe", out);
#endif
    Nob_Cmd cmd = {0};
    nob_cc(&cmd);
    nob_cmd_append(&cmd, "-I", "include", "-I", "tests", "-O2", "-Wall", src);
    append_dep_cflags(&cmd);
    if (support_obj)
      nob_cmd_append(&cmd, support_obj);
    for (size_t i = 0; i < sources.count; ++i) {
      if (strcmp(sources.items[i], "src/main.c") == 0)
        continue;
      if (strcmp(sources.items[i], "src\\main.c") == 0)
        continue;
      nob_cmd_append(&cmd, obj_path(sources.items[i]));
    }
    append_dep_ldflags(&cmd);
    nob_cmd_append(&cmd, LINK_FLAGS, "-o", out);
    snprintf(suite_names[t], sizeof(suite_names[t]), "%s", name);
    if (!nob_cmd_run(&cmd)) {
      snprintf(suite_names[t], sizeof(suite_names[t]), "%s (BUILD FAILED)",
               name);
      suite_ok[t] = false;
      failed++;
      continue;
    }
    Nob_Cmd run = {0};
    nob_cmd_append(&run, out);
    suite_ok[t] = nob_cmd_run(&run);
    if (!suite_ok[t])
      failed++;
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

#define SRC_FOLDER "src"
#define BUILD_FOLDER "build"
#define WIN_OBJ_FOLDER BUILD_FOLDER "/win_obj"
#define OUTPUT BUILD_FOLDER "/flint_win.exe"
#define CROSS_CC "x86_64-w64-mingw32-gcc"

#define WIN_CC_FLAGS                                                               \
  "-std=gnu17", "-DCOBJMACROS", "-DWINVER=0x0601", "-D_WIN32_WINNT=0x0601",    \
      "-I/usr/x86_64-w64-mingw32/include", "-I", "include", "-MMD", "-MP",     \
      "-O3", "-Wall", "-Wextra", "-Wno-sign-compare", "-Wno-unused-parameter"
#define WIN_LINK_FLAGS                                                             \
  "-lcomctl32", "-lgdiplus", "-lgdi32", "-lshlwapi", "-lshell32", "-lwinmm",   \
      "-lole32", "-luuid", "-lmsimg32", "-ldwmapi", "-lpsapi", "-lcrypt32",    \
      "-lcomdlg32", "-ladvapi32", "-ldxva2", "-luser32", "-lwinhttp",          \
      "-Lbuild/winlibs", "-liphlpapi"

static Nob_File_Paths win_sources = {0};
static Nob_File_Paths win_feature_sources = {0};
static Nob_File_Paths win_test_sources = {0};

static bool win_collect_source(Nob_Walk_Entry entry) {
  if (entry.type == NOB_FILE_REGULAR) {
    Nob_String_View path = nob_sv_from_cstr(entry.path);
    if (nob_sv_ends_with_cstr(path, ".c")) {
      if (nob_sv_starts_with(path, nob_sv_from_cstr("tests/")) ||
          nob_sv_starts_with(path, nob_sv_from_cstr("tests\\"))) {
        const char *base = nob_path_name(path.data);
        if (strncmp(base, "test_", 5) == 0)
          nob_da_append(&win_test_sources, strdup(path.data));
        return true;
      }
      if (nob_sv_ends_with_cstr(path, "_x11.c"))
        return true;
      if (nob_sv_ends_with_cstr(path, "_macos.c"))
        return true;
      if (nob_sv_ends_with_cstr(path, "_wayland.c"))
        return true;
      if (nob_sv_ends_with_cstr(path, "wm.c"))
        return true;
      bool is_feature = false;
      if (nob_sv_starts_with(path, nob_sv_from_cstr("src/features/")) ||
          nob_sv_starts_with(path, nob_sv_from_cstr("src\\features\\"))) {
        is_feature = true;
      }
      if (is_feature) {
        nob_da_append(&win_feature_sources, strdup(path.data));
      } else {
        nob_da_append(&win_sources, strdup(path.data));
      }
    }
  }
  return true;
}

static const char *win_obj_path(const char *src) {
  return nob_temp_sprintf(WIN_OBJ_FOLDER "/%s.o", nob_path_name(src));
}

static const char *win_dep_path(const char *src) {
  return nob_temp_sprintf(WIN_OBJ_FOLDER "/%s.d", nob_path_name(src));
}

static const char *win_err_path(const char *src) {
  return nob_temp_sprintf(WIN_OBJ_FOLDER "/%s.err", nob_path_name(src));
}

static void unset_include_env(void) {
  unsetenv("CPATH");
  unsetenv("C_INCLUDE_PATH");
  unsetenv("CPLUS_INCLUDE_PATH");
  unsetenv("OBJC_INCLUDE_PATH");
  unsetenv("LIBRARY_PATH");
}

static bool win_is_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static bool win_stale(const char *src, const char *obj) {
  if (!nob_file_exists(obj))
    return true;
  if (nob_needs_rebuild1(obj, src) != 0)
    return true;

  Nob_String_Builder dep = {0};
  if (!nob_read_entire_file(win_dep_path(src), &dep))
    return true;

  Nob_String_View sv = nob_sv_from_parts(dep.items, dep.count);
  size_t i = 0;
  while (i < sv.count && sv.data[i] != ':')
    i++;
  if (i >= sv.count)
    return true;
  sv = nob_sv_from_parts(sv.data + i + 1, sv.count - i - 1);

  while (sv.count > 0) {
    size_t k = 0;
    while (k < sv.count && win_is_space(sv.data[k]))
      k++;
    sv = nob_sv_from_parts(sv.data + k, sv.count - k);
    if (sv.count == 0)
      break;

    size_t j = 0;
    while (j < sv.count && !win_is_space(sv.data[j]))
      j++;
    Nob_String_View tok = nob_sv_from_parts(sv.data, j);
    sv = nob_sv_from_parts(sv.data + j, sv.count - j);

    if (tok.count == 0)
      continue;
    if (tok.data[tok.count - 1] == '\\')
      continue;
    if (tok.data[tok.count - 1] == ':')
      continue;

    char *dep_file = nob_temp_sprintf("%.*s", (int)tok.count, tok.data);
    if (nob_needs_rebuild1(obj, dep_file) != 0)
      return true;
  }
  return false;
}

static Nob_File_Paths win_to_build = {0};
static Nob_File_Paths win_err_paths = {0};
static Nob_File_Paths win_printed_sigs = {0};

static char *win_err_signature(const char *err) {
  Nob_String_Builder sb = {0};
  if (!nob_read_entire_file(err, &sb) || sb.count == 0)
    return NULL;
  nob_sb_append_null(&sb);

  Nob_String_Builder sig = {0};
  char *save = NULL;
  char *line = strtok_r(sb.items, "\n", &save);
  while (line) {
    if (strstr(line, "error:")) {
      nob_sb_append_buf(&sig, line, strlen(line));
      nob_sb_append(&sig, '\n');
    }
    line = strtok_r(NULL, "\n", &save);
  }
  if (sig.count == 0)
    return NULL;
  nob_sb_append_null(&sig);
  return strdup(sig.items);
}

static bool win_print_failures(void) {
  Nob_String_Builder failed_names = {0};
  size_t failed_count = 0;

  for (size_t i = 0; i < win_to_build.count; ++i) {
    char *sig = win_err_signature(win_err_paths.items[i]);
    if (!sig)
      continue;

    failed_count++;
    if (failed_names.count > 0)
      nob_sb_append_buf(&failed_names, ", ", 2);
    nob_sb_append_buf(&failed_names, win_to_build.items[i],
                      strlen(win_to_build.items[i]));

    bool already_printed = false;
    for (size_t k = 0; k < win_printed_sigs.count; ++k) {
      if (strcmp(win_printed_sigs.items[k], sig) == 0) {
        already_printed = true;
        break;
      }
    }
    if (!already_printed) {
      nob_da_append(&win_printed_sigs, sig);
      Nob_String_Builder block = {0};
      if (nob_read_entire_file(win_err_paths.items[i], &block)) {
        nob_log(NOB_ERROR, "%s:", win_to_build.items[i]);
        fwrite(block.items, 1, block.count, stdout);
        printf("\n");
      }
    } else {
      free(sig);
    }
  }

  if (failed_count > 0) {
    nob_sb_append_null(&failed_names);
    nob_log(NOB_ERROR, "Compilation failed (%zu file%s): %s", failed_count,
            failed_count == 1 ? "" : "s", failed_names.items);
    return false;
  }
  return true;
}

static bool win_compile_all(bool *any_compiled) {
  *any_compiled = win_to_build.count > 0;
  if (win_to_build.count == 0)
    return true;

  bool ok = true;
  Nob_Procs procs = {0};
  size_t max_procs = (size_t)nob_nprocs() + 1;

  for (size_t i = 0; i < win_to_build.count; ++i) {
    Nob_Cmd cmd = {0};
    nob_cc(&cmd);

    cmd.items[0] = (char *)CROSS_CC;
    nob_cmd_append(&cmd, WIN_CC_FLAGS, "-c", win_to_build.items[i], "-o",
                   win_obj_path(win_to_build.items[i]));
    if (!nob_cmd_run(&cmd, .async = &procs, .max_procs = max_procs,
                     .stderr_path = win_err_paths.items[i])) {
      ok = false;
    }
  }

  if (!nob_procs_wait_and_reset(&procs))
    ok = false;
  return win_print_failures() && ok;
}

static bool win_run_tests_cross(void) {
  if (!nob_mkdir_if_not_exists("build/win_tests"))
    return false;
  if (win_test_sources.count == 0) {
    nob_log(NOB_INFO, "No tests found in tests/");
    return true;
  }
  const char *support_obj = NULL;
  if (nob_file_exists("tests/support.c")) {
    support_obj = "build/win_tests/support.o";
    Nob_Cmd scmd = {0};
    nob_cmd_append(&scmd, CROSS_CC);
    nob_cmd_append(&scmd, "-I", "include", "-I", "tests", "-O2", "-c",
                   "tests/support.c", "-o", support_obj);
    nob_log(NOB_INFO, "Building test support %s", support_obj);
    if (!nob_cmd_run(&scmd)) {
      nob_log(NOB_ERROR, "Failed to build test support");
      return false;
    }
  }
  size_t failed = 0;
  size_t built = 0;
  for (size_t t = 0; t < win_test_sources.count; ++t) {
    const char *src = win_test_sources.items[t];
    const char *base = nob_path_name(src);
    char name[128];
    snprintf(name, sizeof(name), "%s", base);
    char *dot = strrchr(name, '.');
    if (dot)
      *dot = '\0';
    const char *out = nob_temp_sprintf("build/win_tests/%s.exe", name);
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, CROSS_CC);
    nob_cmd_append(&cmd, "-I", "include", "-I", "tests", "-O2", "-Wall", src);
    if (support_obj)
      nob_cmd_append(&cmd, support_obj);
    for (size_t i = 0; i < win_sources.count; ++i) {
      if (strcmp(win_sources.items[i], "src/main.c") == 0)
        continue;
      if (strcmp(win_sources.items[i], "src\\main.c") == 0)
        continue;
      nob_cmd_append(&cmd, win_obj_path(win_sources.items[i]));
    }
    nob_cmd_append(&cmd, WIN_LINK_FLAGS, "-o", out);
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

static int main_win32(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);
  unset_include_env();

  int do_test = 0;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-test") == 0)
      do_test = 1;
  }

  if (!nob_mkdir_if_not_exists(BUILD_FOLDER))
    return 1;
  if (!nob_mkdir_if_not_exists(WIN_OBJ_FOLDER))
    return 1;
  if (!nob_mkdir_if_not_exists("build/winlibs"))
    return 1;
  {
    Nob_Cmd sh = {0};
    nob_cmd_append(&sh, "sh", "-c",
                   "set -- /usr/x86_64-w64-mingw32/lib/libmcfgthread.a "
                   "/nix/store/*mcfgthread*mingw32*/lib/libmcfgthread.a; "
                   "for f in \"$@\"; do [ -f \"$f\" ] && ln -sf \"$f\" "
                   "build/winlibs/libmcfgthread.a && exit 0; done; "
                   "exit 0");
    if (!nob_cmd_run(&sh))
      nob_log(NOB_ERROR, "Failed to stage libmcfgthread");
  }
  if (!nob_walk_dir(SRC_FOLDER, win_collect_source)) {
    nob_log(NOB_ERROR, "Failed to walk " SRC_FOLDER);
    return 1;
  }
  if (!nob_walk_dir("tests", win_collect_source)) {
    nob_log(NOB_ERROR, "Failed to walk tests");
    return 1;
  }

  for (size_t i = 0; i < win_sources.count; ++i) {
    const char *src = win_sources.items[i];
    if (win_stale(src, win_obj_path(src))) {
      nob_da_append(&win_to_build, src);
      nob_da_append(&win_err_paths, strdup(win_err_path(src)));
    }
  }

  bool any_compiled = false;
  if (!win_compile_all(&any_compiled))
    return 1;

  if (do_test && !win_run_tests_cross())
    return 1;
  if (do_test)
    return 0;

  const char *output = OUTPUT;
  bool features_stale = false;
  for (size_t i = 0; i < win_feature_sources.count; ++i) {
    const char *src = win_feature_sources.items[i];
    size_t len = strlen(nob_path_name(src));
    char *no_ext = malloc(len + 1);
    strcpy(no_ext, nob_path_name(src));
    char *dot = strrchr(no_ext, '.');
    if (dot)
      *dot = '\0';
    char *so = nob_temp_sprintf("build/features/%s.dll", no_ext);
    if (!nob_file_exists(so) || nob_needs_rebuild1(so, src) != 0)
      features_stale = true;
    free(no_ext);
    if (features_stale)
      break;
  }
  if (!any_compiled && !features_stale && nob_file_exists(output) &&
      nob_needs_rebuild(output, (const char **)win_to_build.items,
                        win_to_build.count) == 0) {
    nob_log(NOB_INFO, "No changes, skipping link");
    return 0;
  }
  if (!any_compiled && !features_stale) {
    nob_log(NOB_INFO, "No changes to core, skipping link");
  }

  Nob_File_Paths objs = {0};
  for (size_t i = 0; i < win_sources.count; ++i) {
    nob_da_append(&objs, strdup(win_obj_path(win_sources.items[i])));
  }

  Nob_Cmd cmd = {0};
  nob_cmd_append(&cmd, CROSS_CC, "-o", output);
  for (size_t i = 0; i < objs.count; ++i) {
    nob_cmd_append(&cmd, objs.items[i]);
  }
  nob_cmd_append(&cmd, "-Wl,--export-all-symbols",
                 "-Wl,--out-implib,build/win_obj/main.lib.a");
  nob_cmd_append(&cmd, WIN_LINK_FLAGS);

  nob_log(NOB_INFO, "Linking %s", output);
  if (!nob_cmd_run(&cmd)) {
    nob_log(NOB_ERROR, "Failed to link %s", output);
    return 1;
  }

  {
    if (!nob_mkdir_if_not_exists("build/features"))
      return 1;
    for (size_t i = 0; i < win_feature_sources.count; ++i) {
      const char *src = win_feature_sources.items[i];
      const char *base = nob_path_name(src);
      size_t len = strlen(base);
      char *no_ext = malloc(len + 1);
      strcpy(no_ext, base);
      char *dot = strrchr(no_ext, '.');
      if (dot)
        *dot = '\0';
      const char *so = nob_temp_sprintf("build/features/%s.dll", no_ext);
      bool need_so = !nob_file_exists(so) || nob_needs_rebuild1(so, src) != 0;
      if (nob_file_exists("build/win_obj/main.lib.a") &&
          nob_needs_rebuild1(so, "build/win_obj/main.lib.a") != 0)
        need_so = true;
      if (!need_so)
        continue;
      Nob_Cmd fcmd = {0};
      nob_cmd_append(&fcmd, CROSS_CC, WIN_CC_FLAGS, "-shared", "-o", so, src,
                     "build/win_obj/main.lib.a", WIN_LINK_FLAGS);
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

int main(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF_PLUS(argc, argv, "nob.h");
  for (int _i = 1; _i < argc; _i++) {
    if (strcmp(argv[_i], "-win32") == 0) return main_win32(argc, argv);
  }

  int do_run = 0;
  int do_actual = 0;
  int do_test = 0;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "--help") == 0) {
      printf("Usage: %s [-run] [-actual] [-test] [-help]\n", argv[0]);
      printf("  -run    Run flint after build\n");
      printf("  -actual Build flint.exe instead of debug build (Windows)\n");
      printf("  -test   Run test suite\n");
      printf("  -help   Show this help\n");
      return 0;
    }
    if (strcmp(argv[i], "-run") == 0)
      do_run = 1;
    if (strcmp(argv[i], "-actual") == 0)
      do_actual = 1;
    if (strcmp(argv[i], "-test") == 0)
      do_test = 1;
  }

  if (!nob_mkdir_if_not_exists(BUILD_FOLDER))
    return 1;
  if (!nob_mkdir_if_not_exists(OBJ_FOLDER))
    return 1;
  if (!nob_walk_dir(SRC_FOLDER, collect_source)) {
    return 1;
  }
  if (!nob_walk_dir("tests", collect_source)) {
    return 1;
  }

#if !defined(_WIN32)
  discover_dep_flags();
#endif

  write_compile_commands();

  for (size_t i = 0; i < sources.count; ++i) {
    const char *src = sources.items[i];
    if (stale(src, obj_path(src))) {
      nob_da_append(&to_build, src);
      nob_da_append(&err_paths, strdup(err_path(src)));
    }
  }

  if (!nob_mkdir_if_not_exists("build/features"))
    return 1;
#if defined(_WIN32)
  if (!nob_mkdir_if_not_exists("build/debug"))
    return 1;
#endif

  bool any_compiled = false;
  if (!compile_all(&any_compiled))
    return 1;

#if defined(_WIN32)
  const char *output = do_actual ? "build/flint.exe" : "build/debug/flint.exe";
#else
  const char *output = CONSOLE_OUTPUT;
#endif
  Nob_File_Paths objs = {0};
  for (size_t i = 0; i < sources.count; ++i) {
    nob_da_append(&objs, strdup(obj_path(sources.items[i])));
  }

  bool needs_link =
      any_compiled || !nob_file_exists(output) ||
      nob_needs_rebuild(output, (const char **)objs.items, objs.count) != 0;
#if defined(_WIN32)
  {
    const char *other = do_actual ? "build/debug/flint.exe" : "build/flint.exe";
    if (nob_file_exists(other) &&
        nob_needs_rebuild(other, (const char **)objs.items, objs.count) != 0) {
      needs_link = true;
    }
  }
#endif
  if (!needs_link) {
    nob_log(NOB_INFO, "No changes, skipping link");
    nob_da_free(objs);
    if (!build_features())
      return 1;
    if (do_run && !run_output(output))
      return 1;
    if (do_test && !run_tests())
      return 1;
    return 0;
  }

  Nob_Cmd cmd = {0};
  nob_cc(&cmd);
  nob_cmd_append(&cmd, "-o", output);
  for (size_t i = 0; i < objs.count; ++i) {
    nob_cmd_append(&cmd, objs.items[i]);
  }
#if defined(_WIN32)
  if (do_actual)
      nob_cmd_append(&cmd, "-mwindows");
  nob_cmd_append(&cmd, "-Wl,--export-all-symbols",
                 "-Wl,--out-implib,build/main.lib.a");
#endif
  append_dep_ldflags(&cmd);
  nob_cmd_append(&cmd, LINK_FLAGS);
  bool linked = nob_cmd_run(&cmd);
  nob_da_free(objs);
  if (!linked)
    return 1;
  if (!build_features())
    return 1;

#if !defined(_WIN32)
  {
    Nob_Cmd chmod = {0};
    nob_cmd_append(&chmod, "chmod", "+x", output);
    nob_cmd_run(&chmod);
  }
#endif
  stop_running_instance();
  if (do_run && !run_output(output))
    return 1;
  nob_log(NOB_INFO, "Built Successfully");
  if (do_test && !run_tests())
    return 1;
  return 0;
}
