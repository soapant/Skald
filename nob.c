#define NOB_IMPLEMENTATION
#include "nob.h"
#ifdef _WIN32
#include <tlhelp32.h>
#endif

#define SRC_FOLDER "src"
#define BUILD_FOLDER "build"
#define OBJ_FOLDER BUILD_FOLDER "/obj"
#define CONSOLE_OUTPUT BUILD_FOLDER "/main.exe"
#define ACTUAL_OUTPUT BUILD_FOLDER "/myprogram.exe"

#define CC_FLAGS "-std=gnu17", "-DCOBJMACROS", "-I", "include", "-MMD", "-MP", "-Wno-multichar", "-Wunused-parameter", "-fdiagnostics-color=always"
#define LINK_FLAGS "-lcomctl32", "-lgdiplus", "-lgdi32", "-lshlwapi", "-lshell32", \
                   "-lwinmm", "-lole32", "-luuid", "-lmsimg32", "-ldbghelp", "-ldwmapi", \
                   "-lpsapi", "-lcrypt32", "-lcomdlg32", "-ladvapi32"

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

static const char* dep_path(const char* src) {
  return nob_temp_sprintf(OBJ_FOLDER "/%s.d", nob_path_name(src));
}

static bool parse_depfile(const char* path, Nob_File_Paths* deps) {
  FILE* f = fopen(path, "rb");
  if (!f) return false;
  char line[4096];
  bool first = true;
  while (fgets(line, sizeof(line), f)) {
    char* p = line;
    if (first) {
      char* colon = strchr(line, ':');
      if (!colon) {
        fclose(f);
        return false;
      }
      p = colon + 1;
      first = false;
    }
    size_t len = strlen(p);
    while (len > 0 && (p[len-1] == '\n' || p[len-1] == '\r' || p[len-1] == ' ' || p[len-1] == '\t')) len--;
    if (len > 0 && p[len-1] == '\\') len--;
    p[len] = '\0';
    char* tok = strtok(p, " \t\r\n");
    while (tok) {
      size_t tok_len = strlen(tok);
      while (tok_len > 0 && tok[tok_len-1] == ':') tok_len--;
      tok[tok_len] = '\0';
      if (tok_len > 0) nob_da_append(deps, strdup(tok));
      tok = strtok(NULL, " \t\r\n");
    }
  }
  fclose(f);
  return true;
}

static bool obj_is_stale(const char* src, const char* obj) {
  if (!nob_file_exists(obj)) return true;
  if (nob_needs_rebuild1(obj, src)) {
    nob_log(NOB_INFO, "STALE %s: %s is newer", obj, src);
    return true;
  }
  const char* dep_file = dep_path(src);
  Nob_File_Paths deps = {0};
  if (parse_depfile(dep_file, &deps)) {
    for (size_t i = 0; i < deps.count; ++i) {
      if (nob_needs_rebuild1(obj, deps.items[i])) {
        nob_log(NOB_INFO, "STALE %s: %s is newer", obj, deps.items[i]);
        return true;
      }
    }
    return false;
  }
  nob_log(NOB_INFO, "STALE %s: missing/invalid dependency file %s", obj, dep_file);
  return true;
}

static const char* compile_log_path(const char* src) {
  return nob_temp_sprintf(OBJ_FOLDER "/%s.log", nob_path_name(src));
}

static void delete_log_file(const char* path) {
  for (int attempt = 0; attempt < 20; ++attempt) {
    if (!nob_file_exists(path)) return;
    if (DeleteFileA(path)) return;
    Sleep(5);
  }
}

static void kill_pid_tree(DWORD root_pid) {
  DWORD killed[256];
  size_t killed_count = 0;
  killed[killed_count++] = root_pid;

  for (int round = 0; round < 5; ++round) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) break;
    bool found = false;
    PROCESSENTRY32 pe = {0};
    pe.dwSize = sizeof(pe);
    for (BOOL ok = Process32First(snap, &pe); ok; ok = Process32Next(snap, &pe)) {
      if (killed_count >= NOB_ARRAY_LEN(killed)) break;
      for (size_t i = 0; i < killed_count; ++i) {
        if (pe.th32ParentProcessID != killed[i]) continue;
        DWORD child_pid = pe.th32ProcessID;
        bool seen = false;
        for (size_t j = 0; j < killed_count; ++j) {
          if (killed[j] == child_pid) { seen = true; break; }
        }
        if (seen) break;
        HANDLE child = OpenProcess(PROCESS_TERMINATE, FALSE, child_pid);
        if (child) {
          TerminateProcess(child, 1);
          CloseHandle(child);
          found = true;
          killed[killed_count++] = child_pid;
        }
        break;
      }
    }
    CloseHandle(snap);
    if (!found) break;
    Sleep(2);
  }

  for (int attempt = 0; attempt < 50; ++attempt) {
    bool any_alive = false;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
      PROCESSENTRY32 pe = {0};
      pe.dwSize = sizeof(pe);
      for (BOOL ok = Process32First(snap, &pe); ok; ok = Process32Next(snap, &pe)) {
        for (size_t i = 0; i < killed_count; ++i) {
          if (pe.th32ProcessID == killed[i]) { any_alive = true; break; }
        }
        if (any_alive) break;
      }
      CloseHandle(snap);
    }
    if (!any_alive) break;
    Sleep(5);
  }
}

