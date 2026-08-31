#ifndef MUTE_MICROPHONE_H
#define MUTE_MICROPHONE_H

#include <stdbool.h>

extern bool mic_muted;

void TriggerMuteState(void);
void mute_microphone_init(void);

#if defined(_WIN32)
#ifndef COBJMACROS
#define COBJMACROS
#endif

#ifndef CINTERFACE
#define CINTERFACE
#endif

#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>

HRESULT SetDeviceMute(EDataFlow flow, BOOL mute);
#endif

#endif
