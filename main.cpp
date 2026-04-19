#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <propkey.h>
#include <propvarutil.h>
#include <propsys.h>
#include <shobjidl.h>
#include <shcore.h>
#include <shellapi.h>

#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <mutex>
#include <iostream>
#include <unordered_set>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "propsys.lib")

template<class Interface>
inline void SafeRelease(Interface** ppInterfaceToRelease) {
    if (*ppInterfaceToRelease != NULL) {
        (*ppInterfaceToRelease)->Release();
        (*ppInterfaceToRelease) = NULL;
    }
}

namespace fs = std::filesystem;

struct ImageEntry {
    std::wstring path;
    uint64_t sortTime; // Date taken or creation time
};

enum class WindowMode { Standard, Borderless, Fullscreen };
enum class ZoomMode { FitBoth, FitHeight, FitWidth, Native100, Custom };

class PicViewer {
public:
    PicViewer() : m_hwnd(NULL), m_mode(WindowMode::Standard), m_currentIndex(-1),
                  m_pD2DFactory(NULL), m_pRenderTarget(NULL), 
                  m_pWICFactory(NULL), m_pDWriteFactory(NULL), m_pTextFormat(NULL),
                  m_pBitmap(NULL), m_showExif(false), m_dpiX(96.0f), m_dpiY(96.0f),
                  m_zoomMode(ZoomMode::FitBoth), m_customZoom(1.0f) {
        ZeroMemory(&m_wpPrev, sizeof(m_wpPrev));
        m_wpPrev.length = sizeof(WINDOWPLACEMENT);
    }

    ~PicViewer() {
        DiscardDeviceResources();
        SafeRelease(&m_pTextFormat);
        SafeRelease(&m_pDWriteFactory);
        SafeRelease(&m_pWICFactory);
        SafeRelease(&m_pD2DFactory);
        CoUninitialize();
    }

