#pragma once

#include <windows.h>
#include <wincrypt.h>
#include <shlobj.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class ClipboardHistory {
public:
    static constexpr unsigned defaultLimit = 20;
    static constexpr unsigned maxItems = 100;
    static constexpr size_t maxText = 16384;

    explicit ClipboardHistory(std::wstring folder = localFolder()) : folder_(std::move(folder)) {}

    const std::vector<std::wstring>& items() const { return items_; }
    bool storageError() const { return failed_; }
    static bool validLimit(unsigned limit) { return limit == 20 || limit == 50 || limit == 100; }

    bool load(unsigned limit) {
        limit_ = validLimit(limit) ? limit : defaultLimit;
        blocked_ = false;
        if (folder_.empty()) return block();
        auto file = openFile(fileName(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, OPEN_EXISTING);
        if (!file) {
            const DWORD error = GetLastError();
            if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) return block();
            items_.clear();
            failed_ = false;
            return true;
        }
        LARGE_INTEGER size{};
        if (!GetFileSizeEx(file.get(), &size) || size.QuadPart <= 0 || size.QuadPart > maxFileBytes) return block();
        std::vector<BYTE> encrypted(static_cast<size_t>(size.QuadPart));
        DWORD bytesRead = 0;
        if (!ReadFile(file.get(), encrypted.data(), static_cast<DWORD>(encrypted.size()), &bytesRead, nullptr) ||
            bytesRead != encrypted.size()) return block();
        DATA_BLOB input{bytesRead, encrypted.data()}, plain{};
        const BOOL decrypted = CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &plain);
        LocalBytes ownedPlain(plain.pbData);
        if (!decrypted) return block();
        std::vector<std::wstring> restored;
        const bool valid = decode(plain, restored);
        SecureZeroMemory(plain.pbData, plain.cbData);
        if (!valid) return block();
        failed_ = false;
        if (restored.size() > limit_) {
            restored.resize(limit_);
            if (!save(restored)) return block();
        }
        items_ = std::move(restored);
        return true;
    }

    bool setLimit(unsigned limit) {
        if (!validLimit(limit) || blocked_) return false;
        if (items_.size() > limit) {
            auto next = items_;
            next.resize(limit);
            if (!replace(std::move(next))) return false;
        }
        limit_ = limit;
        return true;
    }

    bool clear() {
        if (folder_.empty()) return fail();
        if (!deleteFile(tempName()) || !deleteFile(fileName())) return fail();
        items_.clear();
        failed_ = false;
        blocked_ = false;
        return true;
    }

    bool addText(std::wstring text) {
        if (text.empty() || text.size() > maxText || text.find(L'\0') != std::wstring::npos) return true;
        if (!items_.empty() && items_.front() == text) return true;
        // A history that cannot be decrypted is preserved until the user explicitly clears it.
        if (blocked_) return false;
        auto next = items_;
        next.erase(std::remove(next.begin(), next.end(), text), next.end());
        next.insert(next.begin(), std::move(text));
        if (next.size() > limit_) next.resize(limit_);
        return replace(std::move(next));
    }

    // False here means the clipboard was in use and can be retried shortly.
    bool readClipboard(HWND owner) {
        const DWORD sequence = GetClipboardSequenceNumber();
        if (sequence && sequence == lastSequence_) return true;
        if (!OpenClipboard(owner)) return false;
        std::wstring text;
        if (!isExcluded() && IsClipboardFormatAvailable(CF_UNICODETEXT)) {
            HGLOBAL handle = GetClipboardData(CF_UNICODETEXT);
            if (handle) {
                const size_t capacity = GlobalSize(handle) / sizeof(wchar_t);
                if (const auto* chars = static_cast<const wchar_t*>(GlobalLock(handle))) {
                    const size_t end = std::min(capacity, maxText + 1);
                    size_t length = 0;
                    while (length < end && chars[length] != L'\0') ++length;
                    if (length < end) text.assign(chars, length);
                    GlobalUnlock(handle);
                }
            }
        }
        lastSequence_ = GetClipboardSequenceNumber();
        CloseClipboard();
        if (!text.empty()) addText(std::move(text));
        return true;
    }

    void noteCurrentClipboard() { lastSequence_ = GetClipboardSequenceNumber(); }

    bool copyToClipboard(HWND owner, const std::wstring& text) {
        const size_t size = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
        if (!memory) return false;
        void* buffer = GlobalLock(memory);
        if (!buffer) { GlobalFree(memory); return false; }
        std::memcpy(buffer, text.c_str(), size);
        GlobalUnlock(memory);
        const UINT format = exclusionFormat();
        HGLOBAL marker = format ? GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DWORD)) : nullptr;
        if (!marker || !OpenClipboard(owner)) {
            if (marker) GlobalFree(marker);
            GlobalFree(memory);
            return false;
        }
        // Publish the exclusion marker before making private history text available.
        const bool marked = EmptyClipboard() && SetClipboardData(format, marker);
        if (!marked) GlobalFree(marker);
        const bool copied = marked && SetClipboardData(CF_UNICODETEXT, memory);
        if (!copied) {
            GlobalFree(memory);
            if (marked) EmptyClipboard();
        }
        noteCurrentClipboard();
        CloseClipboard();
        return copied;
    }

    static std::wstring label(const std::wstring& text) {
        std::wstring preview;
        for (wchar_t c : text) {
            if (preview.size() >= 26) break;
            if (c == L'\r' || c == L'\n' || c == L'\t') c = L' ';
            if (c < L' ' || (c >= 0x202a && c <= 0x202e) || (c >= 0x2066 && c <= 0x2069)) continue;
            if (c == L' ' && (preview.empty() || preview.back() == L' ')) continue;
            preview.push_back(c);
        }
        if (!preview.empty() && preview.back() >= 0xd800 && preview.back() <= 0xdbff) preview.pop_back();
        if (preview.empty()) return L"空白文本";
        if (text.size() > preview.size()) preview += L"…";
        return preview;
    }

