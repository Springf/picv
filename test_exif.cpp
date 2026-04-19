#include <windows.h>
#include <iostream>
#include <string>
#include <propkey.h>
#include <propvarutil.h>
#include <shobjidl.h>
#include <propsys.h>

#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "ole32.lib")

int wmain(int argc, wchar_t* argv[]) {
    CoInitialize(NULL);
    if (argc < 2) return 1;

    IPropertyStore* pStore = NULL;
    if (SUCCEEDED(SHGetPropertyStoreFromParsingName(argv[1], NULL, GPS_DEFAULT, IID_PPV_ARGS(&pStore)))) {
        PROPVARIANT prop;
        PropVariantInit(&prop);

        auto PrintProp = [&](PROPERTYKEY key, const wchar_t* name) {
            if (SUCCEEDED(pStore->GetValue(key, &prop))) {
                PWSTR displayStr = NULL;
                if (SUCCEEDED(PSFormatForDisplayAlloc(key, prop, PDFF_DEFAULT, &displayStr))) {
                    std::wcout << name << L": " << displayStr << std::endl;
                    CoTaskMemFree(displayStr);
                } else {
                    std::wcout << name << L": [Could not format]" << std::endl;
                }
                PropVariantClear(&prop);
            } else {
                std::wcout << name << L": N/A" << std::endl;
            }
        };

        PrintProp(PKEY_Photo_CameraModel, L"Model");
        PrintProp(PKEY_Photo_DateTaken, L"Date");
        PrintProp(PKEY_Photo_ISOSpeed, L"ISO");
        PrintProp(PKEY_Photo_FNumber, L"F-Stop");
        PrintProp(PKEY_Photo_ExposureTime, L"Exposure");
        PrintProp(PKEY_Photo_FocalLength, L"Focal Length");
        PrintProp(PKEY_Image_Dimensions, L"Dimensions");

        pStore->Release();
    }
    CoUninitialize();
    return 0;
}
