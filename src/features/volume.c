#include "core/flapi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>

const GUID flint_IID_IAudioEndpointVolume =
  { 0x5cdf2c82, 0x841e, 0x4546, { 0x97, 0x22, 0x0c, 0xf7, 0x40, 0x78, 0x22, 0x9a } };
#else
#include <unistd.h>

static void pulse_sink(void) {
    if (getenv("XDG_RUNTIME_DIR")) return;
    char runtime_dir[64];
    snprintf(runtime_dir, sizeof(runtime_dir), "/run/user/%d", (int)getuid());
    setenv("XDG_RUNTIME_DIR", runtime_dir, 1);
}

static int parse_percent(const char *text) {
    if (!text || !text[0]) return -1;
    const char *p = text;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            int value = 0;
            while (*p >= '0' && *p <= '9') { value = value * 10 + (*p - '0'); p++; }
            if (*p == '%') return value;
        } else {
            p++;
        }
    }
    return -1;
}

static int parse_fraction(const char *text) {
    if (!text || !text[0]) return -1;
    const char *slash = strchr(text, '/');
    if (!slash) return -1;
    const char *p = slash + 1;
    while (*p == ' ') p++;
    if (*p >= '0' && *p <= '9') {
        int value = 0;
        while (*p >= '0' && *p <= '9') { value = value * 10 + (*p - '0'); p++; }
        while (*p == ' ') p++;
        if (*p == '%') return value;
    }
    return -1;
}

static int run_parse(const char *cmd) {
    char *output = fl_run_terminal_command_blocking((char *)cmd);
    if (!output) return -1;
    int volume = parse_fraction(output);
    if (volume < 0) volume = parse_percent(output);
    free(output);
    return volume;
}
#endif

int get_current_volume(void) {
#if defined(_WIN32)
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    IAudioEndpointVolume *pVolume = NULL;
    float currentVolume = 0.0f;

    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (hr == RPC_E_CHANGED_MODE) {
    } else if (FAILED(hr)) {
        return 0;
    }

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, (void**)&pEnumerator);
    if (FAILED(hr)) goto cleanup;

    hr = pEnumerator->lpVtbl->GetDefaultAudioEndpoint(pEnumerator, eRender, eConsole, &pDevice);
    if (FAILED(hr)) goto cleanup;

    hr = pDevice->lpVtbl->Activate(pDevice, &flint_IID_IAudioEndpointVolume, CLSCTX_ALL, NULL,
                                   (void**)&pVolume);
    if (FAILED(hr)) goto cleanup;

    pVolume->lpVtbl->GetMasterVolumeLevelScalar(pVolume, &currentVolume);

cleanup:
    if (pVolume) pVolume->lpVtbl->Release(pVolume);
    if (pDevice) pDevice->lpVtbl->Release(pDevice);
    if (pEnumerator) pEnumerator->lpVtbl->Release(pEnumerator);
    CoUninitialize();

    return (int)(currentVolume * 100.0f + 0.5f);
#else
    pulse_sink();
    int volume = run_parse("pactl get-sink-volume @DEFAULT_SINK@");
    if (volume < 0) volume = run_parse("wpctl get-volume @DEFAULT_AUDIO_SINK@");
    return volume < 0 ? 0 : volume;
#endif
}

void set_volume(int volume) {
#if defined(_WIN32)
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    IAudioEndpointVolume *pVolume = NULL;

    hr = CoInitialize(NULL);
    if (FAILED(hr))  return;

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, (void**)&pEnumerator);
    if (FAILED(hr)) goto cleanup;

    hr = pEnumerator->lpVtbl->GetDefaultAudioEndpoint(pEnumerator, eRender, eConsole, &pDevice);
    if (FAILED(hr)) goto cleanup;

    hr = pDevice->lpVtbl->Activate(pDevice, &flint_IID_IAudioEndpointVolume, CLSCTX_ALL, NULL,
                                   (void**)&pVolume);
    if (FAILED(hr)) goto cleanup;

    float volume_float = (float)volume / 100.0f;

    pVolume->lpVtbl->SetMasterVolumeLevelScalar(pVolume, volume_float, NULL);

cleanup:
    if (pVolume) pVolume->lpVtbl->Release(pVolume);
    if (pDevice) pDevice->lpVtbl->Release(pDevice);
    if (pEnumerator) pEnumerator->lpVtbl->Release(pEnumerator);
    CoUninitialize();
#else
    pulse_sink();
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "pactl set-sink-volume @DEFAULT_SINK@ %d%%", volume);
    char *out = fl_run_terminal_command_blocking(cmd);
    bool ok = out && !out[0];
    if (out) free(out);
    if (!ok) {
        snprintf(cmd, sizeof(cmd), "wpctl set-volume @DEFAULT_AUDIO_SINK@ %d%%", volume);
        out = fl_run_terminal_command_blocking(cmd);
        if (out) free(out);
    }
    if (volume > 0) {
        out = fl_run_terminal_command_blocking("pactl set-sink-mute @DEFAULT_SINK@ 0");
        if (out) free(out);
    }
#endif
    return;
}

#define VK_MEDIA_PLAY_PAUSE 0xB3

void toggle_play_pause(void) {
    #if defined(_WIN32)
      INPUT input[2] = {0};

      input[0].type = INPUT_KEYBOARD;
      input[0].ki.wVk = VK_MEDIA_PLAY_PAUSE;

      input[1].type = INPUT_KEYBOARD;
      input[1].ki.wVk = VK_MEDIA_PLAY_PAUSE;
      input[1].ki.dwFlags = KEYEVENTF_KEYUP;

      SendInput(2, input, sizeof(INPUT));
    #else
      GError *error = NULL;
      GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
      if (!conn) { g_error_free(error); return; }

      gchar* bus_name = FindPlayerBusName(conn);
      if (!bus_name) { g_object_unref(conn); return; }

      GDBusProxy *proxy = g_dbus_proxy_new_sync(conn, G_DBUS_PROXY_FLAGS_NONE, NULL,
          bus_name, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", NULL, &error);
      
      if (proxy) {
          g_dbus_proxy_call_sync(proxy, "PlayPause", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
          g_object_unref(proxy);
      }

      if (error) g_error_free(error);
      g_free(bus_name);
      g_object_unref(conn);
    #endif
}

void increment_volume() {
    set_volume(get_current_volume() + 10);
}

void decrement_volume() {
    set_volume(get_current_volume() - 10);
}

FLINT_FEATURE_INIT {
    fl_register_hotkey("Ctrl+]",increment_volume);
    fl_register_hotkey("Ctrl+[",decrement_volume);
    fl_register_hotkey("RShift",toggle_play_pause); 
}
