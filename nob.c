#define NOB_IMPLEMENTATION
#define NOB_EXPERIMENTAL_DELETE_OLD
#include "nob.h"
#include "include/core/types/color.h"
#include "include/config_defs.h"

#define SRC_FOLDER "src"
#define BUILD_FOLDER "build"

#define CC_COMMON_FLAGS                                                        \
  "-std=gnu17", "-DCOBJMACROS", "-I", "include", "-MMD", "-MP", "-Wall",       \
      "-Wextra", "-Wshadow", "-Wmissing-prototypes", "-Wstrict-prototypes",    \
      "-Wformat=2", "-Wundef", "-Wvla"

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
      "-Wall", "-Wextra", "-Wno-sign-compare", "-Wno-unused-parameter"

#define WIN_LINK_FLAGS                                                         \
  "-static", "-lcomctl32", "-lgdiplus", "-lgdi32", "-lshlwapi", "-lshell32",   \
      "-lwinmm", "-lole32", "-luuid", "-lmsimg32", "-ldwmapi", "-lpsapi",      \
      "-lcrypt32", "-lcomdlg32", "-ladvapi32", "-ldxva2", "-luser32",          \
      "-lwinhttp", "-Lbuild/winlibs", "-liphlpapi"

#define CROSS_CC "x86_64-w64-mingw32-gcc"

#define SDK_CC_FLAGS                                                           \
  "-std=gnu17", "-DCOBJMACROS", "-MMD", "-MP", "-Wall", "-Wextra",             \
      "-Wno-unused-parameter"
#define WIN_SDK_FLAGS                                                          \
  "-std=gnu17", "-DCOBJMACROS", "-DWINVER=0x0601", "-D_WIN32_WINNT=0x0601",    \
      "-I/usr/x86_64-w64-mingw32/include", "-MMD", "-MP", "-Wall", "-Wextra",  \
      "-Wno-sign-compare", "-Wno-unused-parameter"

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
static bool release = false;
static bool werror = false;
static const char *sdk_dir = BUILD_FOLDER "/sdk";
static const char *features_dir = BUILD_FOLDER "/features";
static const char *main_implib = BUILD_FOLDER "/main.lib.a";

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
      if (nob_sv_starts_with(path, nob_sv_from_cstr("features/")) ||
          nob_sv_starts_with(path, nob_sv_from_cstr("features\\"))) {
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
  const char *result = nob_temp_sprintf("%s/%s", features_dir, no_ext);
  free(no_ext);
  return result;
}

static const char *feature_so_path(const char *src) {
  return nob_temp_sprintf("%s%s", feature_base(src), feature_ext());
}

static const char *feature_dep_path(const char *src) {
  char *flat = nob_temp_strdup(src);
  for (char *p = flat; *p; p++)
    if (*p == '/' || *p == '\\')
      *p = '_';
  const char *result = nob_temp_sprintf("%s/%s.d", obj_dir, flat);
  return result;
}

static const char *release_features[] = {"control_center", "settings",
                                         "clipboard_history","window_management"};

static bool release_feature_wanted(const char *src) {
  const char *base = nob_path_name(src);
  for (size_t i = 0; i < sizeof(release_features) / sizeof(release_features[0]);
       i++) {
    size_t n = strlen(release_features[i]);
    if (strncmp(base, release_features[i], n) == 0 && base[n] == '.')
      return true;
  }
  return false;
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
    const char *start = p;
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
      if (release) {
        nob_cmd_append(&cmd, "-O3");
        nob_cmd_append(&cmd, "-mwindows");
      } else {
        nob_cmd_append(&cmd, "-ggdb");
      }
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

    const char *dep_file = nob_temp_sprintf("%.*s", (int)tok.count, tok.data);
    if (nob_needs_rebuild1(obj, dep_file) != 0)
      return true;
  }
  return false;
}

static bool stale(const char *src, const char *obj) {
  return stale_with_dep(src, obj, dep_path(src));
}

static const char *sdk_files[] = {
    "config.h",
    "config_defs.h",
    "config_values.h",
    "core/cc_registry.h",
    "core/checkbox.h",
    "core/clipboard_history.h",
    "core/context.h",
    "core/skapi.h",
    "core/skapi_input.h",
    "core/types.h",
    "core/types/color.h",
    "core/types/file.h",
    "core/types/keys.h",
    "core/types/prompt.h",
    "core/types/skapi_types.h",
    "core/types/window.h",
    "core/widget_registry.h",
    "core/window_manager.h",
    "helpers/helpers_owner.h",
    "helpers/helpers_misc.h",
    "helpers/helpers_render_thread.h",
    "helpers/helpers_text.h",
    "helpers/helpers_ui.h",
    "platform/draw.h",
    "platform/platform.h",
    "platform/widget.h",
};

static bool emit_sdk(const char *dst_sdk) {
  char dirs[5][512];
  snprintf(dirs[0], sizeof(dirs[0]), "%s", dst_sdk);
  snprintf(dirs[1], sizeof(dirs[1]), "%s/core", dst_sdk);
  snprintf(dirs[2], sizeof(dirs[2]), "%s/core/types", dst_sdk);
  snprintf(dirs[3], sizeof(dirs[3]), "%s/helpers", dst_sdk);
  snprintf(dirs[4], sizeof(dirs[4]), "%s/platform", dst_sdk);
  for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); ++i) {
    if (!nob_mkdir_if_not_exists(dirs[i]))
      return false;
  }
  bool ok = true;
  for (size_t i = 0; i < sizeof(sdk_files) / sizeof(sdk_files[0]); ++i) {
    char src[512], dst[512];
    snprintf(src, sizeof(src), "include/%s", sdk_files[i]);
    snprintf(dst, sizeof(dst), "%s/%s", dst_sdk, sdk_files[i]);
    if (!nob_file_exists(dst) || nob_needs_rebuild1(dst, src) != 0) {
      if (!nob_copy_file(src, dst))
        ok = false;
    }
  }
  return ok;
}

