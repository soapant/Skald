#include <windows.h>
#include <stdio.h>

// Global counter for key presses
int key_count = 0;
LARGE_INTEGER last_key_time;
LARGE_INTEGER start_time;
BOOL first_key = TRUE;

LRESULT CALLBACK TestKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        KBDLLHOOKSTRUCT* pKeyInfo = (KBDLLHOOKSTRUCT*)lParam;
        key_count++;
        QueryPerformanceCounter(&last_key_time);
        
        // Calculate time since first key
        if (first_key) {
            first_key = FALSE;
            QueryPerformanceCounter(&start_time);
        }
        
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        double elapsed_ms = ((double)(last_key_time.QuadPart - start_time.QuadPart) / freq.QuadPart) * 1000.0;
        
        printf("[KEY] VK: 0x%X (%c), Count: %d, Elapsed: %.1f ms\n", 
               pKeyInfo->vkCode, pKeyInfo->vkCode >= 0x20 && pKeyInfo->vkCode <= 0x7E ? pKeyInfo->vkCode : '?', 
               key_count, elapsed_ms);
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int main() {
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    
    HHOOK hook = SetWindowsHookEx(WH_KEYBOARD_LL, TestKeyboardProc, GetModuleHandle(NULL), 0);
    if (!hook) {
        printf("Failed to install hook\n");
        return 1;
    }
    
    printf("Hook installed. Press keys to test.\n");
    printf("Press Escape to exit.\n\n");
    
    QueryPerformanceCounter(&start);
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            break;
        }
    }
    
    QueryPerformanceCounter(&end);
    UnhookWindowsHookEx(hook);
    
    double elapsed_ms = ((double)(end.QuadPart - start.QuadPart) / freq.QuadPart) * 1000.0;
    double avg_ms_per_key = key_count > 0 ? elapsed_ms / key_count : 0;
    double keys_per_second = key_count > 0 ? key_count / (elapsed_ms / 1000.0) : 0;
    
    printf("\n--- Results ---\n");
    printf("Total keys: %d\n", key_count);
    printf("Total time: %.2f ms\n", elapsed_ms);
    printf("Average: %.3f ms per key\n", avg_ms_per_key);
    printf("Rate: %.1f keys/second\n", keys_per_second);
    
    return 0;
}
