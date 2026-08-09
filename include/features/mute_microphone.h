#ifndef MUTE_MICROPHONE_H
#define MUTE_MICROPHONE_H

#ifndef COBJMACROS
#define COBJMACROS
#endif

#ifndef CINTERFACE
#define CINTERFACE
#endif

#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <stdbool.h>

extern bool mic_muted;

HRESULT SetDeviceMute(EDataFlow flow, BOOL mute);
void TriggerMuteState(void);

#endif
