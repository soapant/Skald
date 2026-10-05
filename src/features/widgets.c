#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "core/flapi.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/statvfs.h>
#endif


#define COLOR RGB(1, 1, 111)
#define FPS 1
#define NO_BG true
#define FONTSIZE 14

static bool percentage = false;

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
    char root[MAX_PATH] = {0};
    DWORD n = GetCurrentDirectoryA(sizeof(root) - 1, root);
    if (n == 0 || n < 3) return false;
    if (root[1] != ':') return false;
    root[2] = '\\';
    root[3] = 0;

    ULARGE_INTEGER avail, total, free_total;
    if (!GetDiskFreeSpaceExA(root, &avail, &total, &free_total)) return false;
    if (total.QuadPart == 0) return false;
    *used_out = (unsigned long long)(total.QuadPart - free_total.QuadPart);
    *total_out = (unsigned long long)total.QuadPart;
    return true;
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
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (!GlobalMemoryStatusEx(&ms)) return false;
    if (ms.ullTotalPhys == 0) return false;
    *used_out = (unsigned long long)(ms.ullTotalPhys - ms.ullAvailPhys);
    *total_out = (unsigned long long)ms.ullTotalPhys;
    return true;
#endif
}

static bool sample_cpu(unsigned long long *busy_out, unsigned long long *total_out) {
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
    *busy_out = busy;
    *total_out = total;
    return true;
#else
    FILETIME idle_ft, kernel_ft, user_ft;
    if (!GetSystemTimes(&idle_ft, &kernel_ft, &user_ft)) return false;

    ULARGE_INTEGER idle, kernel, user;
    idle.LowPart = idle_ft.dwLowDateTime;
    idle.HighPart = idle_ft.dwHighDateTime;
    kernel.LowPart = kernel_ft.dwLowDateTime;
    kernel.HighPart = kernel_ft.dwHighDateTime;
    user.LowPart = user_ft.dwLowDateTime;
    user.HighPart = user_ft.dwHighDateTime;

    unsigned long long busy = (kernel.QuadPart - idle.QuadPart) + user.QuadPart;
    unsigned long long total = busy + idle.QuadPart;
    *busy_out = busy;
    *total_out = total;
    return true;
#endif
}

static bool read_cpu(unsigned long long *used_out, unsigned long long *total_out) {
    static unsigned long long prev_busy = 0, prev_total = 0;
    static bool seeded = false;

    *used_out = 0;
    *total_out = 0;

    unsigned long long busy = 0, total = 0;
    if (!sample_cpu(&busy, &total)) return false;

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
        fl_format_bytes(used, used_str, sizeof(used_str));
        fl_format_bytes(total, total_str, sizeof(total_str));
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

    strftime(buf, sizeof(buf), "%A %B %d %H:%M:%S", tm);

    fl_draw_text(.text = buf, .x = 10, .y = 6, .fontsize = FONTSIZE,
                 .color = RGB(220, 220, 220), .align = "LEFT");
}

static void widgets_internet_render(FlWidgetState *state) {
    FlNetStatus status = fl_net_status();
    switch (status) {
        case FL_NET_OFFLINE:
            fl_place_image(.path = "resources/wifi-not-avail.png", .x = 10, .y = 7, .h = FONTSIZE + 6, .w = FONTSIZE + 6);
            break;
        case FL_NET_ETHERNET:
            fl_place_image(.path = "resources/ethernet.png", .x = 10, .y = 7, .h = FONTSIZE + 6, .w = FONTSIZE + 6);
            break;
        case FL_NET_WIFI:
            fl_place_image(.path = "resources/wifi.png", .x = 10, .y = 7, .h = FONTSIZE + 6, .w = FONTSIZE + 6);
            break;
    }
}

static FlWidget* g_disk = NULL;
static FlWidget* g_mem = NULL;
static FlWidget* g_cpu = NULL;
static FlWidget* g_internet = NULL;

void on_internet_hover(void) {
    FlPos pos = fl_get_widget_pos("internet");
    fl_message(.message = "testing",                        
    .x = pos.x,                                  
    .h = 600,
    .w = 600,
    .y = pos.y - FONTSIZE * 4,                   
    .hover_key = "internet"); 
}


void FlipWidgets(void) {
    percentage = !percentage;
    fl_widget_set_render(g_disk, widgets_disk_usage_render);
    fl_widget_set_render(g_mem, widgets_memory_usage_render);
    fl_widget_set_render(g_cpu, widgets_cpu_usage_render);
    fl_widget_set_render(g_internet, widgets_internet_render);
}

FLINT_FEATURE_INIT {
    g_disk = fl_widget(.name = "disk_usage",
              .color = COLOR,
              .fps = FPS,
              .no_bg = NO_BG,
              .slot = FL_WIDGET_SLOT_BOTTOM_LEFT,
              .render = widgets_disk_usage_render);

    g_mem = fl_widget(.name = "memory_usage",
              .color = COLOR,
              .fps = FPS,
              .no_bg = NO_BG,
              .slot = FL_WIDGET_SLOT_BOTTOM_LEFT,
              .render = widgets_memory_usage_render);

    g_cpu = fl_widget(.name = "cpu_usage",
              .color = COLOR,
              .fps = FPS,
              .no_bg = NO_BG,
              .slot = FL_WIDGET_SLOT_BOTTOM_LEFT,
              .render = widgets_cpu_usage_render);

    fl_widget(.name = "date_and_time",
              .color = COLOR,
              .fps = FPS,
              .no_bg = NO_BG,
              .slot = FL_WIDGET_SLOT_BOTTOM_RIGHT,
              .render = widgets_date_and_time_render);

    g_internet = fl_widget(.name = "internet",
              .color = COLOR,
              .fps = 10,
              .no_bg = NO_BG,
              .on_hover = on_internet_hover,
              .slot = FL_WIDGET_SLOT_BOTTOM_RIGHT,
              .render = widgets_internet_render);

    fl_register_hotkey("Alt+Shift+S", FlipWidgets);
}