static void terminate_process_tree(Nob_Proc proc) {
  kill_pid_tree(GetProcessId(proc));
  TerminateProcess(proc, 1);
}

static void drain_done(Nob_Procs* procs, Nob_File_Paths* logs, bool* failed, const char** failed_log) {
  for (size_t i = 0; i < procs->count; ++i) {
    int ret = nob__proc_wait_async(procs->items[i], 1);
    if (ret == 0) continue;
    if (ret < 0) {
      *failed = true;
      *failed_log = logs->items[i];
      CloseHandle(procs->items[i]);
      nob_da_remove_unordered(procs, i);
      nob_da_remove_unordered(logs, i);
      for (size_t j = 0; j < procs->count; ++j) {
        terminate_process_tree(procs->items[j]);
        WaitForSingleObject(procs->items[j], 1000);
        CloseHandle(procs->items[j]);
      }
      Sleep(20);
      for (size_t j = 0; j < logs->count; ++j) {
        delete_log_file(logs->items[j]);
      }
      procs->count = 0;
      logs->count = 0;
      return;
    }
    nob_da_remove_unordered(procs, i);
    nob_da_remove_unordered(logs, i);
    return;
  }
}

static bool compile_all(bool* any_compiled) {
  if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return false;
  if (!nob_mkdir_if_not_exists(OBJ_FOLDER)) return false;

  Nob_File_Paths to_build = {0};
  for (size_t i = 0; i < sources.count; ++i) {
    const char* src = sources.items[i];
    if (obj_is_stale(src, obj_path(src))) {
      nob_da_append(&to_build, src);
    }
  }
  *any_compiled = to_build.count > 0;
  if (to_build.count == 0) return true;

  size_t max_procs = (size_t)nob_nprocs() + 1;
  Nob_Procs procs = {0};
  Nob_File_Paths logs = {0};
  const char* failed_log = NULL;
  bool failed = false;

  for (size_t i = 0; i < to_build.count; ++i) {
    while (procs.count >= max_procs && !failed) {
      drain_done(&procs, &logs, &failed, &failed_log);
      if (procs.count >= max_procs && !failed) Sleep(1);
    }
    if (failed) break;

    const char* src = to_build.items[i];
    const char* log = compile_log_path(src);
    delete_log_file(log);
    Nob_Cmd cmd = {0};
    nob_cc(&cmd);
    nob_cc_flags(&cmd);
    nob_cmd_append(&cmd, CC_FLAGS, "-c", src, "-o", obj_path(src));
    if (!nob_cmd_run(&cmd, .async = &procs, .max_procs = max_procs,
                     .stderr_path = log)) {
      failed = true;
      failed_log = log;
      break;
    }
    nob_da_append(&logs, (char*)log);
  }

  while (procs.count > 0 && !failed) {
    drain_done(&procs, &logs, &failed, &failed_log);
    if (procs.count > 0 && !failed) Sleep(1);
  }

  if (failed) {
    for (size_t j = 0; j < procs.count; ++j) {
      terminate_process_tree(procs.items[j]);
      WaitForSingleObject(procs.items[j], 1000);
      CloseHandle(procs.items[j]);
    }
    Sleep(20);
    for (size_t j = 0; j < logs.count; ++j) {
      delete_log_file(logs.items[j]);
    }
    procs.count = 0;
    logs.count = 0;
  }

  if (failed_log != NULL) {
    Nob_String_Builder sb = {0};
    if (nob_read_entire_file(failed_log, &sb)) {
      nob_log(NOB_ERROR, "Compilation failed. Full output (%s):", failed_log);
      if (sb.count > 0) {
        fwrite(sb.items, 1, sb.count, stdout);
        if (sb.items[sb.count - 1] != '\n') printf("\n");
      }
    } else {
      nob_log(NOB_ERROR, "Compilation failed (%s), but no log was written", failed_log);
    }
    nob_sb_free(sb);
  }

  if (failed) {
    nob_log(NOB_ERROR, "Compilation failed");
    for (size_t j = 0; j < sources.count; ++j) {
      const char* log = compile_log_path(sources.items[j]);
      if (failed_log == NULL || strcmp(log, failed_log) != 0) delete_log_file(log);
    }
  } else {
    for (size_t j = 0; j < sources.count; ++j) {
      delete_log_file(compile_log_path(sources.items[j]));
    }
  }

  nob_da_free(logs);
  return !failed;
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
  const char* log = BUILD_FOLDER "/link.log";
  if (!nob_cmd_run(&cmd, .stderr_path = log)) {
    Nob_String_Builder sb = {0};
    if (nob_read_entire_file(log, &sb)) {
      if (sb.count > 0) {
        fwrite(sb.items, 1, sb.count, stdout);
        if (sb.items[sb.count - 1] != '\n') printf("\n");
      }
    }
    nob_sb_free(sb);
    delete_log_file(log);
    nob_log(NOB_ERROR, "Failed to link %s", output);
    return false;
  }
  delete_log_file(log);
  return true;
}