static void append_plugin_build_cmd(Nob_Cmd *cmd, const char *src,
                                    const char *out) {
  const char *so = out;
  if (target_win32) {
    nob_cmd_append(cmd, CROSS_CC, WIN_SDK_FLAGS, "-I", sdk_dir, "-shared", "-o",
                   so, src, main_implib, WIN_LINK_FLAGS);
    if (release)
      nob_cmd_append(cmd, "-O3");
    else
      nob_cmd_append(cmd, "-ggdb");
  } else {
    append_cc(cmd);
    nob_cc_flags(cmd);
    append_san_flags(cmd);
#if defined(_WIN32)
    nob_cmd_append(cmd, SDK_CC_FLAGS, "-I", sdk_dir, "-shared", "-o", so, src,
                   main_implib, "-MF", feature_dep_path(src), LINK_FLAGS);
#else
    nob_cmd_append(cmd, SDK_CC_FLAGS, "-I", sdk_dir, "-fPIC", "-shared", "-o",
                   so, src, "-MF", feature_dep_path(src));
    append_dep_cflags(cmd);
    nob_cmd_append(cmd, "-lX11");
#if defined(__APPLE__)
    nob_cmd_append(cmd, LINK_FLAGS);
#endif
#endif
    if (release)
      nob_cmd_append(cmd, "-O3");
    else
      nob_cmd_append(cmd, "-ggdb");
  }
}

