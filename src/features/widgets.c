#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "core/flapi.h"

#if !defined(_WIN32)
#include <sys/statvfs.h>
#endif

#define COLOR RGB(1, 1, 1)
#define FPS 1
#define BG true
#define FONTSIZE 14

static bool percentage = false;

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

static bool read_disk(unsigned long long *used_out, unsigned long long *total_out) {
    *used_out = 0;
    *total_out = 0;
#if !defined(_WIN32)
    struct statvfs vfs;
    if (statvfs("/", &vfs) != 0) return false;
    unsigned long long block_size = vfs.f_frsize ? vfs.f_frsize : vfs.f_bsize;
    unsigned long long total = vfs.f_blocks * block_size;
    if (total == 0) return false;
    *used_out = total - vfs.f_bfree * block_size;
    *total_out = total;
    return true;
#else
    return false;
#endif
}

static bool read_mem(unsigned long long *used_out, unsigned long long *total_out) {
    *used_out = 0;
    *total_out = 0;
#if !defined(_WIN32)
    FILE* fp = fopen("/proc/meminfo", "r");
    if (!fp) return false;
    unsigned long long total_kb = 0, avail_kb = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            sscanf(line + 9, "%llu", &total_kb);
        } else if (strncmp(line, "MemAvailable:", 13) == 0) {
            sscanf(line + 13, "%llu", &avail_kb);
        }
        if (total_kb && avail_kb) break;
    }
    fclose(fp);
    if (total_kb == 0) return false;
    *used_out = (total_kb - avail_kb) * 1024ULL;
    *total_out = total_kb * 1024ULL;
    return true;
#else
    return false;
#endif
}

static bool read_cpu(unsigned long long *used_out, unsigned long long *total_out) {
    static unsigned long long prev_busy = 0, prev_total = 0;
    static bool seeded = false;

    *used_out = 0;
    *total_out = 0;
#if !defined(_WIN32)
    FILE* fp = fopen("/proc/stat", "r");
    if (!fp) return false;
    char label[16];
    unsigned long long user = 0, nice = 0, sys = 0, idle = 0, iowait = 0;
    unsigned long long irq = 0, softirq = 0, steal = 0;
    int got = fscanf(fp, "%15s %llu %llu %llu %llu %llu %llu %llu %llu",
                     label, &user, &nice, &sys, &idle, &iowait, &irq, &softirq, &steal);
    fclose(fp);
    if (got < 5) return false;

    unsigned long long busy = user + nice + sys + irq + softirq + steal;
    unsigned long long total = busy + idle + iowait;
    if (!seeded) {
        seeded = true;
        prev_busy = busy;
        prev_total = total;
        *total_out = 1;
        return true;
    }
    unsigned long long d_total = total - prev_total;
    *used_out = busy - prev_busy;
    *total_out = d_total;
    if (d_total == 0) *used_out = 0;
    prev_busy = busy;
    prev_total = total;
    return d_total > 0;
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

static void draw_usage(const char* label, unsigned long long used, unsigned long long total) {
    char buf[128];
    if (total == 0) {
        snprintf(buf, sizeof(buf), "%s??", label);
    } else if (percentage) {
        snprintf(buf, sizeof(buf), "%s%3llu%%", label, (used * 100ULL) / total);
    } else {
        char used_str[32];
        char total_str[32];
        get_size_string(used, used_str, sizeof(used_str));
        get_size_string(total, total_str, sizeof(total_str));
        snprintf(buf, sizeof(buf), "%s%s/%s", label, used_str, total_str);
    }
    fl_draw_text(.text = buf, .x = 10, .y = 6, .fontsize = FONTSIZE,
                 .color = usage_color(used, total), .align = "LEFT");
}

static void widgets_disk_usage_render(FlWidgetState *state) {
    unsigned long long used = 0, total = 0;
    read_disk(&used, &total);
    draw_usage("Disk Usage:", used, total);
}

static void widgets_memory_usage_render(FlWidgetState *state) {
    unsigned long long used = 0, total = 0;
    read_mem(&used, &total);
    draw_usage("Mem Usage:", used, total);
}

static void widgets_cpu_usage_render(FlWidgetState *state) {
    unsigned long long used = 0, total = 0;
    read_cpu(&used, &total);
    char buf[128];
    if (total == 0) {
        snprintf(buf, sizeof(buf), "CPU Usage:??");
        fl_draw_text(.text = buf, .x = 10, .y = 6, .fontsize = FONTSIZE,
                     .color = RGB(255, 255, 255), .align = "LEFT");
        return;
    }
    snprintf(buf, sizeof(buf), "CPU Usage:%3llu%%", (used * 100ULL) / total);
    fl_draw_text(.text = buf, .x = 10, .y = 6, .fontsize = FONTSIZE,
                 .color = usage_color(used, total), .align = "LEFT");
}

static void widgets_date_and_time_render(FlWidgetState *state) {
    char buf[128];
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    if (!tm) return;

    strftime(buf, sizeof(buf), "%A, %B %d %H:%M:%S", tm);

    fl_draw_text(.text = buf, .x = 10, .y = 6, .fontsize = FONTSIZE,
                 .color = RGB(220, 220, 220), .align = "LEFT");
}

void FlipWidgets(void) {
    percentage = !percentage;
}

FLINT_FEATURE_INIT {
    fl_widget(.name = "disk_usage",
              .color = COLOR,
              .fps = FPS,
              .no_bg = BG,
              .slot = FL_WIDGET_SLOT_BOTTOM_LEFT,
              .render = widgets_disk_usage_render);

    fl_widget(.name = "memory_usage",
              .color = COLOR,
              .fps = FPS,
              .no_bg = BG,
              .slot = FL_WIDGET_SLOT_BOTTOM_LEFT,
              .render = widgets_memory_usage_render);

    fl_widget(.name = "cpu_usage",
              .color = COLOR,
              .fps = FPS,
              .no_bg = BG,
              .slot = FL_WIDGET_SLOT_BOTTOM_LEFT,
              .render = widgets_cpu_usage_render);

    fl_widget(.name = "date_and_time",
              .color = COLOR,
              .fps = FPS,
              .no_bg = BG,
              .slot = FL_WIDGET_SLOT_BOTTOM_RIGHT,
              .render = widgets_date_and_time_render);

    fl_register_hotkey("Alt+Shift+G", FlipWidgets);
}
