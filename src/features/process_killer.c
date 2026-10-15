<<<<<<< HEAD
#include "core/flapi.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if !defined(_WIN32)
#include <strings.h>
#endif

const char* bkp_windows[] = {"Brawlhalla","Fortnite","RocketLeague","Factorio","Steam","Wine","EpicGamesLauncher"};

#define PK_MAX        2048
#define PK_NAME_LEN   64
#define PK_ROW_LEN    160
#define PK_SORT_MODES 3

typedef struct {
    char name[PK_NAME_LEN];
    unsigned long long rss_kb;
    double cpu;
} PkProc;

static PkProc g_pk[PK_MAX];
static int g_pk_count = 0;
static int g_pk_sort = 0;
static FlTimer g_pk_last_collect;
static bool g_pk_collect_started = false;
static char g_pk_rows[PK_MAX][PK_ROW_LEN];
static const char* g_pk_items[PK_MAX];

static const char* pk_sort_label(int mode) {
    if (mode == 1) return "cpu";
    if (mode == 2) return "name";
    return "memory";
}

static bool pk_isspace(char c) { return c == ' ' || c == '\t'; }



static int pk_cmp_mem(const void* a, const void* b) {
    const PkProc* x = a;
    const PkProc* y = b;
    if (x->rss_kb != y->rss_kb) return x->rss_kb < y->rss_kb ? 1 : -1;
    return strcmp(x->name, y->name);
}

static int pk_cmp_cpu(const void* a, const void* b) {
    const PkProc* x = a;
    const PkProc* y = b;
    if (x->cpu != y->cpu) return x->cpu < y->cpu ? 1 : -1;
    if (x->rss_kb != y->rss_kb) return x->rss_kb < y->rss_kb ? 1 : -1;
    return strcmp(x->name, y->name);
}

static int pk_cmp_alpha(const void* a, const void* b) {
    return strcasecmp(((const PkProc*)a)->name, ((const PkProc*)b)->name);
}


#if !defined(_WIN32)

static void pk_rtrim(char* s) {
    size_t n = strlen(s);
    while (n > 0 && pk_isspace(s[n - 1])) s[--n] = '\0';
}

static void pk_parse_line(char* line) {
    pk_rtrim(line);
    size_t len = strlen(line);
    if (len == 0) return;

    size_t e = len;
    while (e > 0 && pk_isspace(line[e - 1])) e--;
    if (e == 0) return;

    size_t f3 = e;
    while (f3 > 0 && !pk_isspace(line[f3 - 1])) f3--;
    if (f3 == 0) return;
    line[f3] = '\0';
    const char* cpu_s = line + f3 + 1;

    size_t f2 = f3;
    while (f2 > 0 && pk_isspace(line[f2 - 1])) f2--;
    if (f2 == 0) return;
    size_t f2e = f2;
    while (f2 > 0 && !pk_isspace(line[f2 - 1])) f2--;

    char rss_buf[32];
    size_t rlen = f2e - f2;
    if (rlen >= sizeof(rss_buf)) rlen = sizeof(rss_buf) - 1;
    memcpy(rss_buf, line + f2, rlen);
    rss_buf[rlen] = '\0';
    line[f2] = '\0';
    const char* rss_s = rss_buf;

    char* name = line;
    while (pk_isspace(*name)) name++;
    if (*name == '\0') return;
    if (g_pk_count >= PK_MAX) return;

    PkProc* p = &g_pk[g_pk_count++];
    snprintf(p->name, sizeof(p->name), "%s", name);
    p->rss_kb = strtoull(rss_s, NULL, 10);
    p->cpu = strtod(cpu_s, NULL);
}

static void pk_collect_running_processes(void) {
    g_pk_count = 0;
    char* out = fl_run_terminal_command("ps -eo comm=,rss=,%cpu= --no-headers");
    if (!out) return;
    char* p = out;
    while (*p && g_pk_count < PK_MAX) {
        char* nl = strchr(p, '\n');
        if (nl) *nl = '\0';
        pk_parse_line(p);
        p = nl ? nl + 1 : p + strlen(p);
    }
    free(out);
}
#else 
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

