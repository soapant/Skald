// volume.h
#ifndef VOLUME_H
#define VOLUME_H

#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include "config.h"
#include "types.h"
#include <stdlib.h>
#include <stdio.h>
#include <initguid.h>  // <-- ADD THIS - forces GUID definitions

// ============================================================
// FORCE GUID DEFINITIONS
// ============================================================
DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xbcde0395, 0xe52f, 0x467c, 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e);
DEFINE_GUID(IID_IMMDeviceEnumerator, 0xa95664d2, 0x9614, 0x4f35, 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6);
DEFINE_GUID(IID_IAudioEndpointVolume, 0x5cdf2c82, 0x841e, 0x4546, 0x97, 0x22, 0x0c, 0xf7, 0x40, 0x78, 0x22, 0x9a);

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
    
    const char* prompt[1] = {"Enter volume (0-100)"};
    
    char* input = PromptUser(prompt, "🔊 Volume", 1, 220, 150, 32, NULL, NULL, NULL, NULL, NULL);
    
    if (input != NULL && strlen(input) > 0) {
        int volume_int = atoi(input);
        if (volume_int < 0) volume_int = 0;
        if (volume_int > 100) volume_int = 100;
        float volume = volume_int / 100.0f;
        SetSystemVolume(volume);
        printf("[VOLUME] Set to: %d%%\n", volume_int);
    }
}


#endif // VOLUME_H
