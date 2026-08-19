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
    (void)brightness;
#endif
}

void TriggerMonitorControl(void) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;

    const char* prompt = "Enter Brightness(1, 100)";
    char* input = Platform_SimplePromptUser(prompt, "⏱️ MonitorControl", 920, 650, 32);

    if (input && input[0] != '\0') {
        SetMonitorBright(atoi(input));
    }
}