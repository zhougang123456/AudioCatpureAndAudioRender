#include <mmdeviceapi.h>
#include <audioclient.h>
#include <cstdio>
#include "atlconv.h"

/* This was only added to MinGW in ~2015 and our Cerbero toolchain is too old */
#if defined(_MSC_VER)
#include <functiondiscoverykeys_devpkey.h>
#elif !defined(PKEY_Device_FriendlyName)
#include <initguid.h>
#include <propkey.h>
DEFINE_PROPERTYKEY(PKEY_Device_FriendlyName, 0xa45c254e, 0xdf1c, 0x4efd, 0x80,
    0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);
DEFINE_PROPERTYKEY(PKEY_AudioEngine_DeviceFormat, 0xf19f064d, 0x82c, 0x4e27,
    0xbc, 0x73, 0x68, 0x82, 0xa1, 0xbb, 0x8e, 0x4c, 0);
#endif

const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IMMEndpoint = __uuidof(IMMEndpoint);

static int update_devices(IMMDeviceEnumerator* enumerator);

class ClinkIMMNotificationClient : public IMMNotificationClient
{
public:
    static HRESULT
        CreateInstance(IMMDeviceEnumerator* enumerator, IMMNotificationClient** client)
    {

        ClinkIMMNotificationClient* self;
        self = new ClinkIMMNotificationClient();
        self->enumerator_ = enumerator;
        *client = (IMMNotificationClient*)self;
        return S_OK;
    }

    /* IUnknown */
    STDMETHODIMP
        QueryInterface(REFIID riid, void** object)
    {
        if (!object)
            return E_POINTER;

        if (riid == IID_IUnknown) {
            *object = static_cast<IUnknown*> (this);
        }
        else if (riid == __uuidof(IMMNotificationClient)) {
            *object = static_cast<IMMNotificationClient*> (this);
        }
        else {
            *object = nullptr;
            return E_NOINTERFACE;
        }

        AddRef();

        return S_OK;
    }

    STDMETHODIMP_(ULONG)
        AddRef(void)
    {
        return InterlockedIncrement(&ref_count_);
    }

    STDMETHODIMP_(ULONG)
        Release(void)
    {
        ULONG ref_count;

        printf("%d", ref_count_);
        ref_count = InterlockedDecrement(&ref_count_);

        if (ref_count == 0) {
            delete this;
        }

        return ref_count;
    }

    /* IMMNotificationClient */
    STDMETHODIMP
        OnDeviceStateChanged(LPCWSTR device_id, DWORD new_state)
    {
        HRESULT hr;
        printf("device change\n");
        update_devices(enumerator_);
        return S_OK;
    }

    STDMETHODIMP
        OnDeviceAdded(LPCWSTR device_id)
    {
        HRESULT hr; 
        printf("device add\n");
        return S_OK;
    }

    STDMETHODIMP
        OnDeviceRemoved(LPCWSTR device_id)
    {
        HRESULT hr;
        printf("device remove\n");
        return S_OK;
    }

    STDMETHODIMP
        OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR default_device_id)
    {
        HRESULT hr;
        printf("default device change\n");
        return S_OK;
    }

    STDMETHODIMP
        OnPropertyValueChanged(LPCWSTR device_id, const PROPERTYKEY key)
    {
        printf("device value change\n");
        return S_OK;
    }

private:
    ClinkIMMNotificationClient()
        : ref_count_(1)
    {
    }

    virtual ~ClinkIMMNotificationClient()
    {
        printf("free ClinkIMMNotificationClient\n");
        printf("free ClinkIMMNotificationClient\n");
    }

private:
    ULONG ref_count_;
    IMMDeviceEnumerator* enumerator_;
};

static char* wchar_to_char(wchar_t* wchar)
{
    USES_CONVERSION;
    return W2A(wchar);
}