private:
    struct CloseHandleFn { void operator()(void* h) const { CloseHandle(h); } };
    struct LocalFreeFn { void operator()(BYTE* h) const { LocalFree(h); } };
    using File = std::unique_ptr<void, CloseHandleFn>;
    using LocalBytes = std::unique_ptr<BYTE, LocalFreeFn>;
    static constexpr LONGLONG maxFileBytes = 4 * 1024 * 1024;
    static constexpr std::uint32_t formatVersion = 1;
    std::wstring folder_;
    std::vector<std::wstring> items_;
    unsigned limit_ = defaultLimit;
    DWORD lastSequence_ = 0;
    bool failed_ = false;
    bool blocked_ = false;

    static std::wstring localFolder() {
        wchar_t folder[MAX_PATH]{};
        if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, folder))) return {};
        return std::wstring(folder) + L"\\KaraCat";
    }

    std::wstring fileName() const { return folder_ + L"\\clipboard.dat"; }
    std::wstring tempName() const { return fileName() + L".tmp"; }
    bool fail() { failed_ = true; return false; }
    bool block() { blocked_ = true; return fail(); }

    static File openFile(const std::wstring& path, DWORD access, DWORD share, DWORD mode) {
        HANDLE file = CreateFileW(path.c_str(), access, share, nullptr, mode, FILE_ATTRIBUTE_NORMAL, nullptr);
        return File(file == INVALID_HANDLE_VALUE ? nullptr : file);
    }

    static bool deleteFile(const std::wstring& path) {
        if (DeleteFileW(path.c_str())) return true;
        const DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
    }

    static UINT exclusionFormat() {
        static const UINT format = RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing");
        return format;
    }

    static bool deniedByFormat(UINT format) {
        HGLOBAL handle = format ? GetClipboardData(format) : nullptr;
        if (!handle || GlobalSize(handle) < sizeof(DWORD)) return false;
        const auto* value = static_cast<const DWORD*>(GlobalLock(handle));
        if (!value) return false;
        const bool denied = *value == 0;
        GlobalUnlock(handle);
        return denied;
    }

    static bool isExcluded() {
        static const UINT history = RegisterClipboardFormatW(L"CanIncludeInClipboardHistory");
        static const UINT password = RegisterClipboardFormatW(L"Clipboard Viewer Ignore");
        return (exclusionFormat() && IsClipboardFormatAvailable(exclusionFormat())) ||
               (password && IsClipboardFormatAvailable(password)) || deniedByFormat(history);
    }

    static bool decode(const DATA_BLOB& blob, std::vector<std::wstring>& output) {
        const BYTE* bytes = blob.pbData;
        size_t left = blob.cbData;
        const auto readWord = [&](std::uint32_t& value) {
            if (left < sizeof(value)) return false;
            std::memcpy(&value, bytes, sizeof(value));
            bytes += sizeof(value);
            left -= sizeof(value);
            return true;
        };
        std::uint32_t version = 0, count = 0;
        if (!readWord(version) || version != formatVersion || !readWord(count) || count > maxItems) return false;
        for (unsigned i = 0; i < count; ++i) {
            std::uint32_t length = 0;
            if (!readWord(length) || length == 0 || length > maxText || left < length * sizeof(wchar_t)) return false;
            std::wstring text(length, L'\0');
            const size_t size = length * sizeof(wchar_t);
            std::memcpy(text.data(), bytes, size);
            if (text.find(L'\0') != std::wstring::npos) return false;
            output.push_back(std::move(text));
            bytes += size;
            left -= size;
        }
        return left == 0;
    }

    bool replace(std::vector<std::wstring> next) {
        if (!save(next)) return fail();
        items_ = std::move(next);
        failed_ = false;
        return true;
    }

    bool save(const std::vector<std::wstring>& next) const {
        if (folder_.empty() || (!CreateDirectoryW(folder_.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)) {
            return false;
        }
        std::vector<BYTE> plain;
        const auto writeWord = [&](std::uint32_t value) {
            const auto* bytes = reinterpret_cast<const BYTE*>(&value);
            plain.insert(plain.end(), bytes, bytes + sizeof(value));
        };
        writeWord(formatVersion);
        writeWord(static_cast<std::uint32_t>(next.size()));
        for (const auto& text : next) {
            writeWord(static_cast<std::uint32_t>(text.size()));
            const auto* bytes = reinterpret_cast<const BYTE*>(text.data());
            plain.insert(plain.end(), bytes, bytes + text.size() * sizeof(wchar_t));
        }
        DATA_BLOB input{static_cast<DWORD>(plain.size()), plain.data()}, encrypted{};
        const BOOL protectedData = CryptProtectData(&input, L"MeoCat Clipboard History", nullptr,
                                                     nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &encrypted);
        LocalBytes ownedEncrypted(encrypted.pbData);
        SecureZeroMemory(plain.data(), plain.size());
        if (!protectedData || encrypted.cbData > maxFileBytes) return false;
        auto file = openFile(tempName(), GENERIC_WRITE, 0, CREATE_ALWAYS);
        DWORD written = 0;
        const bool complete = file && WriteFile(file.get(), encrypted.pbData, encrypted.cbData, &written, nullptr) &&
                              written == encrypted.cbData && FlushFileBuffers(file.get());
        file.reset();
        if (complete && MoveFileExW(tempName().c_str(), fileName().c_str(),
                                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
        deleteFile(tempName());
        return false;
    }
};
