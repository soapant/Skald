#include "platform/platform.h"
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>

const char* Platform_GetRealHome(void) {
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir) return pw->pw_dir;
    char *home = getenv("HOME");
    return home ? home : "/tmp";
}

const char* Platform_GetRealUser(void) {
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_name) return pw->pw_name;
    char *user = getenv("USER");
    return user ? user : "user";
}

bool Platform_QueryBrowser(char* query) {
    return false;
}

bool Platform_GetFeatureBuildCommand(const char* src, const char* out_tmp, char* cmd, size_t cmd_size) {
    snprintf(cmd, cmd_size, "gcc -std=gnu17 -I include  -pipe -fPIC -shared -o %s %s 2>&1", out_tmp, src);
    return true;
}

bool Platform_AtomicRename(const char* old_path, const char* new_path) {
    return rename(old_path, new_path) == 0;
}

int64_t Platform_TimerNowTicks(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

int64_t Platform_TimerTickFrequency(void) {
    return 1000000000LL;
}
