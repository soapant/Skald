#include "helpers/helpers_owner.h"
#include "platform/lib.h"
#include <stdio.h>
#include <string.h>

void Owner_ModuleBasename(const char* p, char* out, size_t n) {
    if (!out || n == 0) return;
    if (!p || !p[0]) { out[0] = '\0'; return; }
    const char* s = p;
    for (const char* q = p; *q; q++) if (*q == '/' || *q == '\\') s = q + 1;
    snprintf(out, n, "%s", s);
}

bool Owner_ModuleMatches(const char* owner, const char* a, const char* b) {
    if (!owner || !owner[0]) return false;
    if (a && a[0] && strcmp(owner, a) == 0) return true;
    if (b && b[0] && strcmp(owner, b) == 0) return true;
    char ob[OWNER_PATH_MAX], ab[OWNER_PATH_MAX], bb[OWNER_PATH_MAX];
    Owner_ModuleBasename(owner, ob, sizeof(ob));
    if (a && a[0]) { Owner_ModuleBasename(a, ab, sizeof(ab)); if (strcmp(ob, ab) == 0) return true; }
    if (b && b[0]) { Owner_ModuleBasename(b, bb, sizeof(bb)); if (strcmp(ob, bb) == 0) return true; }
    return false;
}

void Owner_ResolveForFunc(void* fn, char* out, size_t n) {
    if (!out || n == 0) return;
    out[0] = '\0';
    if (!fn) return;
    Platform_GetModuleOwnerPath(fn, out, n);
    out[n - 1] = '\0';
}