static int find_or_add_pk_proc(const char* name) {
    for (int i = 0; i < g_pk_count; i++) {
        if (strcasecmp(g_pk[i].name, name) == 0) {
            return i;
        }
    }
    if (g_pk_count >= PK_MAX) return -1;
    
    int index = g_pk_count++;
    snprintf(g_pk[index].name, sizeof(g_pk[index].name), "%s", name);
    g_pk[index].rss_kb = 0;
    g_pk[index].cpu = 0.0;
    return index;
}

static void pk_collect_running_processes(void) {
    g_pk_count = 0;

    typedef struct { DWORD pid; ULONGLONG cpu_start; } CpuSample;
    static CpuSample samples[PK_MAX];
    int sample_count = 0;

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    DWORD num_cores = sysInfo.dwNumberOfProcessors; 

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hSnapshot, &pe32)) {
        CloseHandle(hSnapshot);
        return;
    }

    
    do {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe32.th32ProcessID);
        if (hProcess) {
            FILETIME ftCreation, ftExit, ftKernel, ftUser;
            if (GetProcessTimes(hProcess, &ftCreation, &ftExit, &ftKernel, &ftUser) && sample_count < PK_MAX) {
                ULONGLONG total_ticks = (((ULONGLONG)ftKernel.dwHighDateTime << 32) | ftKernel.dwLowDateTime) +
                                       (((ULONGLONG)ftUser.dwHighDateTime << 32) | ftUser.dwLowDateTime);
                samples[sample_count].pid = pe32.th32ProcessID;
                samples[sample_count].cpu_start = total_ticks;
                sample_count++;
            }
            CloseHandle(hProcess);
        }
    } while (Process32Next(hSnapshot, &pe32));

    if (!Process32First(hSnapshot, &pe32)) {
        CloseHandle(hSnapshot);
        return;
    }

    do {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe32.th32ProcessID);
        if (hProcess) {
            PROCESS_MEMORY_COUNTERS_EX pmc; 
            if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
                
                int index = find_or_add_pk_proc(pe32.szExeFile);
                if (index != -1) {
                    
                    g_pk[index].rss_kb += (unsigned long long)(pmc.PrivateUsage / 1024);

                    FILETIME ftCreation, ftExit, ftKernel, ftUser;
                    if (GetProcessTimes(hProcess, &ftCreation, &ftExit, &ftKernel, &ftUser)) {
                        ULONGLONG cpu_end = (((ULONGLONG)ftKernel.dwHighDateTime << 32) | ftKernel.dwLowDateTime) +
                                            (((ULONGLONG)ftUser.dwHighDateTime << 32) | ftUser.dwLowDateTime);
                        
                        
                        for (int s = 0; s < sample_count; s++) {
                            if (samples[s].pid == pe32.th32ProcessID) {
                                ULONGLONG delta_ticks = (cpu_end > samples[s].cpu_start) ? (cpu_end - samples[s].cpu_start) : 0;
                                
                                
                                double process_pct = ((double)delta_ticks / 1000000.0) * 100.0;
                                g_pk[index].cpu += (process_pct / num_cores);
                                break;
                            }
                        }
                    }
                }
            }
            CloseHandle(hProcess);
        }
    } while (Process32Next(hSnapshot, &pe32));

    CloseHandle(hSnapshot);

    
    for (int i = 0; i < g_pk_count; i++) {
        if (g_pk[i].cpu > 100.0) g_pk[i].cpu = 100.0;
    }
}
#endif

static void pk_sort(void) {
    if (g_pk_count <= 1) return;
    qsort(g_pk, (size_t)g_pk_count, sizeof(PkProc),
          g_pk_sort == 0 ? pk_cmp_mem : g_pk_sort == 1 ? pk_cmp_cpu : pk_cmp_alpha);
}