static void append_feature_build_cmd(Nob_Cmd *cmd, const char *src) {
  append_plugin_build_cmd(cmd, src, feature_so_path(src));
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
    if (release && !release_feature_wanted(src))
      continue;
    const char *so = feature_so_path(src);
    bool need_rebuild = !nob_file_exists(so);
    if (!need_rebuild) {
      if (target_win32) {
        need_rebuild = nob_needs_rebuild1(so, src) != 0 ||
                       (nob_file_exists(main_implib) &&
                        nob_needs_rebuild1(so, main_implib) != 0);
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

static void emit_bool_toml(FILE *f, const char *name, bool v) {
  fprintf(f, "%s = %s\n", name, v ? "true" : "false");
}

static void emit_int_toml(FILE *f, const char *name, int v) {
  fprintf(f, "%s = %d\n", name, v);
}

static void emit_float_toml(FILE *f, const char *name, float v) {
  char tmp[64];
  snprintf(tmp, sizeof(tmp), "%g", (double)v);
  if (!strchr(tmp, '.') && !strchr(tmp, 'e') && !strchr(tmp, 'E') &&
      !strchr(tmp, 'n') && !strchr(tmp, 'N')) {
    size_t n = strlen(tmp);
    snprintf(tmp + n, sizeof(tmp) - n, ".0");
  }
  fprintf(f, "%s = %s\n", name, tmp);
}

static void emit_str_toml(FILE *f, const char *name, const char *v) {
  fprintf(f, "%s = \"%s\"\n", name, v ? v : "");
}

static void emit_skip_toml(FILE *f, const char *name, ...) {
  (void)f;
  (void)name;
}

static void emit_color_toml(FILE *f, const char *name, unsigned v) {
  unsigned r = v & 0xFFu, g = (v >> 8) & 0xFFu, b = (v >> 16) & 0xFFu;
  fprintf(f, "%s = \"#%02X%02X%02X\"\n", name, r, g, b);
}

static bool emit_release_config(const char *dst_path) {
  FILE *outf = fopen(dst_path, "wb");
  if (!outf) {
    nob_log(NOB_ERROR, "release config: cannot open %s for writing", dst_path);
    return false;
  }
  fprintf(outf, "# <skald-config-generated>\n");
#define EMIT_ROOT(type, name, def)                                             \
  _Generic((type){0},                                                          \
      bool: emit_bool_toml,                                                    \
      int: emit_int_toml,                                                      \
      float: emit_float_toml,                                                  \
      const char *: emit_str_toml,                                             \
      ColorRef: emit_skip_toml)(outf, #name, def);
  SKALD_CONFIG_VALUES(EMIT_ROOT)
#undef EMIT_ROOT
  fprintf(outf, "\n[color]\n");
#define EMIT_COLOR(type, name, def)                                            \
  _Generic((type){0},                                                          \
      bool: emit_skip_toml,                                                    \
      int: emit_skip_toml,                                                     \
      float: emit_skip_toml,                                                   \
      const char *: emit_skip_toml,                                            \
      ColorRef: emit_color_toml)(outf, #name + 6, def);
  SKALD_CONFIG_VALUES(EMIT_COLOR)
#undef EMIT_COLOR
  fprintf(outf, "# </skald-config-generated>\n");
  if (fclose(outf) != 0) {
    nob_log(NOB_ERROR, "release config: write failed for %s", dst_path);
    return false;
  }
  nob_log(NOB_INFO, "release: generated %s from include/config_defs.h",
          dst_path);
  return true;
}

static void release_finalize(void) {
  if (!release)
    return;
  bool win_out = target_win32;
#if defined(_WIN32)
  win_out = true;
#endif
  if (win_out) {
    if (nob_file_exists(main_implib) &&
        !nob_copy_file(main_implib, "release/main.lib.a"))
      nob_log(NOB_WARNING, "release: could not copy %s to release/main.lib.a",
              main_implib);
  }
  nob_log(NOB_INFO,
          "Release bundle ready in release/ (run from that directory)");
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
      if (release)
        nob_cmd_append(&cmd, "-O3");
      else
        nob_cmd_append(&cmd, "-ggdb");
    } else {
      append_cc(&cmd);
      nob_cc_flags(&cmd);
      append_wayland_session_flags(&cmd);
      nob_cmd_append(&cmd, CC_FLAGS, "-c", to_build.items[i], "-o",
                     obj_path(to_build.items[i]));
      if (werror)
        nob_cmd_append(&cmd, "-Werror", "-Wno-error=unused-parameter",
                       "-Wno-error=undef");
      if (release)
        nob_cmd_append(&cmd, "-O3");
      else
        nob_cmd_append(&cmd, "-ggdb");
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

typedef struct {
  const char *path;
  int line;
} CheckLoc;

typedef struct {
  int files_scanned;
  int skapi_violations;
  int fn_violations;
  int struct_violations;
} CheckState;

static CheckState check_state;
static char decl_buf[8192];
static size_t decl_len;
static bool decl_overflow;
static int decl_start_line;
static int brace_depth;

static char **g_declared = NULL;
static size_t g_declared_count = 0;
static size_t g_declared_cap = 0;

typedef struct {
  char path[1024];
  int line;
  char name[256];
  const char *prefix;
} PendingFn;
static PendingFn pending_fns[512];
static size_t pending_fn_count = 0;

static void decl_remember(const char *name) {
  for (size_t i = 0; i < g_declared_count; i++)
    if (strcmp(g_declared[i], name) == 0)
      return;
  if (g_declared_count >= g_declared_cap) {
    size_t ncap = g_declared_cap ? g_declared_cap * 2 : 256;
    char **grown = (char **)realloc(g_declared, ncap * sizeof(*grown));
    if (!grown)
      return;
    g_declared = grown;
    g_declared_cap = ncap;
  }
  size_t n = strlen(name) + 1;
  char *dup = (char *)malloc(n);
  if (!dup)
    return;
  memcpy(dup, name, n);
  g_declared[g_declared_count++] = dup;
}

static bool decl_known(const char *name) {
  for (size_t i = 0; i < g_declared_count; i++)
    if (strcmp(g_declared[i], name) == 0)
      return true;
  return false;
}

static void check_norm_path(const char *src, char *dst, size_t cap) {
  size_t i = 0;
  for (; src[i] && i + 1 < cap; i++)
    dst[i] = (src[i] == '\\') ? '/' : src[i];
  dst[i] = '\0';
}

static bool check_starts_with(const char *s, const char *prefix) {
  size_t i = 0;
  while (prefix[i] && s[i] == prefix[i])
    i++;
  return prefix[i] == '\0';
}

static bool check_ends_with(const char *s, const char *suffix) {
  size_t ns = strlen(s), nf = strlen(suffix);
  if (nf > ns)
    return false;
  return strcmp(s + ns - nf, suffix) == 0;
}

static const char *check_fn_prefix(const char *path_norm) {
  if (check_ends_with(path_norm, "skapi_x11.c") ||
      check_ends_with(path_norm, "skapi_win32.c"))
    return NULL;
  if (strstr(path_norm, "src/platform/"))
    return "Platform_";
  if (strstr(path_norm, "include/platform/x11/"))
    return "x11_";
  if (strstr(path_norm, "include/platform/win32/"))
    return "win32_";
  if (strstr(path_norm, "include/platform/"))
    return "Platform_";
  if (strstr(path_norm, "src/helpers/x11/") ||
      strstr(path_norm, "include/helpers/x11/"))
    return "x11_";
  if (strstr(path_norm, "src/helpers/win32/") ||
      strstr(path_norm, "include/helpers/win32/"))
    return "win32_";
  return NULL;
}

static void check_violation(const char *path, int line, const char *message) {
  fprintf(stderr, "%s:%d: %s\n", path, line, message);
}

typedef enum {
  CHK_IDENT,
  CHK_STAR,
  CHK_LPAREN,
  CHK_RPAREN,
  CHK_LBRACE,
  CHK_RBRACE,
  CHK_SEMI,
  CHK_EQUAL,
  CHK_COMMA,
  CHK_OTHER
} ChkTokKind;

typedef struct {
  size_t beg, end;
  ChkTokKind kind;
} ChkTok;

static ChkTok chk_toks[1024];
static size_t chk_tok_count;

static void chk_tok_add(size_t beg, size_t end, ChkTokKind kind) {
  if (chk_tok_count < sizeof(chk_toks) / sizeof(chk_toks[0])) {
    chk_toks[chk_tok_count].beg = beg;
    chk_toks[chk_tok_count].end = end;
    chk_toks[chk_tok_count].kind = kind;
    chk_tok_count++;
  }
}

static void chk_tokenize(const char *buf, size_t len) {
  chk_tok_count = 0;
  size_t j = 0;
  while (j < len) {
    char c = buf[j];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      j++;
      continue;
    }
    if (isalpha((unsigned char)c) || c == '_') {
      size_t beg = j;
      while (j < len && (isalpha((unsigned char)buf[j]) ||
                         isdigit((unsigned char)buf[j]) || buf[j] == '_'))
        j++;
      chk_tok_add(beg, j, CHK_IDENT);
      continue;
    }
    switch (c) {
    case '*':
      chk_tok_add(j, j + 1, CHK_STAR);
      break;
    case '(':
      chk_tok_add(j, j + 1, CHK_LPAREN);
      break;
    case ')':
      chk_tok_add(j, j + 1, CHK_RPAREN);
      break;
    case '{':
      chk_tok_add(j, j + 1, CHK_LBRACE);
      break;
    case '}':
      chk_tok_add(j, j + 1, CHK_RBRACE);
      break;
    case ';':
      chk_tok_add(j, j + 1, CHK_SEMI);
      break;
    case '=':
      chk_tok_add(j, j + 1, CHK_EQUAL);
      break;
    case ',':
      chk_tok_add(j, j + 1, CHK_COMMA);
      break;
    default:
      chk_tok_add(j, j + 1, CHK_OTHER);
      break;
    }
    j++;
  }
}

static bool chk_tok_is(size_t i, const char *buf, const char *name) {
  if (chk_toks[i].kind != CHK_IDENT)
    return false;
  size_t n = strlen(name);
  return chk_toks[i].end - chk_toks[i].beg == n &&
         memcmp(buf + chk_toks[i].beg, name, n) == 0;
}

static bool chk_layer_exempt(const char *path_norm) {
  const char *base = strrchr(path_norm, '/');
  base = base ? base + 1 : path_norm;
  size_t n = strlen(base);
  char low[6];
  if (n >= 5) {
    size_t k = 0;
    for (; k < 5; k++)
      low[k] = (char)tolower((unsigned char)base[k]);
    low[5] = '\0';
    if (strcmp(low, "skapi") == 0)
      return true;
  }
  if (strstr(path_norm, "src/platform/") ||
      strstr(path_norm, "include/platform/"))
    return true;
  if (check_ends_with(path_norm, "core/types/skapi_types.h"))
    return true;
  return false;
}

static bool chk_ci_sk_func(const char *buf, size_t beg, size_t end) {
  size_t n = end - beg;
  if (n <= 3)
    return false;
  return tolower((unsigned char)buf[beg]) == 's' &&
         tolower((unsigned char)buf[beg + 1]) == 'k' && buf[beg + 2] == '_';
}

static bool chk_ci_platform_func(const char *buf, size_t beg, size_t end) {
  size_t n = end - beg;
  if (n <= 9)
    return false;
  return memcmp(buf + beg, "Platform_", 9) == 0;
}

static bool chk_ci_platform_layer(const char *path_norm) {
  return strstr(path_norm, "src/platform/") != NULL ||
         strstr(path_norm, "include/platform/") != NULL;
}

static bool chk_ci_sk_ident(const char *buf, size_t beg, size_t end) {
  size_t n = end - beg;
  if (n < 4)
    return false;
  if (isupper((unsigned char)buf[beg]) && buf[beg + 1] == 'k' &&
      isupper((unsigned char)buf[beg + 2]))
    return true;
  unsigned char a = (unsigned char)tolower((unsigned char)buf[beg]);
  unsigned char b = (unsigned char)tolower((unsigned char)buf[beg + 1]);
  return a == 's' && b == 'k' && buf[beg + 2] == '_' &&
         isalpha((unsigned char)buf[beg + 3]);
}

static bool chk_ci_is(const ChkTok *t, const char *buf, const char *name) {
  size_t n = strlen(name);
  if (t->end - t->beg != n)
    return false;
  for (size_t k = 0; k < n; k++)
    if (tolower((unsigned char)buf[t->beg + k]) !=
        tolower((unsigned char)name[k]))
      return false;
  return true;
}

static void chk_platform_func_scan(const char *path, int lineno,
                                   const char *buf, size_t len) {
  chk_tokenize(buf, len);
  if (chk_tok_count == 0)
    return;
  int bdepth = 0;
  int pdepth = 0;
  for (size_t i = 0; i < chk_tok_count; i++) {
    ChkTokKind k = chk_toks[i].kind;
    if (k == CHK_LBRACE) {
      bdepth++;
      continue;
    }
    if (k == CHK_RBRACE) {
      if (bdepth)
        bdepth--;
      continue;
    }
    if (bdepth != 0)
      continue;
    if (k == CHK_LPAREN) {
      pdepth++;
      continue;
    }
    if (k == CHK_RPAREN) {
      if (pdepth)
        pdepth--;
      continue;
    }
    if (pdepth != 0)
      continue;
    if (k != CHK_IDENT)
      continue;
    if (i + 1 >= chk_tok_count || chk_toks[i + 1].kind != CHK_LPAREN)
      continue;
    if (i + 3 < chk_tok_count && chk_toks[i + 2].kind == CHK_STAR &&
        chk_toks[i + 3].kind == CHK_IDENT)
      continue;
    if (chk_ci_is(&chk_toks[i], buf, "if") ||
        chk_ci_is(&chk_toks[i], buf, "while") ||
        chk_ci_is(&chk_toks[i], buf, "for") ||
        chk_ci_is(&chk_toks[i], buf, "switch") ||
        chk_ci_is(&chk_toks[i], buf, "sizeof") ||
        chk_ci_is(&chk_toks[i], buf, "return"))
      continue;
    if (!chk_ci_platform_func(buf, chk_toks[i].beg, chk_toks[i].end))
      continue;
    bool eq_before = false;
    for (size_t j = 0; j < i; j++)
      if (chk_toks[j].kind == CHK_EQUAL) {
        eq_before = true;
        break;
      }
    if (eq_before)
      continue;
    check_state.fn_violations++;
    char name[256];
    size_t nl = chk_toks[i].end - chk_toks[i].beg;
    memcpy(name, buf + chk_toks[i].beg, nl);
    name[nl] = '\0';
    char msg[512];
    snprintf(msg, sizeof(msg),
             "function '%s' belongs to the platform layer (src/platform/, "
             "include/platform/)",
             name);
    check_violation(path, lineno, msg);
  }
}

static void chk_sk_ident_scan(const char *path, int lineno, const char *buf,
                              size_t len, bool layer_exempt) {
  if (layer_exempt)
    return;
  if (!len)
    return;
  chk_tokenize(buf, len);
  if (chk_tok_count == 0)
    return;
  for (size_t i = 0; i < chk_tok_count; i++) {
    ChkTokKind k = chk_toks[i].kind;
    if (k != CHK_IDENT)
      continue;
    if (!chk_ci_sk_ident(buf, chk_toks[i].beg, chk_toks[i].end))
      continue;
    char name[256];
    size_t nl = chk_toks[i].end - chk_toks[i].beg;
    if (nl >= sizeof(name))
      nl = sizeof(name) - 1;
    memcpy(name, buf + chk_toks[i].beg, nl);
    name[nl] = '\0';
    check_state.struct_violations++;
    char msg[512];
    snprintf(msg, sizeof(msg),
             "identifier '%s' squats the Sk API namespace outside the skapi "
             "layer",
             name);
    check_violation(path, lineno, msg);
  }
}

static void chk_finish_decl(const char *path, int lineno, bool is_h) {
  const char *buf = decl_buf;
  size_t len = decl_len;
  if (chk_tok_count == 0)
    ;
  chk_tokenize(buf, len);
  if (chk_tok_count == 0)
    return;
  for (size_t i = 0; i < chk_tok_count; i++)
    if (chk_tok_is(i, buf, "typedef") || chk_tok_is(i, buf, "attribute") ||
        chk_tok_is(i, buf, "declspec") || chk_tok_is(i, buf, "asm"))
      return;
  int candidate = -1;
  size_t pdepth = 0;
  for (size_t i = 0; i + 1 < chk_tok_count; i++) {
    ChkTokKind k = chk_toks[i].kind;
    if (k == CHK_LPAREN) {
      pdepth++;
      continue;
    }
    if (k == CHK_RPAREN) {
      if (pdepth)
        pdepth--;
      continue;
    }
    if (pdepth != 0)
      continue;
    if (k != CHK_IDENT)
      continue;
    if (chk_toks[i + 1].kind != CHK_LPAREN)
      continue;
    if (i + 3 < chk_tok_count && chk_toks[i + 2].kind == CHK_STAR &&
        chk_toks[i + 3].kind == CHK_IDENT)
      continue;
    if (chk_tok_is(i, buf, "if") || chk_tok_is(i, buf, "while") ||
        chk_tok_is(i, buf, "for") || chk_tok_is(i, buf, "switch") ||
        chk_tok_is(i, buf, "sizeof") || chk_tok_is(i, buf, "return"))
      continue;
    candidate = (int)i;
  }
  if (candidate < 0)
    return;
  for (size_t i = 0; i < (size_t)candidate; i++)
    if (chk_toks[i].kind == CHK_EQUAL)
      return;
  if (!is_h) {
    for (size_t i = 0; i < (size_t)candidate; i++)
      if (chk_tok_is(i, buf, "static"))
        return;
  }
  size_t beg = chk_toks[candidate].beg, end = chk_toks[candidate].end;
  size_t name_len = end - beg;
  char name[256];
  if (name_len >= sizeof(name))
    name_len = sizeof(name) - 1;
  memcpy(name, buf + beg, name_len);
  name[name_len] = '\0';

  char norm[1024];
  check_norm_path(path, norm, sizeof(norm));
  const char *prefix = check_fn_prefix(norm);
  if (is_h)
    decl_remember(name);
  if (!prefix)
    return;
  if (strncmp(name, prefix, strlen(prefix)) == 0)
    return;
  if (!is_h &&
      pending_fn_count < sizeof(pending_fns) / sizeof(pending_fns[0])) {
    PendingFn *p = &pending_fns[pending_fn_count++];
    snprintf(p->path, sizeof(p->path), "%s", path);
    p->line = lineno;
    snprintf(p->name, sizeof(p->name), "%s", name);
    p->prefix = prefix;
    return;
  }
  check_state.fn_violations++;
  char msg[512];
  snprintf(msg, sizeof(msg),
           "function '%s' does not start with '%s' in this directory", name,
           prefix);
  check_violation(path, lineno, msg);
}

static void chk_include_check(const char *path, char *line, int lineno,
                              bool exempt) {
  if (exempt)
    return;
  size_t j = 0;
  while (line[j] == ' ' || line[j] == '\t')
    j++;
  if (line[j] != '#')
    return;
  j++;
  while (line[j] == ' ' || line[j] == '\t')
    j++;
  if (strncmp(line + j, "include", 7) != 0)
    return;
  j += 7;
  if (line[j] && line[j] != ' ' && line[j] != '\t')
    return;
  while (line[j] == ' ' || line[j] == '\t')
    j++;
  char open = line[j];
  char close = (open == '<') ? '>' : (open == '"' ? '"' : 0);
  if (!close)
    return;
  j++;
  size_t beg = j;
  while (line[j] && line[j] != close)
    j++;
  if (line[j] != close)
    return;
  const char *file = line + beg;
  size_t flen = (size_t)(line + j - file);
  const char *slash = NULL;
  for (const char *p = file; p < file + flen; p++)
    if (*p == '/')
      slash = p;
  const char *base = slash ? slash + 1 : file;
  size_t blen = (size_t)(file + flen - base);
  char name[256];
  if (blen >= sizeof(name))
    blen = sizeof(name) - 1;
  memcpy(name, base, blen);
  name[blen] = '\0';

  if (strcmp(name, "skapi.h") == 0 || strcmp(name, "skapi_types.h") == 0 ||
      strcmp(name, "skapi_platform.h") == 0) {
    check_state.skapi_violations++;
    char msg[512];
    snprintf(msg, sizeof(msg),
             "'#include \"%s\"' is only allowed in features/ and tests/", name);
    check_violation(path, lineno, msg);
  }
}

static bool check_walk(Nob_Walk_Entry entry) {
  if (entry.type != NOB_FILE_REGULAR)
    return true;
  const char *path = entry.path;
  brace_depth = 0;
  decl_len = 0;
  decl_overflow = false;
  decl_start_line = 0;
  char norm[1024];
  check_norm_path(path, norm, sizeof(norm));
  if (!check_ends_with(norm, ".c") && !check_ends_with(norm, ".h"))
    return true;
  bool exempt =
      check_starts_with(norm, "features/") || check_starts_with(norm, "tests/");
  bool sk_layer_exempt = chk_layer_exempt(norm);
  check_state.files_scanned++;

  Nob_String_Builder sb = {0};
  if (!nob_read_entire_file(path, &sb)) {
    fprintf(stderr, "%s: failed to read file\n", path);
    return false;
  }

  int lex = 0;
  int lineno = 1;
  size_t i = 0;
  while (i < sb.count) {
    size_t start = i;
    while (i < sb.count && sb.items[i] != '\n')
      i++;
    size_t len = i - start;
    if (i < sb.count)
      i++;

    char work[4096];
    char out[4096];
    size_t n = len < sizeof(work) ? len : sizeof(work);
    memcpy(work, sb.items + start, n);
    work[n] = '\0';

    size_t trimmed = 0;
    while (work[trimmed] == ' ' || work[trimmed] == '\t')
      trimmed++;
    bool preproc = lex != 1 && work[trimmed] == '#';

    size_t oi = 0, j = 0;
    while (work[j]) {
      unsigned char c = (unsigned char)work[j];
      if (lex == 1) {
        if (c == '*' && work[j + 1] == '/') {
          if (oi + 1 < sizeof(out)) {
            out[oi++] = ' ';
            out[oi++] = ' ';
          }
          lex = 0;
          j += 2;
          continue;
        }
        if (oi < sizeof(out) - 1)
          out[oi++] = ' ';
        j++;
        continue;
      }
      if (preproc) {
        if (c == '/' && work[j + 1] == '*') {
          lex = 1;
          if (oi + 1 < sizeof(out)) {
            out[oi++] = ' ';
            out[oi++] = ' ';
          }
          j += 2;
          continue;
        }
        if (oi < sizeof(out) - 1)
          out[oi++] = (char)c;
        j++;
        continue;
      }
      if (c == '"') {
        lex = 2;
        j++;
        while (work[j]) {
          unsigned char sc = (unsigned char)work[j];
          if (sc == '\\') {
            j += 2;
            continue;
          }
          j++;
          if (sc == '"') {
            lex = 0;
            break;
          }
        }
        if (oi < sizeof(out) - 1)
          out[oi++] = ' ';
        continue;
      }
      if (c == '\'') {
        lex = 3;
        j++;
        while (work[j]) {
          unsigned char sc = (unsigned char)work[j];
          if (sc == '\\') {
            j += 2;
            continue;
          }
          j++;
          if (sc == '\'') {
            lex = 0;
            break;
          }
        }
        if (oi < sizeof(out) - 1)
          out[oi++] = ' ';
        continue;
      }
      if (c == '/' && work[j + 1]) {
        char c2 = work[j + 1];
        if (c2 == '*') {
          lex = 1;
          if (oi + 1 < sizeof(out)) {
            out[oi++] = ' ';
            out[oi++] = ' ';
          }
          j += 2;
          continue;
        }
        if (c2 == '/') {
          while (work[j]) {
            if (oi < sizeof(out) - 1)
              out[oi++] = ' ';
            j++;
          }
          continue;
        }
      }
      if (oi < sizeof(out) - 1)
        out[oi++] = (char)c;
      j++;
    }
    out[oi] = '\0';

    const char *base = strrchr(norm, '/');
    base = base ? base + 1 : norm;
    bool skapi_impl = check_starts_with(base, "skapi");
    chk_include_check(path, out, lineno, exempt || skapi_impl);

    if (!exempt && !sk_layer_exempt)
      chk_sk_ident_scan(path, lineno, out, strlen(out), sk_layer_exempt);

    if (preproc) {
      if (!exempt && !sk_layer_exempt)
        chk_sk_ident_scan(path, lineno, out, strlen(out), sk_layer_exempt);
      lineno++;
      continue;
    }

    if (!exempt) {
      size_t k = 0;
      while (out[k]) {
        char c = out[k];
        if (brace_depth == 0) {
          if (decl_len == 0 && c != ' ' && c != '\t' && c != '\0')
            decl_start_line = lineno;
          if (!decl_overflow) {
            if (decl_len + 2 < sizeof(decl_buf)) {
              decl_buf[decl_len] = c;
              decl_len++;
            } else {
              decl_overflow = true;
            }
          }
          if (c == '{') {
            decl_buf[decl_len] = '\0';
            chk_finish_decl(path, decl_start_line ? decl_start_line : lineno,
                            check_ends_with(norm, ".h"));
            if (!chk_ci_platform_layer(norm))
              chk_platform_func_scan(path,
                                     decl_start_line ? decl_start_line : lineno,
                                     decl_buf, decl_len);
            decl_len = 0;
            decl_overflow = false;
            decl_start_line = 0;
            brace_depth++;
          } else if (c == ';') {
            decl_buf[decl_len] = '\0';
            chk_finish_decl(path, decl_start_line ? decl_start_line : lineno,
                            check_ends_with(norm, ".h"));
            if (!chk_ci_platform_layer(norm))
              chk_platform_func_scan(path,
                                     decl_start_line ? decl_start_line : lineno,
                                     decl_buf, decl_len);
            decl_len = 0;
            decl_overflow = false;
            decl_start_line = 0;
          } else if (c == '}') {
            brace_depth--;
            decl_len = 0;
            decl_overflow = false;
            decl_start_line = 0;
          }
        } else {
          if (c == '{')
            brace_depth++;
          else if (c == '}')
            brace_depth--;
        }
        k++;
      }
    }
    lineno++;
  }
  nob_da_free(sb);
  return true;
}

static bool check_layering(void) {
  memset(&check_state, 0, sizeof(check_state));
  decl_len = 0;
  decl_overflow = false;
  decl_start_line = 0;
  pending_fn_count = 0;
  if (!nob_walk_dir(SRC_FOLDER, check_walk))
    return false;
  if (!nob_walk_dir("include", check_walk))
    return false;
  if (!nob_walk_dir("features", check_walk))
    return false;
  if (!nob_walk_dir("tests", check_walk))
    return false;
  for (size_t i = 0; i < pending_fn_count; i++) {
    const PendingFn *p = &pending_fns[i];
    if (decl_known(p->name))
      continue;
    check_state.fn_violations++;
    char msg[512];
    snprintf(msg, sizeof(msg),
             "function '%s' does not start with '%s' in this directory and is "
             "not declared in any header",
             p->name, p->prefix);
    check_violation(p->path, p->line, msg);
  }
  printf("check: scanned %d files, %d skapi violation(s), %d function-name "
         "violation(s), %d struct-name violation(s)\n",
         check_state.files_scanned, check_state.skapi_violations,
         check_state.fn_violations, check_state.struct_violations);
  return check_state.skapi_violations + check_state.fn_violations +
             check_state.struct_violations ==
         0;
}

int main(int argc, char **argv) {
  NOB_GO_REBUILD_URSELF_PLUS(argc, argv, "nob.h", "include/config_defs.h");

  int do_run = 0;
  int do_test = 0;
  int do_check = 0;
  int do_all = 0;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      printf("Usage: %s [-a|--all] [-w|--win32] [-r|--run] [-R|--release] "
             "[-t|--test] [-A|--asan] [-c|--check] [-W|--werror]\n",
             argv[0]);
      printf("  -a, --all      Run nob, nob -t, nob -w and nob -c\n");
      printf("  -w, --win32    Cross compile for Windows using MinGW\n");
      printf("  -W, --werror   Build the main app with warnings as errors\n");
      printf("  -r, --run      Run skald after build\n");
      printf(
          "  -R, --release  Build a self-contained release/ bundle: "
          "skald[.exe],\n"
          "                 sdk/, config/config.toml (defaults),\n"
          "                 features/ with\n"
          "                 control_center, settings and clipboard_history,\n"
          "                 resources/ (png + ico); Windows also emits "
          "main.lib.a;\n"
          "                 no compiler objects\n");
      printf("  -t, --test     Run test suite\n");
      printf(
          "  -A, --asan     Build with clang -fsanitize=address,undefined\n");
      printf(
          "  -c, --check    Check for skapi includes outside features/tests,\n"
          "                 function naming in platform/ and helpers/ (.c and "
          ".h;\n"
          "                 .c externs must match the directory prefix or be\n"
          "                 declared in a header; statics are exempt),\n"
          "                 and sk_ / Sk namesquatting outside the skapi "
          "layer\n"
          "                 (case insensitive)\n");
      printf("  -h, --help     Show this help\n");
      return 0;
    }
    if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--win32") == 0)
      target_win32 = true;
    if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--run") == 0)
      do_run = 1;
    if (strcmp(argv[i], "-R") == 0 || strcmp(argv[i], "--release") == 0)
      release = true;
    if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--test") == 0)
      do_test = 1;
    if (strcmp(argv[i], "-W") == 0 || strcmp(argv[i], "--werror") == 0)
      werror = true;
    if (strcmp(argv[i], "-A") == 0 || strcmp(argv[i], "--asan") == 0)
      sanitize = true;
    if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--check") == 0)
      do_check = 1;
    if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0)
      do_all = 1;
  }

  if (do_all) {
    const char *steps[] = {NULL, "-t", "-w", "-c"};
    for (size_t st = 0; st < sizeof(steps) / sizeof(steps[0]); st++) {
      Nob_Cmd cmd = {0};
      if (steps[st])
        nob_cmd_append(&cmd, argv[0], steps[st]);
      else
        nob_cmd_append(&cmd, argv[0]);
      if (!nob_cmd_run(&cmd)) {
        nob_log(NOB_ERROR, "--all: step %s failed",
                steps[st] ? steps[st] : "(build)");
        return 1;
      }
    }
    return 0;
  }

  if (do_check) {
    if (!check_layering())
      return 1;
    return 0;
  }

  if (release) {
    obj_dir = BUILD_FOLDER "/release_obj";
    sdk_dir = "release/sdk";
    features_dir = "release/build/features";
    main_implib = "release/build/main.lib.a";
    if (!nob_mkdir_if_not_exists("release"))
      return 1;
    if (!nob_mkdir_if_not_exists("release/build"))
      return 1;
    if (!nob_mkdir_if_not_exists("release/build/features"))
      return 1;
    if (!nob_mkdir_if_not_exists("release/config"))
      return 1;
    if (!nob_mkdir_if_not_exists("release/features"))
      return 1;
    if (!nob_copy_directory_recursively("features/control_center",
                                        "release/features/control_center") ||
        !nob_copy_directory_recursively("features/settings",
                                        "release/features/settings") ||
        !nob_copy_directory_recursively("features/clipboard_history",
                                        "release/features/clipboard_history") ||
        !nob_copy_directory_recursively("resources", "release/resources"))
      return 1;
    if (!emit_release_config("release/config/config.toml")) {
      nob_log(NOB_ERROR,
              "release: failed to generate release/config/config.toml");
      return 1;
    }
    nob_log(NOB_INFO,
            "release/: config, features, resources staged (control_center, "
            "settings, clipboard_history)");
  }

  if (sanitize) {
    pick_sanitize_cc();
    obj_dir = BUILD_FOLDER "/san_obj";
  }

  if (target_win32) {
    obj_dir = BUILD_FOLDER "/win_obj";
    if (!release)
      main_implib = BUILD_FOLDER "/win_obj/main.lib.a";
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
  if (!nob_walk_dir("features", collect_source)) {
    nob_log(NOB_ERROR, "Failed to walk features");
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

  if (!nob_mkdir_if_not_exists(features_dir))
    return 1;

  if (!emit_sdk(sdk_dir)) {
    nob_log(NOB_ERROR, "Failed to emit %s", sdk_dir);
    return 1;
  }
  if (release && !emit_sdk("release/build/sdk")) {
    nob_log(NOB_ERROR, "Failed to emit release/build/sdk");
    return 1;
  }

  bool any_compiled = false;
  if (!compile_all(&any_compiled))
    return 1;

  if (do_test)
    return run_tests() ? 0 : 1;

  const char *output = NULL;
  if (target_win32) {
    output = release ? "release/skald.exe" : BUILD_FOLDER "/skald_win.exe";
  } else {
#if defined(_WIN32)
    output = sanitize  ? "build/debug/skald_san.exe"
             : release ? "release/skald.exe"
                       : "build/debug/skald.exe";
#else
    output = release ? "release/skald" : BUILD_FOLDER "/skald";
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
    if (!release) {
      const char *other = "build/skald.exe";
      if (nob_file_exists(other) &&
          nob_needs_rebuild(other, (const char **)objs.items, objs.count) !=
              0) {
        needs_link = true;
      }
    }
#endif
  }

  if (!needs_link) {
    nob_log(NOB_INFO, "No changes, skipping link");
    nob_da_free(objs);
    if (!build_features())
      return 1;
    release_finalize();
    if (do_run && !run_output(output))
      return 1;
    return 0;
  }

  Nob_Cmd cmd = {0};
  if (target_win32) {
    nob_cmd_append(&cmd, CROSS_CC, "-o", output);
    if (release)
      nob_cmd_append(&cmd, "-mwindows");
    for (size_t i = 0; i < objs.count; ++i) {
      nob_cmd_append(&cmd, objs.items[i]);
    }
    nob_cmd_append(&cmd, "-Wl,--export-all-symbols",
                   nob_temp_sprintf("-Wl,--out-implib,%s", main_implib));
    nob_cmd_append(&cmd, WIN_LINK_FLAGS);
  } else {
    append_cc(&cmd);
    nob_cmd_append(&cmd, "-o", output);
    for (size_t i = 0; i < objs.count; ++i) {
      nob_cmd_append(&cmd, objs.items[i]);
    }
    if (release)
      nob_cmd_append(&cmd, "-O3");
#if defined(_WIN32)
    if (release)
      nob_cmd_append(&cmd, "-mwindows");
    nob_cmd_append(&cmd, "-Wl,--export-all-symbols",
                   nob_temp_sprintf("-Wl,--out-implib,%s", main_implib));
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
    release_finalize();
    if (!release)
      stop_running_instance();
    if (do_run && !run_output(output))
      return 1;
    nob_log(NOB_INFO, "Built Successfully");
  } else {
    release_finalize();
    nob_log(NOB_INFO, "Done: %s (copy to a Windows machine and run)", output);
  }

  return 0;
}
