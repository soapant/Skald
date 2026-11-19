#include "core/reload_lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "deps/log.h"
#include "platform/platform.h"
#include "helpers/helpers_fs.h"

bool reload_lib_is_fresh(const char *src, const char *out) {
  long long src_mt = 0, out_mt = 0;
  if (!Platform_FileInfo(src, &src_mt, NULL))
    return false;
  if (!Platform_FileInfo(out, &out_mt, NULL))
    return false;
  return out_mt >= src_mt;
}

bool reload_lib_compile(const char *src, const char *out, const char *log_tag) {
  static int build_seq = 0;
  const char *tag = log_tag ? log_tag : "reload lib";
  size_t tmp_len = strlen(out) + 48;
  char *tmp = malloc(tmp_len);
  if (!tmp)
    return false;
  snprintf(tmp, tmp_len, "%s.tmp.%d.%d", out, (int)getpid(), ++build_seq);

  const char *argv[PLATFORM_BUILD_ARGV_MAX];
  if (!Platform_GetFeatureBuildArgv(src, tmp, argv, PLATFORM_BUILD_ARGV_MAX)) {
    log_error("%s: no build argv for %s", tag, src);
    free(tmp);
    return false;
  }
  int status = -1;
  char *output = Platform_RunArgvCapture(argv, NULL, &status);
  bool ok = output != NULL && status == 0;
  if (output) {
    if (output[0])
      log_info("%s", output);
    free(output);
    output = NULL;
  }
  if (!ok) {
    if (output == NULL && status == -1)
      log_error("%s: could not run compiler for %s", tag, src);
    else
      log_error("%s: build failed for %s (exit %d)", tag, src, status);
    remove(tmp);
    free(tmp);
    return false;
  }
  if (!Platform_AtomicRename(tmp, out)) {
    log_error("%s: rename %s -> %s failed", tag, tmp, out);
    remove(tmp);
    free(tmp);
    return false;
  }
  free(tmp);
  return true;
}

static size_t hot_suffix_len(const char *s, size_t len) {
  while (len >= 5) {
    const char *p = s + len;
    size_t back = len;
    const char *q = p;
    while (back > 0 && q[-1] >= '0' && q[-1] <= '9') {
      q--;
      back--;
    }
    if (back == len || back < 4)
      return len;
    if (!(s[back - 4] == '.' && s[back - 3] == 'h' && s[back - 2] == 'o' &&
          s[back - 1] == 't'))
      return len;
    len = back - 4;
  }
  return len;
}

char *reload_lib_unique_copy(const char *src, const char *base, int seq,
                             const char *ext_suffix) {
  char seqbuf[32];
  snprintf(seqbuf, sizeof(seqbuf), ".hot%d", seq);
  char basebuf[1024];
  size_t base_len = strlen(base);
  size_t trim = hot_suffix_len(base, base_len);
  if (trim < sizeof(basebuf))
    memcpy(basebuf, base, trim), basebuf[trim] = '\0';
  else
    snprintf(basebuf, sizeof(basebuf), "%s", base);
  base = basebuf;
  base_len = strlen(basebuf);
  size_t seq_len = strlen(seqbuf);
  size_t ext_len = ext_suffix ? strlen(ext_suffix) : 0;
  char *path = malloc(base_len + seq_len + ext_len + 1);
  if (!path)
    return NULL;
  memcpy(path, base, base_len);
  memcpy(path + base_len, seqbuf, seq_len);
  if (ext_len)
    memcpy(path + base_len + seq_len, ext_suffix, ext_len);
  path[base_len + seq_len + ext_len] = '\0';
  if (!file_copy_path(src, path)) {
    free(path);
    return NULL;
  }
  return path;
}

int reload_lib_sweep_stale_hot_libs(const char *dir, const char *lib_ext) {
  DirIter *d = Platform_DirOpen(dir);
  if (!d)
    return -1;
  size_t ext_len = lib_ext ? strlen(lib_ext) : 0;
  int nremoved = 0;
  char name[256];
  bool name_is_dir;
  while (Platform_DirNext(d, name, sizeof(name), &name_is_dir)) {
    const char *dot = strstr(name, ".hot");
    if (!dot)
      continue;
    if (ext_len) {
      size_t plen = (size_t)(dot - name);
      if (plen < ext_len || memcmp(dot - ext_len, lib_ext, ext_len) != 0)
        continue;
    }
    const char *scan = dot;
    bool ok = true;
    for (;;) {
      const char *q = scan + 4;
      if (*q < '0' || *q > '9') {
        ok = false;
        break;
      }
      while (*q >= '0' && *q <= '9')
        q++;
      if (*q == '\0')
        break;
      if (ext_len && strcmp(q, lib_ext) == 0)
        break;
      if (strncmp(q, ".hot", 4) != 0) {
        ok = false;
        break;
      }
      scan = q;
    }
    if (!ok)
      continue;
    size_t path_len = strlen(dir) + 1 + strlen(name) + 1;
    char *junk = malloc(path_len);
    if (!junk)
      continue;
    snprintf(junk, path_len, "%s/%s", dir, name);
    if (remove(junk) == 0)
      nremoved++;
    free(junk);
  }
  Platform_DirClose(d);
  return nremoved;
}