static void pk_split_row(const char* row, char* name, size_t name_len,
                         char* mem, size_t mem_len, char* cpu, size_t cpu_len) {
    if (name_len) name[0] = '\0';
    if (mem_len) mem[0] = '\0';
    if (cpu_len) cpu[0] = '\0';
    if (!row || !*row) return;

    const char* bar1 = strchr(row, '|');
    if (!bar1) { snprintf(name, name_len, "%s", row); return; }

    size_t n = (size_t)(bar1 - row);
    while (n > 0 && (row[n - 1] == ' ' || row[n - 1] == '\t')) n--;
    snprintf(name, name_len, "%.*s", (int)n, row);

    const char* rest = bar1 + 1;
    while (*rest == ' ' || *rest == '\t') rest++;
    const char* bar2 = strchr(rest, '|');
    if (!bar2) { snprintf(mem, mem_len, "%s", rest); return; }

    size_t m = (size_t)(bar2 - rest);
    while (m > 0 && (rest[m - 1] == ' ' || rest[m - 1] == '\t')) m--;
    snprintf(mem, mem_len, "%.*s", (int)m, rest);

    const char* c = bar2 + 1;
    while (*c == ' ' || *c == '\t') c++;
    snprintf(cpu, cpu_len, "%s%%", c);
}

static void pk_build_rows(void) {
    for (int i = 0; i < g_pk_count; i++) {
        char mem[24];
        fl_format_bytes((unsigned long long)g_pk[i].rss_kb * 1024ULL, mem, sizeof(mem));
        snprintf(g_pk_rows[i], PK_ROW_LEN, "%.48s | %.20s | %.1f", g_pk[i].name, mem, g_pk[i].cpu);
        g_pk_items[i] = g_pk_rows[i];
    }
}

static void pk_collect_and_sort(void) {
    pk_collect_running_processes();
    pk_sort();
    pk_build_rows();
}

static void BulkKillProcesses(const char* window_titles[], int count) {
    for (int i = 0; i < count; i++) {
        if (!window_titles[i] || !*window_titles[i]) continue;
        fl_terminate_process_by_name(window_titles[i]);
    }
}

static void bulk_kill_bkp(void) {
    BulkKillProcesses(bkp_windows, ARRAY_LEN(bkp_windows));
}

static void pk_cycle_sort(void) {
    g_pk_sort = (g_pk_sort + 1) % PK_SORT_MODES;
    pk_collect_and_sort();
}

static bool pk_enter_callback(const char* selection, ListBoxInfo* lb) {
    if (!selection || !*selection) return false;
    char name[PK_NAME_LEN];
    char mem[32];
    char cpu[24];
    pk_split_row(selection, name, sizeof(name), mem, sizeof(mem), cpu, sizeof(cpu));
    if (name[0] == '\0') return false;
    fl_terminate_process_by_name(name);
    return true;
}

static void pk_refresh_callback(const char* filter, ListBoxInfo* lb) {
    g_pk_last_collect = fl_start_timer();
    g_pk_collect_started = true;
    pk_collect_and_sort();
    fl_populate_listbox(lb, filter, g_pk_items, g_pk_count);
}

#define PK_COL_MEM 430
#define PK_COL_CPU 600

