#pragma once

#include <windows.h>
#include <string>

namespace Startup {

inline constexpr wchar_t keyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
inline constexpr wchar_t valueName[] = L"KuroCat";

enum class State { Off, Current, OtherPath, Unavailable };

inline std::wstring currentCommand() {
    wchar_t path[MAX_PATH + 1]{};
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH + 1);
    if (!length || length + 2 > MAX_PATH) return {};
    return L"\"" + std::wstring(path, length) + L"\"";
}

inline State getState(HKEY user = HKEY_CURRENT_USER) {
    const std::wstring command = currentCommand();
    wchar_t saved[MAX_PATH + 1]{};
    DWORD size = sizeof(saved);
    const LSTATUS status = RegGetValueW(user, keyPath, valueName, RRF_RT_REG_SZ, nullptr, saved, &size);
    if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND) return State::Off;
    if (status == ERROR_MORE_DATA || status == ERROR_UNSUPPORTED_TYPE) return State::OtherPath;
    if (status != ERROR_SUCCESS) return State::Unavailable;
    if (!saved[0]) return State::Off;
    return !command.empty() && CompareStringOrdinal(saved, -1, command.c_str(), -1, TRUE) == CSTR_EQUAL
        ? State::Current : State::OtherPath;
}

inline bool setEnabled(bool enable, HKEY user = HKEY_CURRENT_USER) {
    if (!enable) {
        LSTATUS status = RegDeleteKeyValueW(user, keyPath, valueName);
        return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND;
    }

    const std::wstring command = currentCommand();
    if (command.empty()) return false;

    HKEY key = nullptr;
    if (RegCreateKeyExW(user, keyPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    const DWORD bytes = static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t));
    LSTATUS status = RegSetValueExW(key, valueName, 0, REG_SZ,
                                  reinterpret_cast<const BYTE*>(command.c_str()), bytes);
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

} // namespace Startup
