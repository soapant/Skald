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

void get_disk_usage_percentage(char* buf, size_t buflen) {
#if !defined(_WIN32)
    struct statvfs vfs;
    if (statvfs("/", &vfs) != 0) {
        perror("statvfs");
        snprintf(buf, buflen, "Disk Usage:??%%");
        return;
    }
    unsigned long long block_size = vfs.f_frsize ? vfs.f_frsize : vfs.f_bsize;
    unsigned long long total = vfs.f_blocks * block_size;
    unsigned long long free_ = vfs.f_bfree * block_size;
    unsigned long long used  = total - free_;
    snprintf(buf, buflen, "Disk Usage:%llu%%", (used * 100) / total);
#else
    snprintf(buf, buflen, "Disk Usage:??%%");
#endif
}

void get_disk_usage(char* buf, size_t buflen) {
#if !defined(_WIN32)
    struct statvfs vfs;
    if (statvfs("/", &vfs) != 0) {
        perror("statvfs");
        snprintf(buf, buflen, "Disk Usage:??/??");
        return;
    }
    unsigned long long block_size = vfs.f_frsize ? vfs.f_frsize : vfs.f_bsize;
    unsigned long long total = vfs.f_blocks * block_size;
    unsigned long long free_ = vfs.f_bfree * block_size;
    unsigned long long used  = total - free_;

    char used_str[32];
    char total_str[32];
    get_size_string(used, used_str, sizeof(used_str));
    get_size_string(total, total_str, sizeof(total_str));

    snprintf(buf, buflen, "Disk Usage:%s/%s", used_str, total_str);
#else
    snprintf(buf, buflen, "Disk Usage:??/??");
#endif
}

void get_memory_usage_percentage(char* buf, size_t buflen) {
#if !defined(_WIN32)
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) {
        snprintf(buf, buflen, "Mem Usage:??%%");
        return;
    }

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

    if (total_kb == 0) {
        snprintf(buf, buflen, "Mem Usage:??%%");
        return;
    }

    unsigned long long used_kb = total_kb - avail_kb;
    snprintf(buf, buflen, "Mem Usage:%llu%%", (used_kb * 100) / total_kb);
#else
    snprintf(buf, buflen, "Mem Usage:??%%");
#endif
}

void get_memory_usage(char* buf, size_t buflen) {
#if !defined(_WIN32)
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) {
        snprintf(buf, buflen, "Mem Usage:??/??");
        return;
    }

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

    if (total_kb == 0) {
        snprintf(buf, buflen, "Mem Usage:??/??");
        return;
    }

    unsigned long long used_kb = total_kb - avail_kb;

    char used_str[32];
    char total_str[32];
    // Convert KB to Bytes before passing to get_size_string
    get_size_string(used_kb * 1024ULL, used_str, sizeof(used_str));
    get_size_string(total_kb * 1024ULL, total_str, sizeof(total_str));

    snprintf(buf, buflen, "Mem Usage:%s/%s", used_str, total_str);
#else
    snprintf(buf, buflen, "Mem Usage:??/??");
#endif
}

static void widgets_disk_usage_render(FlWidgetState *state) {
    char buf[128];
    ColorRef color = RGB(255, 255, 255);
    if (percentage) {
        get_disk_usage_percentage(buf, sizeof(buf));
        int num = 0;
        if (sscanf(buf, "Disk Usage:%d%%", &num) == 1) {
            if (num >= 80) color = RGB(255, 0, 0);
            else if (num >= 60) color = RGB(255, 165, 0);
            else if (num >= 40) color = RGB(0, 100, 0);
            else color = RGB(0, 150, 0);
        }
    } else {
        get_disk_usage(buf, sizeof(buf));
    }
    fl_draw_text(.text = buf, .x = 10, .y = 6, .fontsize = 15,
                 .color = color, .align = "LEFT");
}

static void widgets_memory_usage_render(FlWidgetState *state) {
    char buf[128];
    ColorRef color = RGB(255, 255, 255);
    if (percentage) {
        get_memory_usage_percentage(buf, sizeof(buf));
        int num = 0;
        if (sscanf(buf, "Mem Usage:%d%%", &num) == 1) {
            if (num >= 80) color = RGB(255, 0, 0);
            else if (num >= 60) color = RGB(255, 165, 0);
            else if (num >= 40) color = RGB(0, 100, 0);
            else color = RGB(0, 150, 0);
        }
    } else {
        get_memory_usage(buf, sizeof(buf));
    }
    fl_draw_text(.text = buf, .x = 10, .y = 6, .fontsize = 15,
                 .color = color, .align = "LEFT");
}

FLINT_FEATURE_INIT {
    fl_widget(.name = "disk_usage",
              .color = RGB(1, 1, 1),
              .slot = FL_WIDGET_SLOT_BOTTOM_LEFT,
              .render = widgets_disk_usage_render);

    fl_widget(.name = "memory_usage",
              .color = RGB(1, 1, 1),
              .slot = FL_WIDGET_SLOT_BOTTOM_LEFT,
              .render = widgets_memory_usage_render);

    fl_register_hotkey("Alt+Shift+S", FlipWidgets);
}
