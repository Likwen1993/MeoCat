#pragma once
#include <windows.h>
#include <string>

inline UINT windowDpi(HWND window) {
    using GetWindowDpi = UINT (WINAPI*)(HWND);
    static const auto getDpi = reinterpret_cast<GetWindowDpi>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    const UINT dpi = getDpi && window ? getDpi(window) : 0;
    if (dpi) return dpi;
    using GetMonitorDpi = HRESULT (WINAPI*)(HMONITOR, int, UINT*, UINT*);
    static const auto getMonitorDpi = []() -> GetMonitorDpi {
        wchar_t system[MAX_PATH]{};
        const UINT len = GetSystemDirectoryW(system, MAX_PATH);
        if (!len || len >= MAX_PATH) return nullptr;
        HMODULE module = LoadLibraryW((std::wstring(system, len) + L"\\shcore.dll").c_str());
        return module ? reinterpret_cast<GetMonitorDpi>(GetProcAddress(module, "GetDpiForMonitor")) : nullptr;
    }();
    if (getMonitorDpi && window) {
        UINT x = 0, y = 0;
        if (SUCCEEDED(getMonitorDpi(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), 0, &x, &y)) && x) return x;
    }
    HDC dc = GetDC(nullptr);
    if (!dc) return 96;
    const int systemDpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    return systemDpi > 0 ? static_cast<UINT>(systemDpi) : 96;
}
