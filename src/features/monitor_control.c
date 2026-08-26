#if defined(_WIN32)
#include <windows.h>
#include <stdlib.h>
#include <highlevelmonitorconfigurationapi.h>
#endif
#include "common.h"
#include "core/common_ui.h"
#include "features/monitor_control.h"

void SetMonitorBright(int brightness) {
#if defined(_WIN32)
    HMONITOR hMonitor = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
    if (!hMonitor) return;

    DWORD numMonitors = 0;
    if (!GetNumberOfPhysicalMonitorsFromHMONITOR(hMonitor, &numMonitors) || numMonitors == 0) {
        return;
    }

    LPPHYSICAL_MONITOR pMonitors = (LPPHYSICAL_MONITOR)malloc(numMonitors * sizeof(PHYSICAL_MONITOR));
    if (!pMonitors) return;

    if (GetPhysicalMonitorsFromHMONITOR(hMonitor, numMonitors, pMonitors)) {
        if (brightness < 0) brightness = 0;
        if (brightness > 100) brightness = 100;

        SetMonitorBrightness(pMonitors[0].hPhysicalMonitor, (DWORD)brightness);

        DestroyPhysicalMonitors(numMonitors, pMonitors);
    }

    free(pMonitors);
#else
    if (brightness < 0) brightness = 0;
    if (brightness > 100) brightness = 100;

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "brightnessctl set %d%% > /dev/null 2>&1", brightness);
    
    int ret = system(cmd);
    if (ret != 0) {
        snprintf(cmd, sizeof(cmd), "ddcutil setvcp 10 %d > /dev/null 2>&1", brightness);
        system(cmd);
    }
#endif
}

void TriggerMonitorControl(void) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;

    const char* prompt = "Enter Brightness(1, 100)";
    char* input = SIMPLE_PROMPT(.prompt = prompt, .title = "⏱️ MonitorControl",
                                .height = 920, .width = 650, .item_height = 32);

    if (input && input[0] != '\0') {
        SetMonitorBright(atoi(input));
    }
}
