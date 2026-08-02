#ifndef COBJMACROS
#define COBJMACROS
#endif

#ifndef CINTERFACE
#define CINTERFACE
#endif

#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <stdio.h>
#include "types.h"
#include "helpers.h"

#pragma comment(lib, "ole32.lib")

bool mic_muted = false;

HRESULT SetDeviceMute(EDataFlow flow, BOOL mute) {
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    IAudioEndpointVolume *pEndpointVolume = NULL;

    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return hr;

    hr = CoCreateInstance(
        &CLSID_MMDeviceEnumerator, 
        NULL, 
        CLSCTX_INPROC_SERVER, 
        &IID_IMMDeviceEnumerator, 
        (void**)&pEnumerator
    );
    if (FAILED(hr)) goto Cleanup;

    hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(
        pEnumerator, 
        flow, 
        eConsole, 
        &pDevice
    );
    if (FAILED(hr)) goto Cleanup;

    hr = IMMDevice_Activate(
        pDevice, 
        &IID_IAudioEndpointVolume, 
        CLSCTX_INPROC_SERVER, 
        NULL, 
        (void**)&pEndpointVolume
    );
    if (FAILED(hr)) goto Cleanup;

    hr = IAudioEndpointVolume_SetMute(pEndpointVolume, mute, NULL);

Cleanup:
    if (pEndpointVolume) IAudioEndpointVolume_Release(pEndpointVolume);
    if (pDevice) IMMDevice_Release(pDevice);
    if (pEnumerator) IMMDeviceEnumerator_Release(pEnumerator);

    CoUninitialize();
    return hr;
}

void TriggerMuteState(void) {
    if (CheckAndBlockIfPopupActive(hPopupWnd)) return;
    if (!mic_muted){
        if (SUCCEEDED(SetDeviceMute(eCapture, TRUE))) {
          mic_muted = true;
          DisplayMessageText = RGB(255,80,20);
          MSG_MIDDLE_BOTTOM("Mic Muted",70,1);
          DisplayMessageText = COLOR_TEXT;
        }
    } else {
        if (SUCCEEDED(SetDeviceMute(eCapture, FALSE))) {
            mic_muted = false;
            DisplayMessageText = RGB(80,255,20);
            MSG_MIDDLE_BOTTOM("Mic Unmuted",70,1);
            DisplayMessageText = COLOR_TEXT;
        }
    }
}
