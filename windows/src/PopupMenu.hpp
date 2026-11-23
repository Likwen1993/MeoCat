#pragma once

#include <windows.h>
#include <dwmapi.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include "WindowDpi.hpp"

class PopupMenuStyle {
public:
    enum class Kind { Action, Toggle, Title, Status, Divider, Exit };
    using Preview = std::function<std::unique_ptr<Gdiplus::Bitmap>()>;

    struct Row {
        std::wstring text;
        std::wstring icon;
        Kind kind;
        bool checked;
        UINT dpi;
        Preview renderPreview;
        mutable std::unique_ptr<Gdiplus::Bitmap> preview;
    };

    explicit PopupMenuStyle(HWND window)
        : background_(CreateSolidBrush(backgroundColor)), dpi_(windowDpi(window)) {}

    ~PopupMenuStyle() { DeleteObject(background_); }
    PopupMenuStyle(const PopupMenuStyle&) = delete;
    PopupMenuStyle& operator=(const PopupMenuStyle&) = delete;

    UINT track(HMENU menu, HWND owner, POINT where) const {
        // Windows still handles standard mouse and keyboard navigation for the popup.
        constexpr UINT_PTR cornerTimer = 0x4B43544D;
        const UINT_PTR timer = SetTimer(owner, cornerTimer, USER_TIMER_MINIMUM, styleMenuWindows);
        const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                            where.x, where.y, 0, owner, nullptr);
        if (timer) KillTimer(owner, timer);
        return command;
    }

    HMENU create() const {
        HMENU menu = CreatePopupMenu();
        MENUINFO info = {};
        info.cbSize = sizeof(info);
        info.fMask = MIM_BACKGROUND | MIM_STYLE;
        info.hbrBack = background_;
        info.dwStyle = MNS_NOCHECK;
        SetMenuInfo(menu, &info);
        return menu;
    }

    void add(HMENU menu, UINT id, std::wstring text, const wchar_t* icon = L"",
             bool checked = false, HMENU submenu = nullptr, Kind kind = Kind::Action, Preview preview = {}) {
        auto& row = rows_.emplace_back(Row{std::move(text), icon, kind, checked, dpi_, std::move(preview), {}});
        MENUITEMINFOW info = {};
        info.cbSize = sizeof(info);
        info.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_DATA | MIIM_STRING | MIIM_ID;
        info.fType = MFT_OWNERDRAW;
        info.fState = isAction(kind) ? MFS_ENABLED : MFS_DISABLED;
        info.wID = id;
        info.dwItemData = reinterpret_cast<ULONG_PTR>(&row);
        info.dwTypeData = row.text.data();
        if (submenu) {
            info.fMask |= MIIM_SUBMENU;
            info.hSubMenu = submenu;
        }
        InsertMenuItemW(menu, static_cast<UINT>(GetMenuItemCount(menu)), TRUE, &info);
    }

    void addPreview(HMENU menu, UINT id, const wchar_t* label, bool checked, Preview preview) {
        add(menu, id, label, L"", checked, nullptr, Kind::Action, std::move(preview));
    }

    void divider(HMENU menu) { add(menu, 0, L"", L"", false, nullptr, Kind::Divider); }

    static bool measure(MEASUREITEMSTRUCT& item) {
        if (item.CtlType != ODT_MENU || !item.itemData) return false;
        const auto& row = *reinterpret_cast<const Row*>(item.itemData);
        const auto px = [dpi = row.dpi](int v) { return MulDiv(v, dpi, 96); };
        item.itemWidth = px(row.renderPreview ? 244 : 231);
        item.itemHeight = px(row.renderPreview ? 48 : row.kind == Kind::Title ? 52 :
                             row.kind == Kind::Status ? 30 : row.kind == Kind::Divider ? 9 : 31);
        return true;
    }

    static bool draw(const DRAWITEMSTRUCT& item) {
        if (item.CtlType != ODT_MENU || !item.itemData) return false;
        const auto& row = *reinterpret_cast<const Row*>(item.itemData);
        const auto px = [dpi = row.dpi](int v) { return MulDiv(v, dpi, 96); };
        const HDC dc = item.hDC;
        const RECT area = item.rcItem;
        const int mid = (area.top + area.bottom) / 2;
        fill(dc, area, backgroundColor);
        if (row.kind == Kind::Divider) {
            fill(dc, {area.left + px(44), mid, area.right - px(19), mid + px(1)}, RGB(229, 216, 231));
            return true;
        }

        const bool action = isAction(row.kind);
        const bool hover = action && (item.itemState & ODS_SELECTED) != 0;
        if (hover) round(dc, {area.left + px(6), area.top + px(1), area.right - px(5), area.bottom - px(1)},
                         px(11), row.kind == Kind::Exit ? RGB(251, 229, 231) : RGB(236, 220, 244));
        if (row.kind == Kind::Title) {
            round(dc, {area.left + px(8), area.top + px(6), area.right - px(5), area.bottom - px(1)},
                  px(14), RGB(237, 221, 246));
            const int x = area.left + px(30);
            const int y = mid + px(2);
            blackCat(dc, x, y, px(30));
            text(dc, row.text, {area.left + px(52), area.top + px(3), area.right - px(12), area.bottom},
                 px(22), true, false, RGB(66, 39, 82));
            return true;
        }
        if (row.kind == Kind::Status) {
            round(dc, {area.left + px(10), mid - px(10), area.right - px(9), mid + px(10)},
                  px(10), RGB(245, 232, 245));
            const bool ready = row.text.find(L"未启动") == std::wstring::npos &&
                               row.text.find(L"失败") == std::wstring::npos;
            round(dc, {area.left + px(18), mid - px(3), area.left + px(24), mid + px(3)},
                  px(3), ready ? RGB(66, 142, 116) : RGB(159, 99, 105));
            text(dc, row.text, {area.left + px(29), area.top, area.right - px(12), area.bottom},
                 px(11), false, false, RGB(102, 75, 112));
            return true;
        }

        const COLORREF accent = row.kind == Kind::Exit ? RGB(169, 58, 86) : RGB(117, 72, 141);
        if (row.renderPreview) {
            RECT thumbnail = {area.left + px(12), mid - px(19), area.left + px(91), mid + px(19)};
            gradient(dc, thumbnail, px(9), RGB(244, 232, 246), RGB(254, 236, 231));
            if (!row.preview) row.preview = row.renderPreview();
            if (row.preview) {
                Gdiplus::Graphics graphics(dc);
                graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                const Gdiplus::Rect destination(thumbnail.left + px(2), thumbnail.top + px(1),
                                                thumbnail.right - thumbnail.left - px(4),
                                                thumbnail.bottom - thumbnail.top - px(2));
                graphics.DrawImage(row.preview.get(), destination);
            }
        } else if (!row.icon.empty()) {
            RECT icon = {area.left + px(12), mid - px(10), area.left + px(33), mid + px(10)};
            round(dc, icon, px(9), row.kind == Kind::Exit ? RGB(251, 227, 232) : RGB(242, 229, 248));
            text(dc, row.icon, icon, px(13), true, true, accent);
        }
        text(dc, row.text, {area.left + px(row.renderPreview ? 102 : row.icon.empty() ? 16 : 43),
                           area.top, area.right - px(30), area.bottom},
             px(14), false, false, row.kind == Kind::Exit ? accent : RGB(58, 39, 70));
        if (row.checked || row.kind == Kind::Toggle) {
            RECT pill = {area.right - px(31), mid - px(8), area.right - px(15), mid + px(8)};
            round(dc, pill, px(8), row.checked ? RGB(218, 240, 227) : RGB(247, 224, 228));
            text(dc, row.checked ? L"✓" : L"×", pill, px(12), true, true,
                 row.checked ? RGB(48, 109, 85) : RGB(147, 61, 78));
        }
        return true;
    }

