#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <stdio.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")

// Define our own GUIDs (these match the Windows SDK ones)
__declspec(selectany) const CLSID CLSID_MMDeviceEnumerator = {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
__declspec(selectany) const IID IID_IMMDeviceEnumerator = {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
__declspec(selectany) const PROPERTYKEY PKEY_Device_FriendlyName = { {0xA45C254E, 0xDF1C, 0x4EFD, {0x80, 0x20, 0x67, 0xD1, 0x46, 0xA8, 0x50, 0xE0}}, 14 };

void ListAudioDevices(void) {
    HRESULT hr;
    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDeviceCollection* pDevices = NULL;
    IMMDevice* pDevice = NULL;
    IPropertyStore* pProps = NULL;
    PROPVARIANT varName;
    LPWSTR pwszID = NULL;
    UINT count = 0;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                          &IID_IMMDeviceEnumerator, (void**)&pEnumerator);
    if (FAILED(hr)) {
        printf("Failed to create device enumerator. Error: 0x%08lx\n", hr);
        CoUninitialize();
        return;
    }

    IMMDeviceEnumeratorVtbl* enumVtbl = pEnumerator->lpVtbl;
    hr = enumVtbl->EnumAudioEndpoints(pEnumerator, eRender, DEVICE_STATE_ACTIVE, &pDevices);
    if (FAILED(hr)) {
        printf("Failed to enumerate devices. Error: 0x%08lx\n", hr);
        enumVtbl->Release(pEnumerator);
        CoUninitialize();
        return;
    }

    IMMDeviceCollectionVtbl* collVtbl = pDevices->lpVtbl;
    hr = collVtbl->GetCount(pDevices, &count);
    if (FAILED(hr)) {
        printf("Failed to get device count. Error: 0x%08lx\n", hr);
        collVtbl->Release(pDevices);
        enumVtbl->Release(pEnumerator);
        CoUninitialize();
        return;
    }

    printf("\n=== Audio Output Devices (%d found) ===\n\n", count);

    for (UINT i = 0; i < count; i++) {
        hr = collVtbl->Item(pDevices, i, &pDevice);
        if (FAILED(hr)) continue;

        IMMDeviceVtbl* devVtbl = pDevice->lpVtbl;

        printf("[%d]\n", i);

        hr = devVtbl->GetId(pDevice, &pwszID);
        if (SUCCEEDED(hr)) {
            printf("  ID: %ls\n", pwszID);
            CoTaskMemFree(pwszID);
            pwszID = NULL;
        }

        hr = devVtbl->OpenPropertyStore(pDevice, STGM_READ, &pProps);
        if (SUCCEEDED(hr)) {
            IPropertyStoreVtbl* propsVtbl = pProps->lpVtbl;
            PropVariantInit(&varName);
            hr = propsVtbl->GetValue(pProps, &PKEY_Device_FriendlyName, &varName);
            if (SUCCEEDED(hr)) {
                printf("  Name: %ls\n", varName.pwszVal);
            }
            PropVariantClear(&varName);
            propsVtbl->Release(pProps);
            pProps = NULL;
        }

        printf("\n");
        devVtbl->Release(pDevice);
        pDevice = NULL;
    }

    collVtbl->Release(pDevices);
    enumVtbl->Release(pEnumerator);
    CoUninitialize();
}

int main(void) {
    ListAudioDevices();
    printf("Press Enter to exit...");
    getchar();
    return 0;
}