static int update_devices(IMMDeviceEnumerator* enumerator)
{
    BOOLEAN active = TRUE;
    DWORD dwStateMask = active ? DEVICE_STATE_ACTIVE : DEVICE_STATEMASK_ALL;
    IMMDeviceCollection* device_collection = NULL;
    int ii;
    UINT count;

    HRESULT hr = enumerator->EnumAudioEndpoints(eAll, dwStateMask,
        &device_collection);

    if (FAILED(hr)) {
        printf("Failed to EnumAudioEndpoints\n");
        return -1;
    }

    hr = device_collection->GetCount(&count);

    if (FAILED(hr)) {
        printf("Failed to GetCount\n");
        return -1;
    }

    /* Create a GList of GstDevices* to return */
    for (ii = 0; ii < count; ii++) {
        IMMDevice* item = NULL;
        IMMEndpoint* endpoint = NULL;
        IAudioClient* client = NULL;
        IPropertyStore* prop_store = NULL;
        WAVEFORMATEX* format = NULL;
        char* description = NULL;
        char* strid = NULL;
        EDataFlow dataflow;
        PROPVARIANT var;
        wchar_t* wstrid;
        int len;

        hr = device_collection->Item(ii, &item);
        if (hr != S_OK)
            continue;
        hr = item->QueryInterface(IID_IMMEndpoint, (void**)&endpoint);
        if (hr != S_OK) {
            printf("query interface failed\n");
            return -1;
        }

        hr = endpoint->GetDataFlow(&dataflow);
        if (hr != S_OK) {
            printf("get data flow failed\n");
            return -1;
        }

        if (dataflow == eRender) {
            printf("wasapi sink\n");
        }
        else {
            printf("wasapi src\n");
        }

        PropVariantInit(&var);

        hr = item->GetId(&wstrid);
        if (hr != S_OK) {
            printf("get data flow failed\n");
            return -1;
        }

        len = wcslen(wstrid);
        strid = (char*)malloc(sizeof(char) * (len + 1));
        memcpy(strid, wchar_to_char(wstrid), len);
        strid[len] = '\0';

        CoTaskMemFree(wstrid);

        hr = item->OpenPropertyStore(STGM_READ, &prop_store);
        if (hr != S_OK) {
            printf("open property store failed\n");
            return -1;
        }

        /* NOTE: More properties can be added as needed from here:
         * https://msdn.microsoft.com/en-us/library/windows/desktop/dd370794(v=vs.85).aspx */
        hr = prop_store->GetValue(PKEY_Device_FriendlyName, &var);
        if (hr != S_OK) {
            printf("prop store get value failed\n");
            return -1;
        }

        len = strlen(wchar_to_char(var.pwszVal));
        description = (char*)malloc(sizeof(char) * (len + 1));
        memcpy(description, wchar_to_char(var.pwszVal), len);
        description[len] = '\0';
        PropVariantClear(&var);

        /* Get the audio client so we can fetch the mix format for shared mode
         * to get the device format for exclusive mode (or something close to that)
         * fetch PKEY_AudioEngine_DeviceFormat from the property store. */
        hr = item->Activate(IID_IAudioClient, CLSCTX_ALL, NULL,
            (void**)&client);
        if (hr != S_OK) {
            printf("IMMDevice::Activate (IID_IAudioClient) failed"
                "on %s\n", strid);
            goto next;
        }

        hr = client->GetMixFormat(&format);
        if (hr != S_OK || format == NULL) {
            printf("GetMixFormat failed on %s\n", strid);
            goto next;
        }

        printf("wasapi strid: %s description: %s\n", strid, description);

    next:
        PropVariantClear(&var);
        if (prop_store)
            (prop_store)->Release();
        if (endpoint)
            endpoint->Release();
        if (client)
            client->Release();
        if (item)
            item->Release();
        if (format)
            CoTaskMemFree(format);
        if (description)
            free(description);
        if (strid)
            free(strid);
    }
    if (device_collection)
        device_collection->Release();
}
int main()
{
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoCreateInstance(__uuidof (MMDeviceEnumerator),
        nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    if (FAILED(hr)) {
        printf("Failed to create IMMDeviceEnumerator instance\n");
        return -1;
    }

    update_devices(enumerator);

    IMMNotificationClient* client;
    hr = ClinkIMMNotificationClient::CreateInstance(enumerator, &client);
    enumerator->RegisterEndpointNotificationCallback(client);
    
    while (1) {

    }

    if (client && enumerator) {
        enumerator->UnregisterEndpointNotificationCallback(client);
        client->Release();
    }
    if (enumerator)
        enumerator->Release();
   
    return 0;
}