    HRESULT Initialize() {
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (FAILED(hr)) return hr;

        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
        if (FAILED(hr)) return hr;

        hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_pWICFactory));
        if (FAILED(hr)) return hr;

        FetchWicSupportedExtensions();

        hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&m_pDWriteFactory));
        if (FAILED(hr)) return hr;

        hr = m_pDWriteFactory->CreateTextFormat(
            L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, 
            DWRITE_FONT_STRETCH_NORMAL, 18.0f, L"en-us", &m_pTextFormat);
        if (FAILED(hr)) return hr;

        // Make window Per-Monitor V2 DPI aware
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = PicViewer::WndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = sizeof(LONG_PTR);
        wcex.hInstance = GetModuleHandle(NULL);
        wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
        wcex.hbrBackground = NULL;
        wcex.lpszClassName = L"PicViewerClass";

        RegisterClassEx(&wcex);

        m_hwnd = CreateWindow(
            L"PicViewerClass", L"PicViewer", WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
            NULL, NULL, GetModuleHandle(NULL), this);

        if (!m_hwnd) return E_FAIL;

        m_dpiX = (float)GetDpiForWindow(m_hwnd);
        m_dpiY = m_dpiX;

        DragAcceptFiles(m_hwnd, TRUE);

        ShowWindow(m_hwnd, SW_SHOWNORMAL);
        UpdateWindow(m_hwnd);

        // For testing/initial display: Load current directory if no args provided
        int argc;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        std::wstring startPath;
        if (argc > 1) {
            startPath = argv[1];
        } else {
            startPath = fs::current_path().wstring();
        }
        LocalFree(argv);

        ScanFolder(startPath);

        return S_OK;
    }

    void RunMessageLoop() {
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

private:
    std::unordered_set<std::wstring> m_supportedExtensions;

    void FetchWicSupportedExtensions() {
        m_supportedExtensions.clear();
        if (!m_pWICFactory) return;

        IEnumUnknown* pEnum = NULL;
        if (SUCCEEDED(m_pWICFactory->CreateComponentEnumerator(WICDecoder, WICComponentEnumerateDefault, &pEnum))) {
            IUnknown* pUnk = NULL;
            ULONG cbActual = 0;
            while (SUCCEEDED(pEnum->Next(1, &pUnk, &cbActual)) && cbActual == 1) {
                IWICBitmapDecoderInfo* pInfo = NULL;
                if (SUCCEEDED(pUnk->QueryInterface(IID_PPV_ARGS(&pInfo)))) {
                    UINT cch = 0;
                    if (SUCCEEDED(pInfo->GetFileExtensions(0, NULL, &cch)) && cch > 0) {
                        std::vector<WCHAR> exts(cch);
                        if (SUCCEEDED(pInfo->GetFileExtensions(cch, exts.data(), &cch))) {
                            std::wstring extStr(exts.data());
                            size_t start = 0, end = 0;
                            while ((end = extStr.find(L',', start)) != std::wstring::npos) {
                                std::wstring ext = extStr.substr(start, end - start);
                                std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
                                m_supportedExtensions.insert(ext);
                                start = end + 1;
                            }
                            std::wstring ext = extStr.substr(start);
                            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
                            if (!ext.empty()) m_supportedExtensions.insert(ext);
                        }
                    }
                    pInfo->Release();
                }
                pUnk->Release();
            }
            pEnum->Release();
        }
    }

    HWND m_hwnd;
    WindowMode m_mode;
    ZoomMode m_zoomMode;
    float m_customZoom;
    WINDOWPLACEMENT m_wpPrev;

    float m_panX;
    float m_panY;
    bool m_isDragging;
    POINT m_lastMousePos;
    std::wstring m_zoomText;
    UINT_PTR m_zoomTimer;

    ID2D1Factory* m_pD2DFactory;
    ID2D1HwndRenderTarget* m_pRenderTarget;
    IWICImagingFactory* m_pWICFactory;
    IDWriteFactory* m_pDWriteFactory;
    IDWriteTextFormat* m_pTextFormat;
    ID2D1Bitmap* m_pBitmap;
    ID2D1SolidColorBrush* m_pTextBrush;
    ID2D1SolidColorBrush* m_pBgBrush;

    std::vector<ImageEntry> m_images;
    int m_currentIndex;
    std::wstring m_exifText;
    bool m_showExif;
    float m_dpiX, m_dpiY;

    HRESULT CreateDeviceResources() {
        HRESULT hr = S_OK;
        if (!m_pRenderTarget) {
            RECT rc;
            GetClientRect(m_hwnd, &rc);
            D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

            hr = m_pD2DFactory->CreateHwndRenderTarget(
                D2D1::RenderTargetProperties(),
                D2D1::HwndRenderTargetProperties(m_hwnd, size),
                &m_pRenderTarget);

            if (SUCCEEDED(hr)) {
                hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_pTextBrush);
            }
            if (SUCCEEDED(hr)) {
                hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 0.5f), &m_pBgBrush);
            }

            if (SUCCEEDED(hr) && m_images.size() > 0 && m_currentIndex >= 0 && !m_pBitmap) {
                LoadImageAt(m_currentIndex);
            }
        }
        return hr;
    }

    void DiscardDeviceResources() {
        SafeRelease(&m_pRenderTarget);
        SafeRelease(&m_pBitmap);
        SafeRelease(&m_pTextBrush);
        SafeRelease(&m_pBgBrush);
    }

    uint64_t GetImageSortTime(const std::wstring& path) {
        uint64_t timeRet = 0;
        IPropertyStore* pStore = NULL;
        if (SUCCEEDED(SHGetPropertyStoreFromParsingName(path.c_str(), NULL, GPS_DEFAULT, IID_PPV_ARGS(&pStore)))) {
            PROPVARIANT prop;
            PropVariantInit(&prop);
            // Try EXIF Date Taken
            if (SUCCEEDED(pStore->GetValue(PKEY_Photo_DateTaken, &prop)) && prop.vt == VT_FILETIME) {
                timeRet = ((uint64_t)prop.filetime.dwHighDateTime << 32) | prop.filetime.dwLowDateTime;
            }
            if (timeRet == 0) {
                PropVariantClear(&prop);
                // Fallback Date Created
                if (SUCCEEDED(pStore->GetValue(PKEY_DateCreated, &prop)) && prop.vt == VT_FILETIME) {
                    timeRet = ((uint64_t)prop.filetime.dwHighDateTime << 32) | prop.filetime.dwLowDateTime;
                }
            }
            PropVariantClear(&prop);
            pStore->Release();
        }

        if (timeRet == 0) {
            // Ultimate fallback std::filesystem
            try {
                auto ftime = fs::last_write_time(path);
                timeRet = ftime.time_since_epoch().count();
            } catch (...) {}
        }
        return timeRet;
    }

    void ScanFolder(const std::wstring& path) {
        fs::path targetPath = path;
        fs::path directory;
        std::wstring targetFile = L"";

        try {
            if (fs::is_directory(targetPath)) {
                directory = targetPath;
            } else if (fs::is_regular_file(targetPath)) {
                directory = targetPath.parent_path();
                targetFile = targetPath.wstring();
            } else {
                return;
            }

            m_images.clear();
            std::error_code ec;
            for (auto it = fs::directory_iterator(directory, fs::directory_options::skip_permission_denied, ec);
                 it != fs::directory_iterator(); it.increment(ec)) {
                if (ec) continue;
                const auto& entry = *it;
                if (entry.is_regular_file(ec)) {
                    auto ext = entry.path().extension().wstring();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
                    if (m_supportedExtensions.count(ext) > 0) {
                        ImageEntry ie;
                        ie.path = entry.path().wstring();
                        ie.sortTime = 0;
                        m_images.push_back(ie);
                    }
                }
            }

            std::sort(m_images.begin(), m_images.end(), [](const ImageEntry& a, const ImageEntry& b) {
                return a.path < b.path;
            });

            m_currentIndex = 0;
            if (!targetFile.empty()) {
                for (size_t i = 0; i < m_images.size(); ++i) {
                    if (m_images[i].path == targetFile) {
                        m_currentIndex = (int)i;
                        break;
                    }
                }
            }

            LoadImageAt(m_currentIndex);

        } catch (...) {
            // Ignore for now
        }
    }

    void LoadImageAt(int index) {
        if (index < 0 || index >= (int)m_images.size()) return;

        SafeRelease(&m_pBitmap);
        m_exifText = L"";
        m_panX = 0.0f;
        m_panY = 0.0f;
        m_isDragging = false;
        m_zoomText = L"";
        if (m_zoomTimer) { KillTimer(m_hwnd, m_zoomTimer); m_zoomTimer = 0; }

        if (!m_pRenderTarget) return;

        std::wstring path = m_images[index].path;
        IWICBitmapDecoder* pDecoder = NULL;
        IWICBitmapFrameDecode* pSource = NULL;
        IWICFormatConverter* pConverter = NULL;

        HRESULT hr = m_pWICFactory->CreateDecoderFromFilename(
            path.c_str(), NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);

        if (SUCCEEDED(hr)) hr = pDecoder->GetFrame(0, &pSource);
        if (SUCCEEDED(hr)) hr = m_pWICFactory->CreateFormatConverter(&pConverter);

        if (SUCCEEDED(hr)) {
            hr = pConverter->Initialize(
                pSource, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                NULL, 0.f, WICBitmapPaletteTypeMedianCut);
        }

        if (SUCCEEDED(hr)) {
            hr = m_pRenderTarget->CreateBitmapFromWicBitmap(pConverter, NULL, &m_pBitmap);
        }

        // Load EXIF
        if (SUCCEEDED(hr)) {
            ExtractExif(path);
            // Append filename info
            m_exifText = m_images[index].path + L"\n" + m_exifText;
        }

        SafeRelease(&pConverter);
        SafeRelease(&pSource);
        SafeRelease(&pDecoder);

        SetWindowTextW(m_hwnd, path.c_str());
        InvalidateRect(m_hwnd, NULL, FALSE);
    }

    void ExtractExif(const std::wstring& path) {
        IPropertyStore* pStore = NULL;
        std::wstring absPath = fs::absolute(path).wstring();
        if (SUCCEEDED(SHGetPropertyStoreFromParsingName(absPath.c_str(), NULL, GPS_DEFAULT, IID_PPV_ARGS(&pStore)))) {
            PROPVARIANT prop;
            PropVariantInit(&prop);

            auto GetPropString = [&](PROPERTYKEY key, const wchar_t* label) {
                if (SUCCEEDED(pStore->GetValue(key, &prop))) {
                    PWSTR displayStr = NULL;
                    if (SUCCEEDED(PSFormatForDisplayAlloc(key, prop, PDFF_DEFAULT, &displayStr))) {
                        m_exifText += std::wstring(label) + L": " + displayStr + L"\n";
                        CoTaskMemFree(displayStr);
                    }
                    PropVariantClear(&prop);
                }
            };

            GetPropString(PKEY_Photo_CameraModel, L"Model");
            GetPropString(PKEY_Photo_DateTaken, L"Date");
            GetPropString(PKEY_Photo_ISOSpeed, L"ISO");
            GetPropString(PKEY_Photo_FNumber, L"F-Stop");
            GetPropString(PKEY_Photo_ExposureTime, L"Exposure");
            GetPropString(PKEY_Photo_FocalLength, L"Focal Length");
            GetPropString(PKEY_Image_Dimensions, L"Dimensions");

            pStore->Release();
        } else {
            m_exifText += L"EXIF: N/A\n";
        }
    }

    void OnPaint() {
        HRESULT hr = CreateDeviceResources();
        if (SUCCEEDED(hr)) {
            m_pRenderTarget->BeginDraw();
            m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
            m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black)); // Black background

            if (m_pBitmap) {
                auto renderSize = m_pRenderTarget->GetSize();
                auto imgSize = m_pBitmap->GetSize();

                float scaleX = renderSize.width / imgSize.width;
                float scaleY = renderSize.height / imgSize.height;
                float scale = 1.0f;

                switch (m_zoomMode) {
                    case ZoomMode::FitBoth: scale = min(1.0f, min(scaleX, scaleY)); break;
                    case ZoomMode::FitHeight: scale = scaleY; break;
                    case ZoomMode::FitWidth: scale = scaleX; break;
                    case ZoomMode::Native100: scale = 1.0f; break;
                    case ZoomMode::Custom: scale = m_customZoom; break;
                }

                float renderWidth = imgSize.width * scale;
                float renderHeight = imgSize.height * scale;

                float drawX = (renderSize.width - renderWidth) / 2.0f + m_panX;
                float drawY = (renderSize.height - renderHeight) / 2.0f + m_panY;

                D2D1_RECT_F destRect = D2D1::RectF(drawX, drawY, drawX + renderWidth, drawY + renderHeight);
                m_pRenderTarget->DrawBitmap(m_pBitmap, destRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
            }

            if (!m_zoomText.empty() && m_pTextFormat && m_pTextBrush) {
                auto renderSize = m_pRenderTarget->GetSize();
                D2D1_RECT_F textRect = D2D1::RectF(renderSize.width - 200.0f, 10.0f, renderSize.width - 10.0f, 40.0f);
                if (m_pBgBrush) {
                    m_pRenderTarget->FillRectangle(textRect, m_pBgBrush);
                }
                m_pRenderTarget->DrawText(
                    m_zoomText.c_str(), (UINT32)m_zoomText.length(),
                    m_pTextFormat, textRect, m_pTextBrush);
            }

            if (m_showExif && !m_exifText.empty() && m_pTextFormat && m_pTextBrush) {
                D2D1_RECT_F textRect = D2D1::RectF(10.0f, 10.0f, 600.0f, 200.0f);
                if (m_pBgBrush) {
                    m_pRenderTarget->FillRectangle(D2D1::RectF(0, 0, 600, 200), m_pBgBrush);
                }
                m_pRenderTarget->DrawText(
                    m_exifText.c_str(), (UINT32)m_exifText.length(),
                    m_pTextFormat, textRect, m_pTextBrush);
            }

            hr = m_pRenderTarget->EndDraw();
            if (hr == D2DERR_RECREATE_TARGET) {
                hr = S_OK;
                DiscardDeviceResources();
            }
        }
    }

    void OnResize(UINT width, UINT height) {
        if (m_pRenderTarget) {
            m_pRenderTarget->Resize(D2D1::SizeU(width, height));
        }
    }

    void ToggleWindowMode() {
        m_mode = static_cast<WindowMode>((static_cast<int>(m_mode) + 1) % 3);
        DWORD dwStyle = GetWindowLong(m_hwnd, GWL_STYLE);

        if (m_mode == WindowMode::Standard) {
            SetWindowLong(m_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
            SetWindowPlacement(m_hwnd, &m_wpPrev);
            SetWindowPos(m_hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        } else if (m_mode == WindowMode::Borderless) {
            GetWindowPlacement(m_hwnd, &m_wpPrev);
            SetWindowLong(m_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE | WS_SIZEBOX);
            SetWindowPos(m_hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        } else if (m_mode == WindowMode::Fullscreen) {
            if (m_mode != WindowMode::Borderless) {
                GetWindowPlacement(m_hwnd, &m_wpPrev);
            }
            SetWindowLong(m_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            HMONITOR hmon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = { sizeof(mi) };
            if (GetMonitorInfo(hmon, &mi)) {
                SetWindowPos(m_hwnd, HWND_TOP, 
                             mi.rcMonitor.left, mi.rcMonitor.top,
                             mi.rcMonitor.right - mi.rcMonitor.left,
                             mi.rcMonitor.bottom - mi.rcMonitor.top,
                             SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            }
        }
    }

    void ToggleExif() {
        m_showExif = !m_showExif;
        InvalidateRect(m_hwnd, NULL, FALSE);
    }

    void Navigate(int step) {
        if (m_images.empty()) return;
        m_currentIndex += step;
        if (m_currentIndex < 0) m_currentIndex = (int)m_images.size() - 1;
        if (m_currentIndex >= (int)m_images.size()) m_currentIndex = 0;
        LoadImageAt(m_currentIndex);
    }

    void NavigateFirst() {
        if (m_images.empty()) return;
        m_currentIndex = 0;
        LoadImageAt(m_currentIndex);
    }

    void NavigateLast() {
        if (m_images.empty()) return;
        m_currentIndex = (int)m_images.size() - 1;
        LoadImageAt(m_currentIndex);
    }

    void LocateInExplorer() {
        if (m_currentIndex >= 0 && m_currentIndex < (int)m_images.size()) {
            std::wstring args = L"/select,\"" + fs::absolute(m_images[m_currentIndex].path).wstring() + L"\"";
            ShellExecuteW(NULL, L"open", L"explorer.exe", args.c_str(), NULL, SW_SHOWNORMAL);
        }
    }

    void SortByExif() {
        if (m_images.empty()) return;
        
        std::wstring currentPath = m_images[m_currentIndex].path;
        bool wasExifShown = m_showExif;
        
        m_showExif = true;
        m_exifText = L"Sorting folder by EXIF Capture Date... Please wait.\n";
        InvalidateRect(m_hwnd, NULL, FALSE);
        UpdateWindow(m_hwnd);

        for (auto& ie : m_images) {
            ie.sortTime = GetImageSortTime(ie.path);
        }

        std::sort(m_images.begin(), m_images.end(), [](const ImageEntry& a, const ImageEntry& b) {
            if (a.sortTime == b.sortTime) return a.path < b.path;
            return a.sortTime < b.sortTime;
        });

        for (size_t i = 0; i < m_images.size(); ++i) {
            if (m_images[i].path == currentPath) {
                m_currentIndex = (int)i;
                break;
            }
        }

        m_showExif = wasExifShown;
        LoadImageAt(m_currentIndex);
    }

    float GetCurrentScale() {
        if (!m_pBitmap || !m_pRenderTarget) return 1.0f;
        auto renderSize = m_pRenderTarget->GetSize();
        auto imgSize = m_pBitmap->GetSize();
        float scaleX = renderSize.width / imgSize.width;
        float scaleY = renderSize.height / imgSize.height;

        switch (m_zoomMode) {
            case ZoomMode::FitBoth: return min(1.0f, min(scaleX, scaleY));
            case ZoomMode::FitHeight: return scaleY;
            case ZoomMode::FitWidth: return scaleX;
            case ZoomMode::Native100: return 1.0f;
            case ZoomMode::Custom: return m_customZoom;
        }
        return 1.0f;
    }

    bool IsZoomedBeyondWindow() {
        if (!m_pBitmap || !m_pRenderTarget) return false;
        float scale = GetCurrentScale();
        auto renderSize = m_pRenderTarget->GetSize();
        auto imgSize = m_pBitmap->GetSize();
        return (imgSize.width * scale > renderSize.width) || (imgSize.height * scale > renderSize.height);
    }

    void ClampPan() {
        if (!m_pBitmap || !m_pRenderTarget) return;
        float scale = GetCurrentScale();
        auto renderSize = m_pRenderTarget->GetSize();
        auto imgSize = m_pBitmap->GetSize();
        
        float renderWidth = imgSize.width * scale;
        float renderHeight = imgSize.height * scale;
        
        if (renderWidth > renderSize.width) {
            float maxPanX = (renderWidth - renderSize.width) / 2.0f;
            if (m_panX > maxPanX) m_panX = maxPanX;
            if (m_panX < -maxPanX) m_panX = -maxPanX;
        } else {
            m_panX = 0;
        }

        if (renderHeight > renderSize.height) {
            float maxPanY = (renderHeight - renderSize.height) / 2.0f;
            if (m_panY > maxPanY) m_panY = maxPanY;
            if (m_panY < -maxPanY) m_panY = -maxPanY;
        } else {
            m_panY = 0;
        }
    }

    void ShowZoomOverlay(std::wstring text) {
        m_zoomText = text;
        if (m_zoomTimer) KillTimer(m_hwnd, m_zoomTimer);
        m_zoomTimer = SetTimer(m_hwnd, 1, 1500, NULL);
        InvalidateRect(m_hwnd, NULL, FALSE);
    }

    void ToggleZoomZ() {
        if (m_zoomMode == ZoomMode::FitBoth || m_zoomMode == ZoomMode::Custom) { m_zoomMode = ZoomMode::FitHeight; ShowZoomOverlay(L"Fit Height"); }
        else if (m_zoomMode == ZoomMode::FitHeight) { m_zoomMode = ZoomMode::FitWidth; ShowZoomOverlay(L"Fit Width"); }
        else if (m_zoomMode == ZoomMode::FitWidth) { m_zoomMode = ZoomMode::Native100; ShowZoomOverlay(L"100% Native"); }
        else if (m_zoomMode == ZoomMode::Native100) { m_zoomMode = ZoomMode::FitBoth; ShowZoomOverlay(L"Fit Best"); }
        
        m_panX = 0.0f;
        m_panY = 0.0f;
        InvalidateRect(m_hwnd, NULL, FALSE);
    }

public:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        PicViewer* pThis = NULL;

        if (message == WM_CREATE) {
            CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
            pThis = (PicViewer*)pCreate->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        } else {
            pThis = (PicViewer*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        }

        if (pThis) {
            switch (message) {
            case WM_PAINT:
                pThis->OnPaint();
                ValidateRect(hwnd, NULL);
                return 0;
            case WM_SIZE:
                pThis->OnResize(LOWORD(lParam), HIWORD(lParam));
                return 0;
            case WM_DPICHANGED: {
                // Resize according to suggested rect
                auto* const prcNewWindow = (RECT*)lParam;
                SetWindowPos(hwnd,
                    NULL,
                    prcNewWindow->left,
                    prcNewWindow->top,
                    prcNewWindow->right - prcNewWindow->left,
                    prcNewWindow->bottom - prcNewWindow->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
                return 0;
            }
            case WM_KEYDOWN:
                if (wParam == VK_LEFT) {
                    if (pThis->IsZoomedBeyondWindow()) { pThis->m_panX += 50.0f; pThis->ClampPan(); InvalidateRect(hwnd, NULL, FALSE); }
                    else pThis->Navigate(-1);
                }
                else if (wParam == VK_RIGHT) {
                    if (pThis->IsZoomedBeyondWindow()) { pThis->m_panX -= 50.0f; pThis->ClampPan(); InvalidateRect(hwnd, NULL, FALSE); }
                    else pThis->Navigate(1);
                }
                else if (wParam == VK_UP) {
                    if (pThis->IsZoomedBeyondWindow()) { pThis->m_panY += 50.0f; pThis->ClampPan(); InvalidateRect(hwnd, NULL, FALSE); }
                }
                else if (wParam == VK_DOWN) {
                    if (pThis->IsZoomedBeyondWindow()) { pThis->m_panY -= 50.0f; pThis->ClampPan(); InvalidateRect(hwnd, NULL, FALSE); }
                }
                else if (wParam == VK_PRIOR) pThis->Navigate(-1);
                else if (wParam == VK_NEXT) pThis->Navigate(1);
                else if (wParam == VK_HOME) pThis->NavigateFirst();
                else if (wParam == VK_END) pThis->NavigateLast();
                else if (wParam == 'F') pThis->ToggleWindowMode();
                else if (wParam == 'I') pThis->ToggleExif();
                else if (wParam == 'L') pThis->LocateInExplorer();
                else if (wParam == 'S') pThis->SortByExif();
                else if (wParam == 'Z') pThis->ToggleZoomZ();
                else if (wParam == VK_ESCAPE) PostQuitMessage(0);
                return 0;
            case WM_MOUSEWHEEL: {
                int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
                if (GetKeyState(VK_CONTROL) & 0x8000) {
                    if (pThis->m_zoomMode != ZoomMode::Custom) {
                        pThis->m_customZoom = pThis->GetCurrentScale();
                        pThis->m_zoomMode = ZoomMode::Custom;
                    }
                    if (zDelta > 0) pThis->m_customZoom *= 1.25f;
                    else if (zDelta < 0) pThis->m_customZoom /= 1.25f;
                    
                    if (pThis->m_customZoom < 0.25f) pThis->m_customZoom = 0.25f;
                    if (pThis->m_customZoom > 4.0f) pThis->m_customZoom = 4.0f;
                    
                    pThis->ClampPan();
                    
                    wchar_t buf[64];
                    swprintf(buf, 64, L"Zoom: %d%%", (int)(pThis->m_customZoom * 100));
                    pThis->ShowZoomOverlay(buf);

                    InvalidateRect(hwnd, NULL, FALSE);
                } else {
                    if (zDelta > 0) pThis->Navigate(-1);
                    else if (zDelta < 0) pThis->Navigate(1);
                }
                return 0;
            }
            case WM_LBUTTONDOWN:
                if (pThis->IsZoomedBeyondWindow()) {
                    pThis->m_isDragging = true;
                    pThis->m_lastMousePos.x = GET_X_LPARAM(lParam);
                    pThis->m_lastMousePos.y = GET_Y_LPARAM(lParam);
                    SetCapture(hwnd);
                }
                return 0;
            case WM_MOUSEMOVE:
                if (pThis->m_isDragging) {
                    int x = GET_X_LPARAM(lParam);
                    int y = GET_Y_LPARAM(lParam);
                    pThis->m_panX += (x - pThis->m_lastMousePos.x);
                    pThis->m_panY += (y - pThis->m_lastMousePos.y);
                    pThis->m_lastMousePos.x = x;
                    pThis->m_lastMousePos.y = y;
                    pThis->ClampPan();
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                return 0;
            case WM_LBUTTONUP:
                if (pThis->m_isDragging) {
                    pThis->m_isDragging = false;
                    ReleaseCapture();
                }
                return 0;
            case WM_TIMER:
                if (wParam == 1) {
                    KillTimer(hwnd, pThis->m_zoomTimer);
                    pThis->m_zoomTimer = 0;
                    pThis->m_zoomText = L"";
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                return 0;
            case WM_DROPFILES: {
                HDROP hDrop = (HDROP)wParam;
                wchar_t filePath[MAX_PATH];
                if (DragQueryFileW(hDrop, 0, filePath, MAX_PATH)) {
                    pThis->ScanFolder(filePath);
                }
                DragFinish(hDrop);
                return 0;
            }
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            }
        }
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

    PicViewer app;
    if (SUCCEEDED(app.Initialize())) {
        app.RunMessageLoop();
    }
    return 0;
}
