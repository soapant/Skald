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
