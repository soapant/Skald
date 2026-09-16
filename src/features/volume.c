// #if defined(_WIN32)
// #define INITGUID
// #endif
// #include "core/flapi.h"
//
// #if defined(_WIN32)
// #include <mmdeviceapi.h>
// #include <endpointvolume.h>
// #endif
//
// void SetSystemVolume(float volume) {
//     const char* real_user = Platform_GetRealUser();
//     const char* display = Platform_GetSessionDisplay();
//
//     char rt_buf[256];
//     snprintf(rt_buf, sizeof(rt_buf), "%s", Platform_GetRuntimeDir());
//     const char* runtime_dir = rt_buf;
//
//     char cmd[512];
//     if (Platform_IsRunningAsRoot()) {
//         snprintf(cmd, sizeof(cmd), "sudo -u %s DISPLAY=%s XDG_RUNTIME_DIR=%s wpctl set-volume @DEFAULT_AUDIO_SINK%s %.2f", 
//                  real_user, display, runtime_dir, "%", volume);
//     } else {
//         snprintf(cmd, sizeof(cmd), "DISPLAY=%s XDG_RUNTIME_DIR=%s wpctl set-volume @DEFAULT_AUDIO_SINK%s %.2f", 
//                  display, runtime_dir, "%", volume);
//     }
//
//     fl_run_terminal_command(cmd);
// }
//
// void TriggerVolume(void) {
//     int current_vol = GetSystemVolume();
//     char prompt[256]; 
//     char* input;
//     snprintf(prompt, sizeof(prompt), "Enter Volume (0 - 100) Current Volume: %d", current_vol);
//
//     input = SIMPLE_PROMPT(.prompt = prompt);
//
//     if (input != NULL) {
//         if (strcmp(input, "mx") == 0) {
//             SetSystemVolume(1.0f);
//         } else if (strcmp(input, "mn") == 0) {
//             SetSystemVolume(0.0f);
//         } else if (strcmp(input, "md") == 0) {
//             SetSystemVolume(0.5f);
//         } else if (atoi(input) > 100) {
//             SubjectiveMessageDisplay("VOLUME WAS MORE THAN 100 THAT IS NOT ALLOWED", 140, TimeOutForMessages, COLOR_ERROR);
//         } else if (atoi(input) < 0) {
//             SubjectiveMessageDisplay("VOLUME WAS LESS THAN 0 THAT IS NOT ALLOWED", 140, TimeOutForMessages, COLOR_ERROR);
//         } else {
//             int vol = atoi(input);
//             SetSystemVolume(vol / 100.0f);
//         }
//     }
// }
//
// FLINT_FEATURE_INIT {
//     fl_register_hotkey("Alt+N", TriggerVolume,"Fortnite");
//     fl_register_feature("Volume", TriggerVolume);
// }
