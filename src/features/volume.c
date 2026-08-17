#include "common.h"
#include "features/volume.h"
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <initguid.h>
#include "core/common_ui.h"

DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xbcde0395, 0xe52f, 0x467c, 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e);
DEFINE_GUID(IID_IMMDeviceEnumerator, 0xa95664d2, 0x9614, 0x4f35, 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6);
DEFINE_GUID(IID_IAudioEndpointVolume, 0x5cdf2c82, 0x841e, 0x4546, 0x97, 0x22, 0x0c, 0xf7, 0x40, 0x78, 0x22, 0x9a);

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

    return (int)(currentVolume * 100.0f + 0.5f); // Rounding up
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


void TriggerVolume(void) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    int current_vol = GetSystemVolume();
    char prompt[256]; 
    char* input;
    snprintf(prompt, sizeof(prompt), "Enter Volume (0 - 100) Current Volume: %d", current_vol);
    
    input = SimplePromptUser(prompt, "🔊 Volume",920, 650, 32);

    if (input != NULL) {
    if (strcmp(input,"mx") == 0) {
        SetSystemVolume(1.0);
    } else if (strcmp(input,"mn") == 0) {
        SetSystemVolume(0.0);
    } else if (strcmp(input,"md") == 0) {
        SetSystemVolume(0.5);
    } else if (atoi(input) > 100) {
      SubjectiveMessageDisplay("VOLUME WAS MORE THAN 100 THAT IS NOT ALLOWED",140,TimeOutForMessages,COLOR_ERROR);
    } else if (atoi(input) < 0) {
      SubjectiveMessageDisplay("VOLUME WAS LESS THAN 0 THAT IS NOT ALLOWED",140,TimeOutForMessages,COLOR_ERROR);
    } else {
        int vol = atoi(input);
        SetSystemVolume(vol / 100.0f);
    }
    }
}
