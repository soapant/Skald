#ifndef MUTE_MICROPHONE_H
#define MUTE_MICROPHONE_H

#include "common.h"
#include "core/flapi.h"
#include "core/platform.h"
#include "core/common_ui.h"

#if defined(_WIN32)
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <endpointvolume.h>
#endif

bool mic_muted = false;

#if defined(_WIN32)
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
#endif

void TriggerMuteState(void) {
#if defined(_WIN32)
    if (!mic_muted){
        if (SUCCEEDED(SetDeviceMute(eCapture, TRUE))) {
          mic_muted = true;
          MSG_MIDDLE_BOTTOM("Mic Muted",70,TimeOutForMessages,RGB(255,80,20));
        }
    } else {
        if (SUCCEEDED(SetDeviceMute(eCapture, FALSE))) {
            mic_muted = false;
            MSG_MIDDLE_BOTTOM("Mic Unmuted",70,TimeOutForMessages,RGB(80,255,20));
        }
    }
#else
    // Not Implemented
#endif
}

static inline void mute_microphone_init(void) {
    fl_register_hotkey("Alt+Shift+P", TriggerMuteState);
}

#endif
