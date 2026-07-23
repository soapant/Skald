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


void TriggerVolume() {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    int current_vol = GetSystemVolume();
    char prompt[256]; 
    char* input;
    snprintf(prompt, sizeof(prompt), "Enter Volume (0 - 100) Current Volume: %d", current_vol);
    
    input = SimplePromptUser(prompt, "🔊 Volume",920, 650, 32);
    if (input != NULL) {
    int vol = atoi(input);
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    SetSystemVolume(vol / 100.0f);
    }
    
}

#endif 