static void pk_render_callback(PromptState* state) {
    if (!state) return;
    double since = g_pk_collect_started ? fl_end_timer(g_pk_last_collect) : 1e9;
    if (since >= 1000.0) {
        g_pk_last_collect = fl_start_timer();
        g_pk_collect_started = true;
        int keep = state->listbox.selected_index;
        pk_collect_and_sort();
        fl_populate_listbox(&state->listbox, state->filter, g_pk_items, g_pk_count);
        if (keep >= 0 && keep < state->listbox.populated_count) {
            state->listbox.selected_index = keep;
        }
    }

    if (fl_ctrl_pressed() && fl_is_key_pressed('L')) {
        pk_cycle_sort();
        fl_populate_listbox(&state->listbox, NULL, g_pk_items, g_pk_count);
    }

    fl_draw_text(.text = "memory", .x = PK_COL_MEM, .y = 8, .fontsize = 13,
                 .align = "LEFT", .color = RGB(130, 130, 160));
    fl_draw_text(.text = "cpu", .x = PK_COL_CPU, .y = 8, .fontsize = 13,
                 .align = "LEFT", .color = RGB(130, 130, 160));
    char sort_label[64];
    snprintf(sort_label, sizeof(sort_label), "current sort: %s", pk_sort_label(g_pk_sort));
    fl_draw_text(.text = sort_label, .x = 880, .y = 8, .fontsize = 13,
                 .align = "RIGHT", .color = RGB(160, 160, 190));

    ListBoxInfo* lb = &state->listbox;
    FlListSpec spec = {
        .lb = lb,
        .item_height = fl_get_text_height(NULL) + 20,
    };
    fl_place_editbox();
    if (!fl_listbox_begin(&spec)) return;

    FlListItemCtx ctx;
    while (fl_listbox_next(&ctx)) {
        char name[PK_NAME_LEN];
        char mem[32];
        char cpu[24];
        pk_split_row(ctx.text ? ctx.text : "", name, sizeof(name), mem, sizeof(mem), cpu, sizeof(cpu));
        if (name[0] == '\0') continue;

        fl_draw_text(.text = name, .x = ctx.x + 14, .y = ctx.y + 4,
                     .align = "LEFT", .pad = ctx.x + 14);
        fl_draw_text(.text = mem, .x = PK_COL_MEM, .y = ctx.y + 4, .align = "LEFT");
        fl_draw_text(.text = cpu, .x = PK_COL_CPU, .y = ctx.y + 4, .align = "LEFT");
    }
    fl_listbox_end();
}

static void TriggerProcessKiller(void) {
    pk_collect_and_sort();
    PROMPT(.title = "💀 ProcessKiller",
           .refresh = pk_refresh_callback,
           .enter = pk_enter_callback,
           .render = pk_render_callback);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+U", TriggerProcessKiller,"Fortnite");
    fl_register_hotkey("Alt+Shift+P", bulk_kill_bkp);

    fl_register_feature("Process Killer", TriggerProcessKiller);
}
=======
#include "core/flapi.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if !defined(_WIN32)
#include <strings.h>
#endif

const char* bkp_windows[] = {"Brawlhalla","Fortnite","RocketLeague","Factorio","Steam","Wine","EpicGamesLauncher"};

#define PK_MAX        2048
#define PK_NAME_LEN   64
#define PK_ROW_LEN    160
#define PK_SORT_MODES 3

typedef struct {
    char name[PK_NAME_LEN];
    unsigned long long rss_kb;
    double cpu;
} PkProc;

static PkProc g_pk[PK_MAX];
static int g_pk_count = 0;
static int g_pk_sort = 0;
static FlTimer g_pk_last_collect;
static bool g_pk_collect_started = false;
static char g_pk_rows[PK_MAX][PK_ROW_LEN];
static const char* g_pk_items[PK_MAX];

static const char* pk_sort_label(int mode) {
    if (mode == 1) return "cpu";
    if (mode == 2) return "name";
    return "memory";
}


static int pk_cmp_mem(const void* a, const void* b) {
    const PkProc* x = a;
    const PkProc* y = b;
    if (x->rss_kb != y->rss_kb) return x->rss_kb < y->rss_kb ? 1 : -1;
    return strcmp(x->name, y->name);
}

static int pk_cmp_cpu(const void* a, const void* b) {
    const PkProc* x = a;
    const PkProc* y = b;
    if (x->cpu != y->cpu) return x->cpu < y->cpu ? 1 : -1;
    if (x->rss_kb != y->rss_kb) return x->rss_kb < y->rss_kb ? 1 : -1;
    return strcmp(x->name, y->name);
}

static int pk_cmp_alpha(const void* a, const void* b) {
    return strcasecmp(((const PkProc*)a)->name, ((const PkProc*)b)->name);
}


#if !defined(_WIN32)

static void pk_rtrim(char* s) {
    size_t n = strlen(s);
    while (n > 0 && isspace(s[n - 1])) s[--n] = '\0';
}

