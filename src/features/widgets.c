#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "core/flapi.h"

#if !defined(_WIN32)
#include <sys/statvfs.h>
#endif

static bool percentage = false;

void FlipWidgets(void) {
    percentage = !percentage;
}

char* get_size_string(unsigned long long bytes, char* buf, size_t buflen) {
    const char* units[] = { "B", "KB", "MB", "GB", "TB", "PB", "EB" };
    const int   count   = sizeof(units) / sizeof(units[0]);

    double value = (double)bytes;
    int    i     = 0;

    while (value >= 1024.0 && i < count - 1) {
        value /= 1024.0;
        i++;
    }

    if (i == 0) {
        snprintf(buf, buflen, "%llu %s", bytes, units[i]);
    } else {
        snprintf(buf, buflen, "%.1f %s", value, units[i]);
    }

    return buf;
}

static bool disk_usage_bytes(unsigned long long *used_out, unsigned long long *total_out) {
    *used_out = 0;
    *total_out = 0;
#if !defined(_WIN32)
    struct statvfs vfs;
    if (statvfs("/", &vfs) != 0) return false;
    unsigned long long block_size = vfs.f_frsize ? vfs.f_frsize : vfs.f_bsize;
    unsigned long long total = vfs.f_blocks * block_size;
    unsigned long long free_ = vfs.f_bfree * block_size;
    if (total == 0) return false;
    *used_out = total - free_;
    *total_out = total;
    return true;
#else
    return false;
#endif
}

static bool memory_usage_bytes(unsigned long long *used_out, unsigned long long *total_out) {
    *used_out = 0;
    *total_out = 0;
#if !defined(_WIN32)
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) return false;
    unsigned long long total_kb = 0;
    unsigned long long avail_kb = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            sscanf(line + 9, "%llu", &total_kb);
        } else if (strncmp(line, "MemAvailable:", 13) == 0) {
            sscanf(line + 13, "%llu", &avail_kb);
        }
        if (total_kb && avail_kb) break;
    }
    fclose(f);
    if (total_kb == 0) return false;
    *used_out = (total_kb - avail_kb) * 1024ULL;
    *total_out = total_kb * 1024ULL;
    return true;
#else
    return false;
#endif
}

static ColorRef usage_color(unsigned long long used, unsigned long long total) {
    if (total == 0) return RGB(255, 255, 255);
    unsigned int pct = (unsigned int)((used * 100ULL) / total);
    if (pct >= 80) return RGB(255, 0, 0);
    if (pct >= 60) return RGB(255, 165, 0);
    if (pct >= 40) return RGB(0, 100, 0);
    return RGB(0, 150, 0);
}

static void widgets_disk_usage_render(FlWidgetState *state) {
    char buf[128];
    unsigned long long used = 0, total = 0;
    if (!disk_usage_bytes(&used, &total)) {
        snprintf(buf, sizeof(buf), "Disk Usage:??");
        fl_draw_text(.text = buf, .x = 10, .y = 6, .fontsize = 15,
                     .color = RGB(255, 255, 255), .align = "LEFT");
        return;
    }
    if (percentage) {
        snprintf(buf, sizeof(buf), "Disk Usage:%llu%%", (used * 100ULL) / total);
    } else {
        char used_str[32];
        char total_str[32];
        get_size_string(used, used_str, sizeof(used_str));
        get_size_string(total, total_str, sizeof(total_str));
        snprintf(buf, sizeof(buf), "Disk Usage:%s/%s", used_str, total_str);
    }
    fl_draw_text(.text = buf, .x = 10, .y = 6, .fontsize = 15,
                 .color = usage_color(used, total), .align = "LEFT");
}

static void widgets_memory_usage_render(FlWidgetState *state) {
    char buf[128];
    unsigned long long used = 0, total = 0;
    if (!memory_usage_bytes(&used, &total)) {
        snprintf(buf, sizeof(buf), "Mem Usage:??");
        fl_draw_text(.text = buf, .x = 10, .y = 6, .fontsize = 15,
                     .color = RGB(255, 255, 255), .align = "LEFT");
        return;
    }
    if (percentage) {
        snprintf(buf, sizeof(buf), "Mem Usage:%llu%%", (used * 100ULL) / total);
    } else {
        char used_str[32];
        char total_str[32];
        get_size_string(used, used_str, sizeof(used_str));
        get_size_string(total, total_str, sizeof(total_str));
        snprintf(buf, sizeof(buf), "Mem Usage:%s/%s", used_str, total_str);
    }
    fl_draw_text(.text = buf, .x = 10, .y = 6, .fontsize = 15,
                 .color = usage_color(used, total), .align = "LEFT");
}

FLINT_FEATURE_INIT {
    fl_widget(.name = "disk_usage",
              .color = RGB(1, 1, 1),
              .fps = 10,
              .slot = FL_WIDGET_SLOT_BOTTOM_LEFT,
              .render = widgets_disk_usage_render);

    fl_widget(.name = "memory_usage",
              .color = RGB(1, 1, 1),
              .fps = 10,
              .slot = FL_WIDGET_SLOT_BOTTOM_LEFT,
              .render = widgets_memory_usage_render);

    fl_register_hotkey("Alt+Shift+S", FlipWidgets);
}