static void json_escape(Nob_String_Builder* sb, const char* s) {
  for (const char* p = s; *p; ++p) {
    if (*p == '\\') {
      nob_da_append(sb, '/');
    } else if (*p == '"') {
      nob_sb_append_cstr(sb, "\\\"");
    } else {
      nob_da_append(sb, *p);
    }
  }
}

static void json_str(Nob_String_Builder* sb, const char* s) {
  nob_da_append(sb, '"');
  json_escape(sb, s);
  nob_da_append(sb, '"');
}

static bool write_compile_commands(void) {
  Nob_String_Builder sb = {0};
  nob_sb_append_cstr(&sb, "[\n");
  for (size_t i = 0; i < sources.count; ++i) {
    const char* src = sources.items[i];
    if (i > 0) nob_sb_append_cstr(&sb, ",\n");
    nob_sb_append_cstr(&sb, "  {\"directory\": ");
    json_str(&sb, nob_get_current_dir_temp());
    nob_sb_append_cstr(&sb, ", \"file\": ");
    json_str(&sb, src);
    nob_sb_append_cstr(&sb, ", \"arguments\": [");
    const char* args[] = {"cc", "-Wall", "-Wextra", CC_FLAGS, "-x", "c", "-c", src};
    for (size_t j = 0; j < NOB_ARRAY_LEN(args); ++j) {
      if (j > 0) nob_da_append(&sb, ',');
      nob_da_append(&sb, ' ');
      json_str(&sb, args[j]);
    }
    nob_sb_append_cstr(&sb, " ]}");
  }
  nob_sb_append_cstr(&sb, "\n]\n");
  bool ok = nob_write_entire_file("compile_commands.json", sb.items, sb.count);
  nob_sb_free(sb);
  if (ok) nob_log(NOB_INFO, "Wrote compile_commands.json");
  return ok;
}

static bool clean(void) {
  for (size_t i = 0; i < sources.count; ++i) {
    nob_delete_file(obj_path(sources.items[i]));
    nob_delete_file(dep_path(sources.items[i]));
    delete_log_file(compile_log_path(sources.items[i]));
  }
  nob_delete_file(CONSOLE_OUTPUT);
  nob_delete_file(ACTUAL_OUTPUT);
  nob_log(NOB_INFO, "Cleaned " BUILD_FOLDER);
  return true;
}

static bool kill_running(const char* image_name) {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return false;
  PROCESSENTRY32 pe = {0};
  pe.dwSize = sizeof(pe);
  bool killed_any = false;
  for (BOOL ok = Process32First(snap, &pe); ok; ok = Process32Next(snap, &pe)) {
    if (_stricmp(pe.szExeFile, image_name) == 0) {
      HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
      if (proc) {
        TerminateProcess(proc, 0);
        CloseHandle(proc);
        killed_any = true;
      }
    }
  }
  CloseHandle(snap);
  if (killed_any) nob_log(NOB_INFO, "Killed running instance(s) of %s", image_name);
  return killed_any;
}

static void prepend_compiler_to_path(void) {
#ifdef _WIN32
  char full[MAX_PATH];
  char* file = NULL;
  if (SearchPathA(NULL, "cc.exe", NULL, sizeof(full), full, &file) == 0 || file == NULL) {
    return;
  }
  *file = '\0';
  const char* old_path = getenv("PATH");
  if (old_path == NULL) old_path = "";
  char new_path[MAX_PATH + 8192];
  snprintf(new_path, sizeof(new_path), "%s;%s", full, old_path);
  _putenv_s("PATH", new_path);
#endif
}

int main(int argc, char** argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);
  prepend_compiler_to_path();

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

  if (do_clean) return clean() ? 0 : 1;

  if (!write_compile_commands()) return 1;

  const char* output = windowed ? ACTUAL_OUTPUT : CONSOLE_OUTPUT;

  if (do_run) kill_running(nob_path_name(output));

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
