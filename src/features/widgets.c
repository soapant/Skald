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

#define NET_POPUP_W 300
#define NET_POPUP_H 100
#define NET_ROW_H 26
#define NET_PAD 16
#define NET_SAVE_EVERY_SEC 5
#define NET_STATE_FILE "net_usage.txt"

typedef struct {
    unsigned long long today_bytes;
    unsigned long long month_bytes;
    int today_day;
    int month_index;
    unsigned long long last_total_rx;
    double rate_bps;
    char last_iface[64];
    int have_baseline;
} NetUsage;

static NetUsage g_net;
static FlTimer g_net_last_save;
static FlTimer g_net_last_sample;
static bool g_net_persist_loaded;

static void net_localtime(time_t t, struct tm *out) {
#if defined(_WIN32)
    localtime_s(out, &t);
#else
    localtime_r(&t, out);
#endif
}

static void net_usage_reset_day(NetUsage *u, int day, int month_index) {
    if (u->today_day == day && u->month_index == month_index) return;
    if (u->month_index != month_index) {
        u->month_bytes = 0;
        u->month_index = month_index;
    }
    u->today_bytes = 0;
    u->today_day = day;
}

static void net_usage_load(void) {
    if (g_net_persist_loaded) return;
    g_net_persist_loaded = true;

    time_t now = time(NULL);
    struct tm tm_now;
    net_localtime(now, &tm_now);
    g_net.today_day = tm_now.tm_mday;
    g_net.month_index = tm_now.tm_mon;

    char body[512];
    if (fl_load_file(NET_STATE_FILE, body, sizeof(body)) <= 0) return;

    char *cursor = body;
    while (cursor && *cursor) {
        char *nl = strchr(cursor, '\n');
        if (nl) *nl = '\0';
        unsigned long long today = 0, month = 0;
        int day = 0, month_index = 0;
        if (sscanf(cursor, "%d %d %llu %llu", &day, &month_index, &today, &month) == 4) {
            if (month_index == g_net.month_index) {
                g_net.month_bytes = month;
                if (day == g_net.today_day) g_net.today_bytes = today;
            }
        }
        cursor = nl ? nl + 1 : NULL;
    }
}

static void net_usage_save(void) {
    char line[128];
    snprintf(line, sizeof(line), "%d %d %llu %llu",
             g_net.today_day, g_net.month_index, g_net.today_bytes, g_net.month_bytes);
    fl_write_string_to_file(NET_STATE_FILE, line);
}

static void net_usage_sample(void) {
    unsigned long long total_rx = fl_net_rx_bytes();
    double elapsed = fl_end_timer(g_net_last_sample);
    if (g_net.have_baseline && total_rx >= g_net.last_total_rx && elapsed > 0.05) {
        double delta = (double)(total_rx - g_net.last_total_rx);
        double inst = delta / elapsed;
        if (g_net.rate_bps <= 0.0) g_net.rate_bps = inst;
        else g_net.rate_bps = g_net.rate_bps * 0.7 + inst * 0.3;
    }
    if (g_net.have_baseline && total_rx > g_net.last_total_rx) {
        unsigned long long delta = total_rx - g_net.last_total_rx;
        g_net.today_bytes += delta;
        g_net.month_bytes += delta;
    }
    g_net.last_total_rx = total_rx;
    g_net.have_baseline = 1;
    g_net_last_sample = fl_start_timer();
}

static void net_usage_tick(void) {
    time_t now = time(NULL);
    struct tm tm_now;
    net_localtime(now, &tm_now);
    net_usage_reset_day(&g_net, tm_now.tm_mday, tm_now.tm_mon);
    net_usage_sample();

    if (fl_end_timer(g_net_last_save) >= (double)NET_SAVE_EVERY_SEC * 1000.0) {
        net_usage_save();
        g_net_last_save = fl_start_timer();
    }
}

static void net_row(int y, const char *label, const char *value, unsigned long color) {
    fl_draw_text(.text = label, .x = NET_PAD, .y = y,
                 .fontsize = FONTSIZE, .color = color, .align = "LEFT");
    fl_draw_text(.text = value, .x = NET_POPUP_W - NET_PAD, .y = y,
                 .fontsize = FONTSIZE, .color = RGB(235, 235, 245), .align = "RIGHT");
}

static void internet_hover_render(FlWidgetState *state) {
    net_usage_load();
    net_usage_tick();

    fl_draw_line(NET_PAD, NET_PAD + 30, NET_POPUP_W - NET_PAD, NET_PAD + 30, RGB(60, 60, 80));

    char today[32], month[32], rate[32];
    fl_format_bytes(g_net.today_bytes, today, sizeof(today));
    fl_format_bytes(g_net.month_bytes, month, sizeof(month));

    double bps = g_net.rate_bps;
    if (bps >= 1000000.0) {
        snprintf(rate, sizeof(rate), "%.2f MB/s", bps / 1048576.0);
    } else if (bps >= 1000.0) {
        snprintf(rate, sizeof(rate), "%.1f KB/s", bps / 1024.0);
    } else {
        snprintf(rate, sizeof(rate), "%.0f B/s", bps);
    }

    int y = NET_PAD;
    net_row(y, "rate now", rate, RGB(160, 220, 255));
    y += NET_ROW_H;

    net_row(y, "today", today, RGB(200, 200, 220));
    y += NET_ROW_H;

    net_row(y, "this month", month, RGB(200, 200, 220));
    y += NET_ROW_H + 4;
}

void on_internet_hover(void) {
    FlPos pos = fl_get_widget_pos("internet");

    fl_message(.x = pos.x,
               .y = pos.y - 70,
               .w = NET_POPUP_W,
               .h = NET_POPUP_H,
               .color_bg = RGB(1, 1, 1),
               .render = internet_hover_render,
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
              .fps = FPS,
              .no_bg = NO_BG,
              .on_hover = on_internet_hover,
              .slot = FL_WIDGET_SLOT_BOTTOM_RIGHT,
              .render = widgets_internet_render);

    fl_register_hotkey("Alt+Shift+S", FlipWidgets);
}