private:
    static bool isAction(Kind kind) {
        return kind == Kind::Action || kind == Kind::Toggle || kind == Kind::Exit;
    }

    static constexpr COLORREF backgroundColor = RGB(253, 247, 250);
    const HBRUSH background_;
    UINT dpi_;
    std::deque<Row> rows_;

    static void fill(HDC dc, const RECT& rect, COLORREF color) {
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(dc, &rect, brush);
        DeleteObject(brush);
    }

    static void CALLBACK styleMenuWindows(HWND, UINT, UINT_PTR, DWORD) {
        EnumThreadWindows(GetCurrentThreadId(), styleMenuWindow, 0);
    }

    static BOOL CALLBACK styleMenuWindow(HWND window, LPARAM) {
        constexpr wchar_t styled[] = L"MeoCat.NativeMenuStyle";
        wchar_t className[16] = {};
        if (!IsWindowVisible(window) || GetPropW(window, styled) ||
            !GetClassNameW(window, className, 16) || lstrcmpW(className, L"#32768") != 0) {
            return TRUE;
        }

        // Windows 11 composites these corners; older systems keep their native frame.
        const DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUNDSMALL;
        DwmSetWindowAttribute(window, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
        const COLORREF border = RGB(224, 208, 228);
        DwmSetWindowAttribute(window, DWMWA_BORDER_COLOR, &border, sizeof(border));
        SetPropW(window, styled, reinterpret_cast<HANDLE>(static_cast<INT_PTR>(1)));
        return TRUE;
    }

    static Gdiplus::Color paint(COLORREF color) {
        return Gdiplus::Color(GetRValue(color), GetGValue(color), GetBValue(color));
    }

    static void gradient(HDC dc, const RECT& r, int radius, COLORREF first, COLORREF last) {
        using namespace Gdiplus;
        if (r.right <= r.left || r.bottom <= r.top) return;
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        const float left = static_cast<float>(r.left), top = static_cast<float>(r.top);
        const float right = static_cast<float>(r.right), bottom = static_cast<float>(r.bottom);
        const float d = static_cast<float>(std::min<LONG>(radius * 2, std::min(r.right - r.left, r.bottom - r.top)));
        GraphicsPath shape;
        shape.AddArc(left, top, d, d, 180.0f, 90.0f);
        shape.AddArc(right - d, top, d, d, 270.0f, 90.0f);
        shape.AddArc(right - d, bottom - d, d, d, 0.0f, 90.0f);
        shape.AddArc(left, bottom - d, d, d, 90.0f, 90.0f);
        shape.CloseFigure();
        if (first == last) {
            SolidBrush brush(paint(first));
            graphics.FillPath(&brush, &shape);
        } else {
            LinearGradientBrush brush(PointF(left, top), PointF(right, top), paint(first), paint(last));
            graphics.FillPath(&brush, &shape);
        }
    }

    static void round(HDC dc, const RECT& r, int radius, COLORREF color) {
        gradient(dc, r, radius, color, color);
    }

    static void blackCat(HDC dc, int x, int y, int size) {
        using namespace Gdiplus;
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.TranslateTransform(static_cast<float>(x), static_cast<float>(y));
        const float scale = size / 32.0f;
        graphics.ScaleTransform(scale, scale);
        graphics.TranslateTransform(-16.0f, -16.0f);

        SolidBrush black(Color(22, 22, 26));
        const PointF leftEar[] = {{4, 15}, {6, 3}, {14, 11}};
        const PointF rightEar[] = {{28, 15}, {26, 3}, {18, 11}};
        graphics.FillPolygon(&black, leftEar, 3);
        graphics.FillPolygon(&black, rightEar, 3);
        graphics.FillEllipse(&black, 4.0f, 8.0f, 24.0f, 21.0f);

        SolidBrush lime(Color(201, 242, 64));
        graphics.FillEllipse(&lime, 8.5f, 14.5f, 6.0f, 5.2f);
        graphics.FillEllipse(&lime, 17.5f, 14.5f, 6.0f, 5.2f);
        graphics.FillEllipse(&black, 10.7f, 15.2f, 1.8f, 3.8f);
        graphics.FillEllipse(&black, 19.7f, 15.2f, 1.8f, 3.8f);
    }

    static void text(HDC dc, const std::wstring& label, RECT rect, int size, bool bold,
                     bool center, COLORREF color) {
        HFONT font = CreateFontW(-size, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL,
                                 FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                                 center ? L"Segoe UI Symbol" : L"Microsoft YaHei UI");
        HGDIOBJ oldFont = SelectObject(dc, font);
        const int oldMode = SetBkMode(dc, TRANSPARENT);
        const COLORREF oldColor = SetTextColor(dc, color);
        DrawTextW(dc, label.c_str(), static_cast<int>(label.size()), &rect,
                  DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS | (center ? DT_CENTER : DT_LEFT));
        SetTextColor(dc, oldColor);
        SetBkMode(dc, oldMode);
        SelectObject(dc, oldFont);
        DeleteObject(font);
    }
};