static void pk_parse_line(char* line) {
    pk_rtrim(line);
    size_t len = strlen(line);
    if (len == 0) return;

    size_t e = len;
    while (e > 0 && isspace(line[e - 1])) e--;
    if (e == 0) return;

    size_t f3 = e;
    while (f3 > 0 && !isspace(line[f3 - 1])) f3--;
    if (f3 == 0) return;
    line[f3] = '\0';
    const char* cpu_s = line + f3 + 1;

    size_t f2 = f3;
    while (f2 > 0 && isspace(line[f2 - 1])) f2--;
    if (f2 == 0) return;
    size_t f2e = f2;
    while (f2 > 0 && !isspace(line[f2 - 1])) f2--;

    char rss_buf[32];
    size_t rlen = f2e - f2;
    if (rlen >= sizeof(rss_buf)) rlen = sizeof(rss_buf) - 1;
    memcpy(rss_buf, line + f2, rlen);
    rss_buf[rlen] = '\0';
    line[f2] = '\0';
    const char* rss_s = rss_buf;

    char* name = line;
    while (isspace(*name)) name++;
    if (*name == '\0') return;
    if (g_pk_count >= PK_MAX) return;

    PkProc* p = &g_pk[g_pk_count++];
    snprintf(p->name, sizeof(p->name), "%s", name);
    p->rss_kb = strtoull(rss_s, NULL, 10);
    p->cpu = strtod(cpu_s, NULL);
}

static void pk_collect_running_processes(void) {
    g_pk_count = 0;
    char* out = fl_run_terminal_command("ps -eo comm=,rss=,%cpu= --no-headers");
    if (!out) return;
    char* p = out;
    while (*p && g_pk_count < PK_MAX) {
        char* nl = strchr(p, '\n');
        if (nl) *nl = '\0';
        pk_parse_line(p);
        p = nl ? nl + 1 : p + strlen(p);
    }
    free(out);
}
#else 
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

static int find_or_add_pk_proc(const char* name) {
    for (int i = 0; i < g_pk_count; i++) {
        if (strcasecmp(g_pk[i].name, name) == 0) {
            return i;
        }
    }
    if (g_pk_count >= PK_MAX) return -1;
    
    int index = g_pk_count++;
    snprintf(g_pk[index].name, sizeof(g_pk[index].name), "%s", name);
    g_pk[index].rss_kb = 0;
    g_pk[index].cpu = 0.0;
    return index;
}

static void pk_collect_running_processes(void) {
    g_pk_count = 0;

    typedef struct { DWORD pid; ULONGLONG cpu_start; } CpuSample;
    static CpuSample samples[PK_MAX];
    int sample_count = 0;

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    DWORD num_cores = sysInfo.dwNumberOfProcessors; 

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hSnapshot, &pe32)) {
        CloseHandle(hSnapshot);
        return;
    }

    
    do {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe32.th32ProcessID);
        if (hProcess) {
            FILETIME ftCreation, ftExit, ftKernel, ftUser;
            if (GetProcessTimes(hProcess, &ftCreation, &ftExit, &ftKernel, &ftUser) && sample_count < PK_MAX) {
                ULONGLONG total_ticks = (((ULONGLONG)ftKernel.dwHighDateTime << 32) | ftKernel.dwLowDateTime) +
                                       (((ULONGLONG)ftUser.dwHighDateTime << 32) | ftUser.dwLowDateTime);
                samples[sample_count].pid = pe32.th32ProcessID;
                samples[sample_count].cpu_start = total_ticks;
                sample_count++;
            }
            CloseHandle(hProcess);
        }
    } while (Process32Next(hSnapshot, &pe32));

    if (!Process32First(hSnapshot, &pe32)) {
        CloseHandle(hSnapshot);
        return;
    }

    do {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe32.th32ProcessID);
        if (hProcess) {
            PROCESS_MEMORY_COUNTERS_EX pmc; 
            if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
                
                int index = find_or_add_pk_proc(pe32.szExeFile);
                if (index != -1) {
                    
                    g_pk[index].rss_kb += (unsigned long long)(pmc.PrivateUsage / 1024);

                    FILETIME ftCreation, ftExit, ftKernel, ftUser;
                    if (GetProcessTimes(hProcess, &ftCreation, &ftExit, &ftKernel, &ftUser)) {
                        ULONGLONG cpu_end = (((ULONGLONG)ftKernel.dwHighDateTime << 32) | ftKernel.dwLowDateTime) +
                                            (((ULONGLONG)ftUser.dwHighDateTime << 32) | ftUser.dwLowDateTime);
                        
                        
                        for (int s = 0; s < sample_count; s++) {
                            if (samples[s].pid == pe32.th32ProcessID) {
                                ULONGLONG delta_ticks = (cpu_end > samples[s].cpu_start) ? (cpu_end - samples[s].cpu_start) : 0;
                                
                                
                                double process_pct = ((double)delta_ticks / 1000000.0) * 100.0;
                                g_pk[index].cpu += (process_pct / num_cores);
                                break;
                            }
                        }
                    }
                }
            }
            CloseHandle(hProcess);
        }
    } while (Process32Next(hSnapshot, &pe32));

    CloseHandle(hSnapshot);

    
    for (int i = 0; i < g_pk_count; i++) {
        if (g_pk[i].cpu > 100.0) g_pk[i].cpu = 100.0;
    }
}
#endif

