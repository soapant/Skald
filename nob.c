#define NOB_IMPLEMENTATION
#include "nob.h"

#define SRC_FOLDER "src"
#define INCLUDE_FOLDER "include"
#define BUILD_FOLDER "build"
#define OBJ_FOLDER BUILD_FOLDER "/obj"
#define CONSOLE_OUTPUT BUILD_FOLDER "/main.exe"
#define ACTUAL_OUTPUT BUILD_FOLDER "/myprogram.exe"

#define CC_FLAGS "-std=gnu17", "-DCOBJMACROS", "-I", INCLUDE_FOLDER, "-Wno-multichar", "-Wunused-parameter"
#define LINK_FLAGS "-lcomctl32", "-lgdiplus", "-lgdi32", "-lshlwapi", "-lshell32", \
                   "-lwinmm", "-lole32", "-luuid", "-lmsimg32", "-ldbghelp", "-ldwmapi", \
                   "-lpsapi", "-lcrypt32", "-lcomdlg32", "-ladvapi32"

Nob_File_Paths sources = {0};
Nob_File_Paths headers = {0};

static bool ends_with(const char* str, const char* suffix) {
  size_t str_len = strlen(str);
  size_t suffix_len = strlen(suffix);
  if (suffix_len > str_len) return false;
  return strcmp(str + str_len - suffix_len, suffix) == 0;
}

static bool collect(Nob_Walk_Entry entry) {
  if (entry.type != NOB_FILE_REGULAR) return true;
  if (ends_with(entry.path, ".c")) {
    nob_da_append(&sources, strdup(entry.path));
  } else if (ends_with(entry.path, ".h")) {
    nob_da_append(&headers, strdup(entry.path));
  }
  return true;
}

static const char* obj_path(const char* src) {
  return nob_temp_sprintf(OBJ_FOLDER "/%s.o", nob_path_name(src));
}

static bool obj_is_stale(const char* src, const char* obj) {
  if (!nob_file_exists(obj)) return true;
  if (nob_needs_rebuild1(obj, src)) return true;
  for (size_t i = 0; i < headers.count; ++i) {
    if (nob_needs_rebuild1(obj, headers.items[i])) return true;
  }
  return false;
}

static bool compile_all(bool* any_compiled) {
  if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return false;
  if (!nob_mkdir_if_not_exists(OBJ_FOLDER)) return false;

  *any_compiled = false;
  for (size_t i = 0; i < sources.count; ++i) {
    const char* src = sources.items[i];
    const char* obj = obj_path(src);
    if (!obj_is_stale(src, obj)) {
      continue;
    }
    Nob_Cmd cmd = {0};
    nob_cc(&cmd);
    nob_cc_flags(&cmd);
    nob_cmd_append(&cmd, CC_FLAGS, "-c", src, "-o", obj);
    if (!nob_cmd_run_sync_and_reset(&cmd)) {
      nob_log(NOB_ERROR, "Failed to compile %s", src);
      return false;
    }
    *any_compiled = true;
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
  if (!nob_cmd_run_sync_and_reset(&cmd)) {
    nob_log(NOB_ERROR, "Failed to link %s", output);
    return false;
  }
  return true;
}

static bool clean(void) {
  for (size_t i = 0; i < sources.count; ++i) {
    nob_delete_file(obj_path(sources.items[i]));
  }
  nob_delete_file(CONSOLE_OUTPUT);
  nob_delete_file(ACTUAL_OUTPUT);
  nob_log(NOB_INFO, "Cleaned " BUILD_FOLDER);
  return true;
}

int main(int argc, char** argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  bool windowed = false;
  bool do_run = false;
  bool do_clean = false;

  const char* type = argc > 1 ? argv[1] : "-console";
  const char* action = argc > 2 ? argv[2] : NULL;

  if (strcmp(type, "-console") == 0) {
    windowed = false;
  } else if (strcmp(type, "-actual") == 0) {
    windowed = true;
  } else if (strcmp(type, "clean") == 0) {
    do_clean = true;
  } else {
    nob_log(NOB_ERROR, "Unknown build type: %s (expected -console, -actual or clean)", type);
    return 1;
  }

  if (action != NULL) {
    if (strcmp(action, "run") == 0) {
      do_run = true;
    } else {
      nob_log(NOB_ERROR, "Unknown action: %s (expected run)", action);
      return 1;
    }
  }
  if (argc > 3) {
    nob_log(NOB_ERROR, "Too many arguments");
    return 1;
  }
  if (do_clean && do_run) {
    nob_log(NOB_ERROR, "Cannot run a clean build");
    return 1;
  }

  if (!nob_walk_dir(SRC_FOLDER, collect)) {
    nob_log(NOB_ERROR, "Failed to walk " SRC_FOLDER);
    return 1;
  }
  if (!nob_walk_dir(INCLUDE_FOLDER, collect)) {
    nob_log(NOB_ERROR, "Failed to walk " INCLUDE_FOLDER);
    return 1;
  }

  if (do_clean) return clean() ? 0 : 1;

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
