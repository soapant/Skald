#define NOB_IMPLEMENTATION
#define NOB_EXPERIMENTAL_DELETE_OLD
#include "nob.h"

#define SRC_FOLDER "src"
#define BUILD_FOLDER "build"

#define CC_COMMON_FLAGS                                                        \
  "-std=gnu17", "-DCOBJMACROS", "-I", "include", "-MMD", "-MP", "-Wall",       \
      "-Wextra", "-Wno-unused-parameter", "-ggdb"

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

#define WIN_CC_FLAGS                                                           \
  "-std=gnu17", "-DCOBJMACROS", "-DWINVER=0x0601", "-D_WIN32_WINNT=0x0601",    \
      "-I/usr/x86_64-w64-mingw32/include", "-I", "include", "-MMD", "-MP",     \
      "-O3", "-Wall", "-Wextra", "-Wno-sign-compare", "-Wno-unused-parameter"

#define WIN_LINK_FLAGS                                                         \
  "-lcomctl32", "-lgdiplus", "-lgdi32", "-lshlwapi", "-lshell32", "-lwinmm",   \
      "-lole32", "-luuid", "-lmsimg32", "-ldwmapi", "-lpsapi", "-lcrypt32",    \
      "-lcomdlg32", "-ladvapi32", "-ldxva2", "-luser32", "-lwinhttp",          \
      "-Lbuild/winlibs", "-liphlpapi"

#define CROSS_CC "x86_64-w64-mingw32-gcc"

static bool sanitize = false;
static const char *sanitize_cc = NULL;
static const char *sanitize_bin = NULL;

static void pick_sanitize_cc(void) {
  const char *over = getenv("SKALD_SANITIZE_CC");
  if (over && over[0]) {
    sanitize_cc = over;
  } else {
#if defined(_WIN32)
    static char buf[2048];
    const char *up = getenv("USERPROFILE");
    if (up && up[0]) {
      snprintf(buf, sizeof(buf),
               "%s/scoop/apps/msys2/current/clang64/bin/clang.exe", up);
      if (nob_file_exists(buf))
        sanitize_cc = buf;
    }
    if (!sanitize_cc)
      sanitize_cc = "clang64/bin/clang.exe";
#else
    sanitize_cc = "clang";
#endif
  }
  const char *slash = strrchr(sanitize_cc, '/');
  if (!slash)
    slash = strrchr(sanitize_cc, '\\');
  static char bin[2048];
  if (slash) {
    size_t n = (size_t)(slash - sanitize_cc);
    if (n >= sizeof(bin))
      n = sizeof(bin) - 1;
    memcpy(bin, sanitize_cc, n);
    bin[n] = '\0';
    sanitize_bin = bin;
  } else {
    sanitize_bin = ".";
  }
  nob_log(NOB_INFO, "sanitizer CC: %s (runtime dir %s)", sanitize_cc,
          sanitize_bin);
}

static void append_cc(Nob_Cmd *cmd) {
  if (sanitize)
    return (void)nob_cmd_append(cmd, sanitize_cc);
  nob_cc(cmd);
}

static void append_san_flags(Nob_Cmd *cmd) {
  if (sanitize)
    nob_cmd_append(cmd, "-fsanitize=address,undefined",
                   "-fno-omit-frame-pointer");
}

static bool target_win32 = false;
static const char *obj_dir = BUILD_FOLDER "/obj";

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
      if (target_win32) {
        if (nob_sv_ends_with_cstr(path, "_x11.c"))
          return true;
        if (nob_sv_ends_with_cstr(path, "_macos.c"))
          return true;
        if (nob_sv_ends_with_cstr(path, "_wayland.c"))
          return true;
        if (nob_sv_ends_with_cstr(path, "wm.c"))
          return true;
      } else {
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
#else
        if (nob_sv_ends_with_cstr(path, "_win32.c"))
          return true;
        if (nob_sv_ends_with_cstr(path, "_macos.c"))
          return true;
#endif
        if (!getenv("WAYLAND_DISPLAY"))
          if (nob_sv_ends_with_cstr(path, "_wayland.c"))
            return true;
      }
      bool is_feature = false;
      // hardcoded until skald releases
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
  if (target_win32)
    return ".dll";
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
  char *flat = nob_temp_strdup(src);
  for (char *p = flat; *p; p++)
    if (*p == '/' || *p == '\\')
      *p = '_';
  return nob_temp_sprintf("%s/%s%s", obj_dir, flat, ext);
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