static void pk_sort(void) {
    if (g_pk_count <= 1) return;
    qsort(g_pk, (size_t)g_pk_count, sizeof(PkProc),
          g_pk_sort == 0 ? pk_cmp_mem : g_pk_sort == 1 ? pk_cmp_cpu : pk_cmp_alpha);
}

static void pk_split_row(const char* row, char* name, size_t name_len,
                         char* mem, size_t mem_len, char* cpu, size_t cpu_len) {
    if (name_len) name[0] = '\0';
    if (mem_len) mem[0] = '\0';
    if (cpu_len) cpu[0] = '\0';
    if (!row || !*row) return;

    const char* bar1 = strchr(row, '|');
    if (!bar1) { snprintf(name, name_len, "%s", row); return; }

    size_t n = (size_t)(bar1 - row);
    while (n > 0 && (row[n - 1] == ' ' || row[n - 1] == '\t')) n--;
    snprintf(name, name_len, "%.*s", (int)n, row);

    const char* rest = bar1 + 1;
    while (*rest == ' ' || *rest == '\t') rest++;
    const char* bar2 = strchr(rest, '|');
    if (!bar2) { snprintf(mem, mem_len, "%s", rest); return; }

    size_t m = (size_t)(bar2 - rest);
    while (m > 0 && (rest[m - 1] == ' ' || rest[m - 1] == '\t')) m--;
    snprintf(mem, mem_len, "%.*s", (int)m, rest);

    const char* c = bar2 + 1;
    while (*c == ' ' || *c == '\t') c++;
    snprintf(cpu, cpu_len, "%s%%", c);
}

static void pk_build_rows(void) {
    for (int i = 0; i < g_pk_count; i++) {
        char mem[24];
        fl_format_bytes((unsigned long long)g_pk[i].rss_kb * 1024ULL, mem, sizeof(mem));
        snprintf(g_pk_rows[i], PK_ROW_LEN, "%.48s | %.20s | %.1f", g_pk[i].name, mem, g_pk[i].cpu);
        g_pk_items[i] = g_pk_rows[i];
    }
}

static void pk_collect_and_sort(void) {
    pk_collect_running_processes();
    pk_sort();
    pk_build_rows();
}

static void BulkKillProcesses(const char* window_titles[], int count) {
    for (int i = 0; i < count; i++) {
        if (!window_titles[i] || !*window_titles[i]) continue;
        fl_terminate_process_by_name(window_titles[i]);
    }
}

static void bulk_kill_bkp(void) {
    BulkKillProcesses(bkp_windows, ARRAY_LEN(bkp_windows));
}

