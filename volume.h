#ifndef VOLUME_H
#define VOLUME_H

#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include "config.h"
#include "types.h"
#include <stdlib.h>
#include <stdio.h>
#include <initguid.h>

DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xbcde0395, 0xe52f, 0x467c, 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e);
DEFINE_GUID(IID_IMMDeviceEnumerator, 0xa95664d2, 0x9614, 0x4f35, 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6);
DEFINE_GUID(IID_IAudioEndpointVolume, 0x5cdf2c82, 0x841e, 0x4546, 0x97, 0x22, 0x0c, 0xf7, 0x40, 0x78, 0x22, 0x9a);

// ============================================================
// GET CURRENT SYSTEM VOLUME (Returns 0 to 100)
// ============================================================
int GetSystemVolume() {
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

    return (int)(currentVolume * 100.0f + 0.5f); // Rounding up
}

// ============================================================
// SET SYSTEM VOLUME (0.0 = mute, 1.0 = max)
// ============================================================
void SetSystemVolume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    
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

// ============================================================
// TRIGGER VOLUME POPUP
// ============================================================
void TriggerVolume() {
  
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    
    // 1. Get current percentage
    int current_vol = GetSystemVolume();
    
    // 2. Format a dynamic string buffer
    char volume_prompt_buffer[64];
    snprintf(volume_prompt_buffer, sizeof(volume_prompt_buffer), "Enter volume (0-100) | Current: %d", current_vol);
    
    // 3. Pass the buffer into the prompt array
    const char* prompt[1];
    prompt[0] = volume_prompt_buffer;
    
    char* input = PromptUser(prompt, "🔊 Volume", 1, 220, 150, 32, NULL, NULL, NULL, NULL, NULL);
    
    if (input != NULL && strlen(input) > 0) {
        int volume_int = atoi(input);
        if (volume_int < 0) volume_int = 0;
        if (volume_int > 100) volume_int = 100;
        float volume = volume_int / 100.0f;
        SetSystemVolume(volume);
    }
}

#endif // VOLUME_H