static void discover_dep_flags(void) {
#if defined(__APPLE__)
  return;
#else
  if (target_win32)
    return;
  char cmdline[512];
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
    };
    for (size_t i = 0; i < sizeof(fb) / sizeof(fb[0]); i++)
      nob_da_append(&dep_cflags, nob_temp_strdup(fb[i]));
  }
  snprintf(cmdline, sizeof(cmdline),
           "pkg-config --static --libs %s 2>/dev/null", PKG_LIBS_PKGS_STR);
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
  if (target_win32)
    return;
#if !defined(_WIN32)
  if (getenv("WAYLAND_DISPLAY"))
    nob_cmd_append(cmd, "-DSKALD_WAYLAND_SESSION");
#else
  (void)cmd;
#endif
}

static void nob_write_json_string(FILE *f, const char *s);

static bool write_compile_commands(void) {
  const char *cwd = nob_get_current_dir_temp();
  if (!cwd)
    return false;

  FILE *f = fopen("compile_commands.json", "w");
  if (!f)
    return false;

  fprintf(f, "[\n");
  for (size_t i = 0; i < sources.count; ++i) {
    if (i > 0)
      fprintf(f, ",\n");

    Nob_Cmd cmd = {0};
    if (target_win32) {
      nob_cmd_append(&cmd, CROSS_CC);
      nob_cmd_append(&cmd, WIN_CC_FLAGS, "-c", sources.items[i]);
    } else {
      append_cc(&cmd);
      nob_cc_flags(&cmd);
      append_wayland_session_flags(&cmd);
      nob_cmd_append(&cmd, CC_FLAGS, "-c", sources.items[i]);
      append_dep_cflags(&cmd);
    }

    fprintf(f, "  {\n");
    fprintf(f, "    \"directory\": ");
    nob_write_json_string(f, cwd);
    fprintf(f, ",\n");
    fprintf(f, "    \"file\": ");
    nob_write_json_string(f, sources.items[i]);
    fprintf(f, ",\n");
    fprintf(f, "    \"arguments\": [\n");
    for (size_t j = 0; j < cmd.count; ++j) {
      fprintf(f, "      ");
      nob_write_json_string(f, cmd.items[j]);
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
    if (nob_needs_rebuild1(obj, dep_file) != 0)
      return true;
  }
  return false;
}

static bool stale(const char *src, const char *obj) {
  return stale_with_dep(src, obj, dep_path(src));
}

static void append_feature_build_cmd(Nob_Cmd *cmd, const char *src) {
  const char *so = feature_so_path(src);
  if (target_win32) {
    nob_cmd_append(cmd, CROSS_CC, WIN_CC_FLAGS, "-shared", "-o", so, src,
                   "build/win_obj/main.lib.a", WIN_LINK_FLAGS);
  } else {
    append_cc(cmd);
    nob_cc_flags(cmd);
    append_san_flags(cmd);
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
    bool need_rebuild = !nob_file_exists(so);
    if (!need_rebuild) {
      if (target_win32) {
        need_rebuild =
            nob_needs_rebuild1(so, src) != 0 ||
            (nob_file_exists("build/win_obj/main.lib.a") &&
             nob_needs_rebuild1(so, "build/win_obj/main.lib.a") != 0);
      } else {
        need_rebuild = stale_with_dep(src, so, feature_dep_path(src));
      }
    }
    if (!need_rebuild)
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
    if (target_win32) {
      nob_cmd_append(&cmd, CROSS_CC);
      nob_cmd_append(&cmd, WIN_CC_FLAGS, "-c", to_build.items[i], "-o",
                     obj_path(to_build.items[i]));
    } else {
      append_cc(&cmd);
      nob_cc_flags(&cmd);
      append_wayland_session_flags(&cmd);
      nob_cmd_append(&cmd, CC_FLAGS, "-c", to_build.items[i], "-o",
                     obj_path(to_build.items[i]));
      append_dep_cflags(&cmd);
      append_san_flags(&cmd);
    }
    if (!nob_cmd_run(&cmd, .async = &procs, .max_procs = max_procs,
                     .stderr_path = err_paths.items[i])) {
      ok = false;
    }
  }

  if (!nob_procs_wait_and_reset(&procs))
    ok = false;
  if (!ok) {
    for (size_t i = 0; i < err_paths.count; i++) {
      Nob_String_Builder sb = {0};
      if (nob_read_entire_file(err_paths.items[i], &sb)) {
        nob_log(NOB_ERROR, "%s", sb.items);
        nob_sb_free(sb);
      }
    }
  }
  return ok;
}

static bool instance_running(void) {
  const char *marker = "/tmp/skald_nob_check";
  Nob_Cmd chk = {0};
  nob_cmd_append(
      &chk, "sh", "-c",
      "if systemctl --user is-active --quiet skald.service 2>/dev/null "
      "|| pgrep -f 'build/[s]kald' >/dev/null 2>&1; then echo running; fi; "
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
                 "systemctl --user stop skald.service 2>/dev/null; "
                 "pkill -f 'build/[s]kald' 2>/dev/null; "
                 "exit 0");
  if (nob_cmd_run(&cmd)) {
    nob_log(NOB_INFO, "Stopped previously running skald instance");
  }
}

static bool run_output(const char *output) {
  stop_running_instance();
  Nob_Cmd cmd = {0};
#if defined(_WIN32)
  if (sanitize) {
    char wrapper[2048];
    char out_win[2048];
    snprintf(out_win, sizeof(out_win), "%s", output);
    for (char *p = out_win; *p; p++)
      if (*p == '/')
        *p = '\\';
    snprintf(wrapper, sizeof(wrapper), "set PATH=%s;%%PATH%% && %s",
             sanitize_bin, out_win);
    nob_cmd_append(&cmd, "cmd.exe", "/c", wrapper);
    return nob_cmd_run(&cmd);
  }
#endif
  nob_cmd_append(&cmd, output);
  return nob_cmd_run_async_and_reset(&cmd) != NOB_INVALID_PROC;
}

static bool run_tests(void) {
  const char *test_build_folder = sanitize       ? "build/san_tests"
                                  : target_win32 ? "build/win_tests"
                                                 : "build/tests";
  if (!nob_mkdir_if_not_exists(test_build_folder))
    return false;
  if (test_sources.count == 0) {
    nob_log(NOB_INFO, "No tests found in tests/");
    return true;
  }
  const char *support_obj = NULL;
  if (nob_file_exists("tests/support.c")) {
    support_obj = nob_temp_sprintf("%s/support.o", test_build_folder);
    Nob_Cmd scmd = {0};
    if (target_win32) {
      nob_cmd_append(&scmd, CROSS_CC);
    } else {
      append_cc(&scmd);
    }
    append_san_flags(&scmd);
    nob_cmd_append(&scmd, "-I", "include", "-I", "tests", "-O2", "-c",
                   "tests/support.c", "-o", support_obj);
    if (!nob_cmd_run(&scmd)) {
      nob_log(NOB_ERROR, "Failed to build test support");
      return false;
    }
  }

  if (!target_win32) {
    char fixture_dir[512];
    snprintf(fixture_dir, sizeof(fixture_dir), "%s/fixture", test_build_folder);
    if (!nob_mkdir_if_not_exists(fixture_dir))
      return false;
    if (nob_file_exists(nob_temp_sprintf("%s/reload_probe.dll", fixture_dir)))
      nob_delete_file(nob_temp_sprintf("%s/reload_probe.dll", fixture_dir));
    if (nob_file_exists(nob_temp_sprintf("%s/reload_probe.so", fixture_dir)))
      nob_delete_file(nob_temp_sprintf("%s/reload_probe.so", fixture_dir));
    if (nob_file_exists(nob_temp_sprintf("%s/reload_probe.dylib", fixture_dir)))
      nob_delete_file(nob_temp_sprintf("%s/reload_probe.dylib", fixture_dir));
    Nob_Cmd fcmd = {0};
    append_cc(&fcmd);
    nob_cc_flags(&fcmd);
    append_dep_cflags(&fcmd);
#if defined(__APPLE__)
    nob_cmd_append(&fcmd, "-dynamiclib", "-I", "include",
                   "tests/fixtures/reload_probe.c", "-o",
                   nob_temp_sprintf("%s/reload_probe.dylib", fixture_dir));
#elif defined(_WIN32)
    nob_cmd_append(&fcmd, "-shared", "-I", "include",
                   "tests/fixtures/reload_probe.c", "-o",
                   nob_temp_sprintf("%s/reload_probe.dll", fixture_dir));
#else
    nob_cmd_append(&fcmd, "-shared", "-fPIC", "-I", "include",
                   "tests/fixtures/reload_probe.c", "-o",
                   nob_temp_sprintf("%s/reload_probe.so", fixture_dir));
#endif
    if (!nob_cmd_run(&fcmd)) {
      nob_log(NOB_ERROR, "Failed to build reload_probe fixture");
      return false;
    }
  }

  size_t failed = 0;
  size_t suite_count = test_sources.count;

  if (target_win32) {
    size_t built = 0;
    for (size_t t = 0; t < test_sources.count; ++t) {
      const char *src = test_sources.items[t];
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
      for (size_t i = 0; i < sources.count; ++i) {
        if (strcmp(sources.items[i], "src/main.c") == 0 ||
            strcmp(sources.items[i], "src\\main.c") == 0)
          continue;
        nob_cmd_append(&cmd, obj_path(sources.items[i]));
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
    const char *out = nob_temp_sprintf("%s/%s", test_build_folder, name);
#if defined(_WIN32)
    if (!nob_sv_ends_with_cstr(nob_sv_from_cstr(out), ".exe"))
      out = nob_temp_sprintf("%s.exe", out);
#endif
    Nob_Cmd cmd = {0};
    append_cc(&cmd);
    nob_cmd_append(&cmd, "-I", "include", "-I", "tests", "-O2", "-Wall", src);
    append_san_flags(&cmd);
    append_dep_cflags(&cmd);
    if (support_obj)
      nob_cmd_append(&cmd, support_obj);
    for (size_t i = 0; i < sources.count; ++i) {
      if (strcmp(sources.items[i], "src/main.c") == 0 ||
          strcmp(sources.items[i], "src\\main.c") == 0)
        continue;
      nob_cmd_append(&cmd, obj_path(sources.items[i]));
    }
#if defined(_WIN32)
    nob_cmd_append(&cmd, "-Wl,--export-all-symbols");
#endif
    append_dep_ldflags(&cmd);
    append_san_flags(&cmd);
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
#if defined(_WIN32)
    if (sanitize) {
      char wrapper[2048];
      char out_win[2048];
      snprintf(out_win, sizeof(out_win), "%s", out);
      for (char *p = out_win; *p; p++)
        if (*p == '/')
          *p = '\\';
      snprintf(wrapper, sizeof(wrapper), "set PATH=%s;%%PATH%% && %s",
               sanitize_bin, out_win);
      nob_cmd_append(&run, "cmd.exe", "/c", wrapper);
    } else
#endif
    {
      nob_cmd_append(&run, out);
    }
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

static void nob_write_json_string(FILE *f, const char *s) {
  fputc('"', f);
  for (const char *p = s; *p; ++p) {
    if (*p == '"') {
      fputc('\\', f);
      fputc('"', f);
    } else if (*p == '\\') {
      if (target_win32) {
        fputc('/', f);
      } else {
#if defined(_WIN32)
        fputc('/', f);
#else
        fputc('\\', f);
        fputc('\\', f);
#endif
      }
    } else {
      fputc(*p, f);
    }
  }
  fputc('"', f);
}

int main(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF_PLUS(argc, argv, "nob.h");

  int do_run = 0;
  int do_release = 0;
  int do_test = 0;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "--help") == 0) {
      printf("Usage: %s [-win32] [-run] [-release] [-test] [-asan] [-help]\n",
             argv[0]);
      printf("  -win32  Cross compile for Windows using MinGW\n");
      printf("  -run    Run skald after build\n");
      printf("  -release Build skald.exe instead of debug build (Windows) and "
             "add -O3\n");
      printf("  -test   Run test suite\n");
      printf("  -asan   Build with clang -fsanitize=address,undefined\n");
      printf("  -help   Show this help\n");
      return 0;
    }
    if (strcmp(argv[i], "-win32") == 0)
      target_win32 = true;
    if (strcmp(argv[i], "-run") == 0)
      do_run = 1;
    if (strcmp(argv[i], "-release") == 0)
      do_release = 1;
    if (strcmp(argv[i], "-test") == 0)
      do_test = 1;
    if (strcmp(argv[i], "-asan") == 0)
      sanitize = true;
  }

  if (sanitize) {
    pick_sanitize_cc();
    obj_dir = BUILD_FOLDER "/san_obj";
  }

  if (target_win32) {
    obj_dir = BUILD_FOLDER "/win_obj";
    if (!nob_mkdir_if_not_exists(BUILD_FOLDER))
      return 1;
    if (!nob_mkdir_if_not_exists(obj_dir))
      return 1;
    if (!nob_mkdir_if_not_exists("build/winlibs"))
      return 1;
    Nob_Cmd sh = {0};
    nob_cmd_append(&sh, "sh", "-c",
                   "f=/usr/x86_64-w64-mingw32/lib/libmcfgthread.a; "
                   "[ -f \"$f\" ] && ln -sf \"$f\" "
                   "build/winlibs/libmcfgthread.a; "
                   "exit 0");
    if (!nob_cmd_run(&sh))
      nob_log(NOB_ERROR, "Failed to stage libmcfgthread");
  } else {
    if (!nob_mkdir_if_not_exists(BUILD_FOLDER))
      return 1;
    if (!nob_mkdir_if_not_exists(obj_dir))
      return 1;
#if defined(_WIN32)
    if (!nob_mkdir_if_not_exists("build/debug"))
      return 1;
#endif
  }

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
    const char *src = sources.items[i];
    if (stale(src, obj_path(src))) {
      nob_da_append(&to_build, src);
      nob_da_append(&err_paths, strdup(err_path(src)));
    }
  }

  if (!nob_mkdir_if_not_exists("build/features"))
    return 1;

  bool any_compiled = false;
  if (!compile_all(&any_compiled))
    return 1;

  if (do_test)
    return run_tests() ? 0 : 1;

  const char *output = NULL;
  if (target_win32) {
    output = BUILD_FOLDER "/skald_win.exe";
  } else {
#if defined(_WIN32)
    output = sanitize     ? "build/debug/skald_san.exe"
             : do_release ? "build/skald.exe"
                          : "build/debug/skald.exe";
#else
    output = BUILD_FOLDER "/skald";
#endif
  }

  Nob_File_Paths objs = {0};
  for (size_t i = 0; i < sources.count; ++i) {
    nob_da_append(&objs, strdup(obj_path(sources.items[i])));
  }

  bool needs_link = false;
  if (target_win32) {
    bool features_stale = false;
    for (size_t i = 0; i < feature_sources.count; ++i) {
      const char *src = feature_sources.items[i];
      const char *so = feature_so_path(src);
      if (!nob_file_exists(so) || nob_needs_rebuild1(so, src) != 0) {
        features_stale = true;
        break;
      }
    }
    needs_link =
        any_compiled || features_stale || !nob_file_exists(output) ||
        nob_needs_rebuild(output, (const char **)objs.items, objs.count) != 0;
    if (!needs_link) {
      nob_log(NOB_INFO, "No changes to core, skipping link");
    }
  } else {
    needs_link =
        any_compiled || !nob_file_exists(output) ||
        nob_needs_rebuild(output, (const char **)objs.items, objs.count) != 0;
#if defined(_WIN32)
    const char *other =
        do_release ? "build/debug/skald.exe" : "build/skald.exe";
    if (nob_file_exists(other) &&
        nob_needs_rebuild(other, (const char **)objs.items, objs.count) != 0) {
      needs_link = true;
    }
#endif
  }

  if (!needs_link) {
    nob_log(NOB_INFO, "No changes, skipping link");
    nob_da_free(objs);
    if (!build_features())
      return 1;
    if (do_run && !run_output(output))
      return 1;
    return 0;
  }

  Nob_Cmd cmd = {0};
  if (target_win32) {
    nob_cmd_append(&cmd, CROSS_CC, "-o", output);
    for (size_t i = 0; i < objs.count; ++i) {
      nob_cmd_append(&cmd, objs.items[i]);
    }
    nob_cmd_append(&cmd, "-Wl,--export-all-symbols",
                   "-Wl,--out-implib,build/win_obj/main.lib.a");
    nob_cmd_append(&cmd, WIN_LINK_FLAGS);
  } else {
    append_cc(&cmd);
    nob_cmd_append(&cmd, "-o", output);
    for (size_t i = 0; i < objs.count; ++i) {
      nob_cmd_append(&cmd, objs.items[i]);
    }
    if (do_release)
      nob_cmd_append(&cmd, "-O3");
#if defined(_WIN32)
    nob_cmd_append(&cmd, "-mwindows");
    nob_cmd_append(&cmd, "-Wl,--export-all-symbols",
                   "-Wl,--out-implib,build/main.lib.a");
#endif
    append_dep_ldflags(&cmd);
    append_san_flags(&cmd);
    nob_cmd_append(&cmd, LINK_FLAGS);
  }

  nob_log(NOB_INFO, "Linking %s", output);
  bool linked = nob_cmd_run(&cmd);
  nob_da_free(objs);
  if (!linked) {
    nob_log(NOB_ERROR, "Failed to link %s", output);
    return 1;
  }

  if (!build_features())
    return 1;

  if (!target_win32) {
#if !defined(_WIN32)
    Nob_Cmd chmod = {0};
    nob_cmd_append(&chmod, "chmod", "+x", output);
    nob_cmd_run(&chmod);
#endif
    stop_running_instance();
    if (do_run && !run_output(output))
      return 1;
    nob_log(NOB_INFO, "Built Successfully");
  } else {
    nob_log(NOB_INFO, "Done: %s (copy to a Windows machine and run)", output);
  }

  return 0;
}