static void pk_cycle_sort(void) {
    g_pk_sort = (g_pk_sort + 1) % PK_SORT_MODES;
    pk_collect_and_sort();
}

static bool pk_enter_callback(const char* selection, ListBoxInfo* lb) {
    if (!selection || !*selection) return false;
    char name[PK_NAME_LEN];
    char mem[32];
    char cpu[24];
    pk_split_row(selection, name, sizeof(name), mem, sizeof(mem), cpu, sizeof(cpu));
    if (name[0] == '\0') return false;
    fl_terminate_process_by_name(name);
    return true;
}

static void pk_refresh_callback(const char* filter, ListBoxInfo* lb) {
    g_pk_last_collect = fl_start_timer();
    g_pk_collect_started = true;
    pk_collect_and_sort();
    fl_populate_listbox(lb, filter, g_pk_items, g_pk_count);
}

#define PK_COL_MEM 430
#define PK_COL_CPU 600

static void pk_render_callback(PromptState* state) {
    if (!state) return;
    double since = g_pk_collect_started ? fl_end_timer(g_pk_last_collect) : 1e9;
    if (since >= 1000.0) {
        g_pk_last_collect = fl_start_timer();
        g_pk_collect_started = true;
        int keep = state->listbox.selected_index;
        pk_collect_and_sort();
        fl_populate_listbox(&state->listbox, state->filter, g_pk_items, g_pk_count);
        if (keep >= 0 && keep < state->listbox.populated_count) {
            state->listbox.selected_index = keep;
        }
    }

    if (fl_ctrl_pressed() && fl_is_key_pressed('L')) {
        pk_cycle_sort();
        fl_populate_listbox(&state->listbox, NULL, g_pk_items, g_pk_count);
    }

    fl_draw_text(.text = "memory", .x = PK_COL_MEM, .y = 8, .fontsize = 13,
                 .align = "LEFT", .color = RGB(130, 130, 160));
    fl_draw_text(.text = "cpu", .x = PK_COL_CPU, .y = 8, .fontsize = 13,
                 .align = "LEFT", .color = RGB(130, 130, 160));
    char sort_label[64];
    snprintf(sort_label, sizeof(sort_label), "current sort: %s", pk_sort_label(g_pk_sort));
    fl_draw_text(.text = sort_label, .x = 880, .y = 8, .fontsize = 13,
                 .align = "RIGHT", .color = RGB(160, 160, 190));

    ListBoxInfo* lb = &state->listbox;
    FlListSpec spec = {
        .lb = lb,
        .item_height = fl_get_text_height(NULL) + 20,
    };
    fl_place_editbox();
    if (!fl_listbox_begin(&spec)) return;

    FlListItemCtx ctx;
    while (fl_listbox_next(&ctx)) {
        char name[PK_NAME_LEN];
        char mem[32];
        char cpu[24];
        pk_split_row(ctx.text ? ctx.text : "", name, sizeof(name), mem, sizeof(mem), cpu, sizeof(cpu));
        if (name[0] == '\0') continue;

        fl_draw_text(.text = name, .x = ctx.x + 14, .y = ctx.y + 4,
                     .align = "LEFT", .pad = ctx.x + 14);
        fl_draw_text(.text = mem, .x = PK_COL_MEM, .y = ctx.y + 4, .align = "LEFT");
        fl_draw_text(.text = cpu, .x = PK_COL_CPU, .y = ctx.y + 4, .align = "LEFT");
    }
    fl_listbox_end();
}

static void TriggerProcessKiller(void) {
    pk_collect_and_sort();
    PROMPT(.title = "💀 ProcessKiller",
           .refresh = pk_refresh_callback,
           .enter = pk_enter_callback,
           .render = pk_render_callback);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+U", TriggerProcessKiller,"Fortnite");
    fl_register_hotkey("Alt+Shift+P", bulk_kill_bkp);

    fl_register_feature(.name="Process Killer", .callback=TriggerProcessKiller);
}
>>>>>>> 408a6905 (removed some dead functions)
