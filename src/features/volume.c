#if defined(_WIN32)
#define INITGUID
#endif
#include "core/flapi.h"

#if defined(_WIN32)
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#endif

#if defined(_WIN32)
int GetSystemVolume(void) {
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    IAudioEndpointVolume *pVolume = NULL;
    float currentVolume = 0.0f;

    hr = CoInitialize(NULL);
    if (FAILED(hr)) return 0;

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, (void**)&pEnumerator);
    if (FAILED(hr)) goto cleanup;

    hr = pEnumerator->lpVtbl->GetDefaultAudioEndpoint(pEnumerator, eRender, eConsole, &pDevice);
    if (FAILED(hr)) goto cleanup;

    hr = pDevice->lpVtbl->Activate(pDevice, &IID_IAudioEndpointVolume, CLSCTX_ALL, NULL,
                                   (void**)&pVolume);
    if (FAILED(hr)) goto cleanup;

    pVolume->lpVtbl->GetMasterVolumeLevelScalar(pVolume, &currentVolume);

cleanup:
    if (pVolume) pVolume->lpVtbl->Release(pVolume);
    if (pDevice) pDevice->lpVtbl->Release(pDevice);
    if (pEnumerator) pEnumerator->lpVtbl->Release(pEnumerator);
    CoUninitialize();

    return (int)(currentVolume * 100.0f + 0.5f);
}

void SetSystemVolume(float volume) {
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    IAudioEndpointVolume *pVolume = NULL;

    hr = CoInitialize(NULL);
    if (FAILED(hr)) return;

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, (void**)&pEnumerator);
    if (FAILED(hr)) goto cleanup;

    hr = pEnumerator->lpVtbl->GetDefaultAudioEndpoint(pEnumerator, eRender, eConsole, &pDevice);
    if (FAILED(hr)) goto cleanup;

    hr = pDevice->lpVtbl->Activate(pDevice, &IID_IAudioEndpointVolume, CLSCTX_ALL, NULL,
                                   (void**)&pVolume);
    if (FAILED(hr)) goto cleanup;

    pVolume->lpVtbl->SetMasterVolumeLevelScalar(pVolume, volume, NULL);

cleanup:
    if (pVolume) pVolume->lpVtbl->Release(pVolume);
    if (pDevice) pDevice->lpVtbl->Release(pDevice);
    if (pEnumerator) pEnumerator->lpVtbl->Release(pEnumerator);
    CoUninitialize();
}

#else

int GetSystemVolume(void) {
    const char* real_user = Platform_GetRealUser();
    const char* display = Platform_GetSessionDisplay();

    char rt_buf[256];
    snprintf(rt_buf, sizeof(rt_buf), "%s", Platform_GetRuntimeDir());
    const char* runtime_dir = rt_buf;

    char cmd[512];
    if (Platform_IsRunningAsRoot()) {
        snprintf(cmd, sizeof(cmd), "sudo -u %s DISPLAY=%s XDG_RUNTIME_DIR=%s wpctl get-volume @DEFAULT_AUDIO_SINK@", 
                 real_user, display, runtime_dir);
    } else {
        snprintf(cmd, sizeof(cmd), "DISPLAY=%s XDG_RUNTIME_DIR=%s wpctl get-volume @DEFAULT_AUDIO_SINK@", 
                 display, runtime_dir);
    }

    char* out = fl_run_terminal_command(cmd);
    if (!out) return 0;
    float vol = 0.0f;
    int ret = 0;
    if (sscanf(out, "Volume: %f", &vol) == 1) ret = (int)(vol * 100.0f + 0.5f);
    return ret;
}

void SetSystemVolume(float volume) {
    const char* real_user = Platform_GetRealUser();
    const char* display = Platform_GetSessionDisplay();

    char rt_buf[256];
    snprintf(rt_buf, sizeof(rt_buf), "%s", Platform_GetRuntimeDir());
    const char* runtime_dir = rt_buf;

    char cmd[512];
    if (Platform_IsRunningAsRoot()) {
        snprintf(cmd, sizeof(cmd), "sudo -u %s DISPLAY=%s XDG_RUNTIME_DIR=%s wpctl set-volume @DEFAULT_AUDIO_SINK%s %.2f", 
                 real_user, display, runtime_dir, "%", volume);
    } else {
        snprintf(cmd, sizeof(cmd), "DISPLAY=%s XDG_RUNTIME_DIR=%s wpctl set-volume @DEFAULT_AUDIO_SINK%s %.2f", 
                 display, runtime_dir, "%", volume);
    }

    fl_run_terminal_command(cmd);
}

#endif

void TriggerVolume(void) {
    int current_vol = GetSystemVolume();
    char prompt[256]; 
    char* input;
    snprintf(prompt, sizeof(prompt), "Enter Volume (0 - 100) Current Volume: %d", current_vol);
    
    input = SIMPLE_PROMPT(.prompt = prompt);

    if (input != NULL) {
        if (strcmp(input, "mx") == 0) {
            SetSystemVolume(1.0f);
        } else if (strcmp(input, "mn") == 0) {
            SetSystemVolume(0.0f);
        } else if (strcmp(input, "md") == 0) {
            SetSystemVolume(0.5f);
        } else if (atoi(input) > 100) {
            SubjectiveMessageDisplay("VOLUME WAS MORE THAN 100 THAT IS NOT ALLOWED", 140, TimeOutForMessages, COLOR_ERROR);
        } else if (atoi(input) < 0) {
            SubjectiveMessageDisplay("VOLUME WAS LESS THAN 0 THAT IS NOT ALLOWED", 140, TimeOutForMessages, COLOR_ERROR);
        } else {
            int vol = atoi(input);
            SetSystemVolume(vol / 100.0f);
        }
    }
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Alt+N", TriggerVolume,"Fortnite");
    fl_register_feature("Volume", TriggerVolume);
}
