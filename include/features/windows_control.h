#ifndef WINDOWS_CONTROL_H
#define WINDOWS_CONTROL_H

#include <stdbool.h>

bool IsWifiEnabled(void);
bool IsBluetoothEnabled(void);
void TriggerWifiToggle(void);
void TriggerBlueToothToggle(void);
void windows_control_init(void);

#endif
