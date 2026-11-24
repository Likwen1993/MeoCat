#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>
#include <cmath>
#include <algorithm>
#include "KeyMap.hpp"
#include "InputEngine.hpp"
#include "CatPhysics.hpp"
#include "CatRenderer.hpp"
#include "PopupMenu.hpp"
#include "Startup.hpp"
#include "ClipboardHistory.hpp"
#include "../resource.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "crypt32.lib")

namespace {

constexpr UINT WM_TRAYICON = WM_APP + 1;
constexpr UINT_PTR TIMER_RENDER = 1;
constexpr UINT_PTR TIMER_TRAY_RETRY = 2;
constexpr UINT_PTR TIMER_CLIPBOARD_RETRY = 3;
constexpr UINT dpiChangedMessage = 0x02E0;
constexpr wchar_t appName[] = L"MeoCat";

enum MenuCmd : UINT {
    CMD_TOGGLE_VISIBLE = 1001,
    CMD_TOGGLE_CLICKTHROUGH,
    CMD_TOGGLE_TOPMOST,
    CMD_TOGGLE_BADGES,
    CMD_TOGGLE_TAIL,
    CMD_TOGGLE_PET,
    CMD_TOGGLE_STARTUP,
    CMD_EYE_LIME,
    CMD_EYE_BLUE,
    CMD_EYE_AMBER,
    CMD_EYE_CYAN,
    CMD_SCALE_65,
    CMD_SCALE_85,
    CMD_SCALE_100,
    CMD_SCALE_125,
    CMD_SCALE_150,
    CMD_DEMO_BURST,
    CMD_RESET_POS,
    CMD_QUIT,
    MENU_COATS,
    MENU_KEYBOARDS,
    MENU_EYES,
    MENU_SCALE,
    MENU_CLIPBOARD,
    MENU_CLIPBOARD_LIMIT,
    CMD_CLIPBOARD_TOGGLE,
    CMD_CLIPBOARD_CLEAR,
    CMD_CLIPBOARD_20,
    CMD_CLIPBOARD_50,
    CMD_CLIPBOARD_100,
    CMD_EYE_IVORY,
    CMD_TOGGLE_WAND,
    CMD_EYE_BLUE_GOLD,
    CMD_COAT_FIRST = 1100,
    CMD_KEYBOARD_FIRST = 1200,
    CMD_CLIPBOARD_FIRST = 1300
};

struct CoatMenuItem {
    CatCoatTheme coat;
    EyeColorTheme eyes;
    const wchar_t* label;
};

constexpr CoatMenuItem coatMenuItems[] = {
    { CatCoatTheme::ClassicBlack, EyeColorTheme::NeonChartreuse, L"经典小黑猫" },
    { CatCoatTheme::Siamese, EyeColorTheme::SiameseBlue, L"挖煤暹罗猫" },
    { CatCoatTheme::GingerTabby, EyeColorTheme::Ivory, L"橘虎斑" },
    { CatCoatTheme::Tuxedo, EyeColorTheme::NeonChartreuse, L"黑白奶牛猫" },
    { CatCoatTheme::Calico, EyeColorTheme::WarmAmber, L"三花猫" },
    { CatCoatTheme::BritishBlue, EyeColorTheme::WarmAmber, L"英短蓝猫" },
    { CatCoatTheme::Ragdoll, EyeColorTheme::SiameseBlue, L"布偶猫" },
    { CatCoatTheme::WhiteTabby, EyeColorTheme::WarmAmber, L"狸白猫" },
    { CatCoatTheme::SolidWhite, EyeColorTheme::BlueGold, L"异瞳白猫" },
};

constexpr UINT coatMenuCount = sizeof(coatMenuItems) / sizeof(coatMenuItems[0]);

struct KeyboardMenuItem {
    KeyboardTheme theme;
    const wchar_t* label;
};

constexpr KeyboardMenuItem keyboardMenuItems[] = {
    { KeyboardTheme::ClassicMilk, L"奶白暖黄" },
    { KeyboardTheme::MidnightHacker, L"暗夜极客" },
    { KeyboardTheme::RetroCream, L"复古奶油" },
    { KeyboardTheme::PinkPaws, L"草莓猫爪" },
    { KeyboardTheme::FishBones, L"海盐小鱼骨" },
    { KeyboardTheme::CatEars, L"暮紫猫耳" },
};

constexpr UINT keyboardMenuCount = sizeof(keyboardMenuItems) / sizeof(keyboardMenuItems[0]);
static_assert(keyboardMenuCount == static_cast<UINT>(KeyboardTheme::Count));

struct AppState {
    HWND hwnd = nullptr;
    HICON trayIcon = nullptr;
    NOTIFYICONDATAW nid = {};
    UINT taskbarCreated = 0;
    UINT dpi = 96;
    unsigned clipboardRetries = 0;

    CatSettings settings;
    InputEngine input;
    CatPhysics physics;
    ClipboardHistory clipboard;

    bool isVisible = true;
    bool isDragging = false;
    POINT dragStartCursor = {};
    POINT dragStartWinOrigin = {};
    CatHitZone lastPetZone = CatHitZone::None;
    Pt lastHoverCanvas = {};

    // Reusable 32-bit premultiplied DIB backing buffer
    HDC memDC = nullptr;
    HBITMAP dibBmp = nullptr;
    HBITMAP oldBmp = nullptr;
    void* dibBits = nullptr;
    int bufW = 0;
    int bufH = 0;

    double lastTickTime = 0.0;
    bool demoActive = false;
    double demoStartTime = 0.0;
    int demoStepIdx = 0;

    void destroyBuffer() {
        if (memDC && oldBmp) SelectObject(memDC, oldBmp);
        if (dibBmp) DeleteObject(dibBmp);
        if (memDC) DeleteDC(memDC);
        memDC = nullptr; dibBmp = nullptr; oldBmp = nullptr; dibBits = nullptr;
        bufW = 0; bufH = 0;
    }

    void ensureBuffer(int w, int h) {
        if (w == bufW && h == bufH && memDC && dibBits) return;
        destroyBuffer();

        HDC screenDC = GetDC(nullptr);
        memDC = CreateCompatibleDC(screenDC);
        ReleaseDC(nullptr, screenDC);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h; // top-down 32bpp DIB
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        dibBmp = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &dibBits, nullptr, 0);
        if (dibBmp) {
            oldBmp = (HBITMAP)SelectObject(memDC, dibBmp);
            bufW = w;
            bufH = h;
        }
    }
};

AppState* g_app = nullptr;

void computeDefaultOrigin(int w, int h, int& outX, int& outY) {
    RECT work = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    outX = work.right - w - 24;
    outY = work.bottom - h - 12;
}

void applyWindowScaleAndPosition(AppState& app, bool anchorBottomCenter) {
    int newW = (int)std::round(CatPhysics::CanvasWidth * app.settings.scale * app.dpi / 96.0f);
    int newH = (int)std::round(CatPhysics::CanvasHeight * app.settings.scale * app.dpi / 96.0f);

    int x = app.settings.posX;
    int y = app.settings.posY;
    if (anchorBottomCenter && app.hwnd) {
        RECT r = {};
        GetWindowRect(app.hwnd, &r);
        int oldMidX = (r.left + r.right) / 2;
        int oldBotY = r.bottom;
        x = oldMidX - newW / 2;
        y = oldBotY - newH;
    } else if (x == -99999 || y == -99999) {
        computeDefaultOrigin(newW, newH, x, y);
    }

    // Test the usable center against actual displays, including gaps in a multi-monitor layout.
    const POINT center = {x + newW / 2, y + newH / 2};
    if (!MonitorFromPoint(center, MONITOR_DEFAULTTONULL)) {
        computeDefaultOrigin(newW, newH, x, y);
    }

    app.settings.posX = x;
    app.settings.posY = y;
    HWND insertAfter = app.settings.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(app.hwnd, insertAfter, x, y, newW, newH, SWP_NOACTIVATE);
    app.settings.saveToRegistry();
}

void setClickThrough(HWND hwnd, bool transparent) {
    LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    bool isTrans = (ex & WS_EX_TRANSPARENT) != 0;
    if (isTrans != transparent) {
        if (transparent) ex |= WS_EX_TRANSPARENT;
        else ex &= ~WS_EX_TRANSPARENT;
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex);
    }
}

void restoreTrayIcon(AppState& app) {
    if (Shell_NotifyIconW(NIM_ADD, &app.nid) || Shell_NotifyIconW(NIM_MODIFY, &app.nid)) {
        KillTimer(app.hwnd, TIMER_TRAY_RETRY);
    } else {
        SetTimer(app.hwnd, TIMER_TRAY_RETRY, 2000, nullptr);
    }
}

void finishDrag(AppState& app) {
    if (!app.isDragging) return;
    app.isDragging = false;
    if (GetCapture() == app.hwnd) ReleaseCapture();
    RECT rect{};
    if (GetWindowRect(app.hwnd, &rect)) {
        app.settings.posX = rect.left;
        app.settings.posY = rect.top;
        app.settings.saveToRegistry();
    }
}

HMENU createContextMenu(const AppState& app, PopupMenuStyle& style, Startup::State startup,
                        const std::vector<std::wstring>* clipboardSnapshot = nullptr) {
    using Kind = PopupMenuStyle::Kind;
    using Preview = CatRenderer::Preview;
    HMENU menu = style.create();
    if (!menu) return nullptr;

    style.add(menu, 0, appName, L"", false, nullptr, Kind::Title);
    const std::wstring status = (app.input.isRawInputRegistered ? L"输入监听中" : L"输入监听未启动")
        + std::wstring(L"  ·  ") + std::to_wstring(app.input.totalKeystrokesToday)
        + L" 键  ·  " + std::to_wstring(app.input.currentKPM()) + L" KPM";
    style.add(menu, 0, status, L"", false, nullptr, Kind::Status);
    style.divider(menu);

    HMENU coatMenu = style.create();
    for (UINT i = 0; i < coatMenuCount; ++i) {
        const auto& item = coatMenuItems[i];
        CatSettings sample = app.settings;
        sample.coatTheme = item.coat;
        sample.eyeTheme = item.eyes;
        style.addPreview(coatMenu, CMD_COAT_FIRST + i, item.label, app.settings.coatTheme == item.coat,
                         [sample] { return CatRenderer::menuPreview(Preview::Coat, sample); });
    }
    style.add(menu, MENU_COATS, L"猫咪皮肤", L"♥", false, coatMenu);

    HMENU keyboardMenu = style.create();
    for (UINT i = 0; i < keyboardMenuCount; ++i) {
        const auto& item = keyboardMenuItems[i];
        CatSettings sample = app.settings;
        sample.keyboardTheme = item.theme;
        style.addPreview(keyboardMenu, CMD_KEYBOARD_FIRST + i, item.label, app.settings.keyboardTheme == item.theme,
                         [sample] { return CatRenderer::menuPreview(Preview::Keyboard, sample); });
    }
    style.add(menu, MENU_KEYBOARDS, L"键盘主题", L"⌨", false, keyboardMenu);

    HMENU eyeMenu = style.create();
    const auto addEye = [&](UINT id, const wchar_t* label, EyeColorTheme eyes) {
        CatSettings sample = app.settings;
        sample.eyeTheme = eyes;
        style.addPreview(eyeMenu, id, label, app.settings.eyeTheme == eyes,
                         [sample] { return CatRenderer::menuPreview(Preview::Eyes, sample); });
    };
    addEye(CMD_EYE_LIME, L"荧光黄绿", EyeColorTheme::NeonChartreuse);
    addEye(CMD_EYE_BLUE, L"宝石蓝", EyeColorTheme::SiameseBlue);
    addEye(CMD_EYE_AMBER, L"琥珀金", EyeColorTheme::WarmAmber);
    addEye(CMD_EYE_CYAN, L"冰晶浅蓝", EyeColorTheme::IceCyan);
    addEye(CMD_EYE_IVORY, L"象牙白", EyeColorTheme::Ivory);
    addEye(CMD_EYE_BLUE_GOLD, L"蓝金异瞳", EyeColorTheme::BlueGold);
    style.add(menu, MENU_EYES, L"眼睛颜色", L"◉", false, eyeMenu);

    HMENU scaleMenu = style.create();
    const auto addSize = [&](UINT id, const wchar_t* label, float size) {
        style.add(scaleMenu, id, label, L"", std::fabs(app.settings.scale - size) < 0.05f);
    };
    addSize(CMD_SCALE_65, L"迷你  65%", 0.65f);
    addSize(CMD_SCALE_85, L"偏小  85%", 0.85f);
    addSize(CMD_SCALE_100, L"标准  100%", 1.00f);
    addSize(CMD_SCALE_125, L"偏大  125%", 1.25f);
    addSize(CMD_SCALE_150, L"超大  150%", 1.50f);
    style.add(menu, MENU_SCALE, L"桌宠尺寸", L"↔", false, scaleMenu);

    HMENU clipboardMenu = style.create();
    style.add(clipboardMenu, CMD_CLIPBOARD_TOGGLE, L"记录复制的文本", L"▤",
              app.settings.clipboardEnabled, nullptr, Kind::Toggle);
    HMENU limitMenu = style.create();
    style.add(limitMenu, CMD_CLIPBOARD_20, L"20 条", L"", app.settings.clipboardLimit == 20);
    style.add(limitMenu, CMD_CLIPBOARD_50, L"50 条", L"", app.settings.clipboardLimit == 50);
    style.add(limitMenu, CMD_CLIPBOARD_100, L"100 条", L"", app.settings.clipboardLimit == 100);
    style.add(clipboardMenu, MENU_CLIPBOARD_LIMIT, L"保留条数", L"≡", false, limitMenu);
    style.divider(clipboardMenu);
    const auto& items = clipboardSnapshot ? *clipboardSnapshot : app.clipboard.items();
    if (app.clipboard.storageError()) {
        style.add(clipboardMenu, 0, L"历史读取或保存失败", L"", false, nullptr, Kind::Status);
    }
    if (items.empty()) style.add(clipboardMenu, 0, L"没有历史文本", L"", false, nullptr, Kind::Status);
    constexpr unsigned groupSize = 12;
    for (unsigned first = 0; first < items.size(); first += groupSize) {
        const unsigned last = std::min<unsigned>(first + groupSize, static_cast<unsigned>(items.size()));
        HMENU group = first == 0 ? clipboardMenu : style.create();
        if (first > 0) {
            style.add(clipboardMenu, CMD_CLIPBOARD_FIRST + ClipboardHistory::maxItems + first,
                      std::to_wstring(first + 1) + L"–" + std::to_wstring(last) + L" 条", L"", false, group);
        }
        for (unsigned i = first; i < last; ++i) {
            style.add(group, CMD_CLIPBOARD_FIRST + i, ClipboardHistory::label(items[i]));
        }
    }
    style.divider(clipboardMenu);
    style.add(clipboardMenu, CMD_CLIPBOARD_CLEAR, L"清空 MeoCat 历史…", L"×", false, nullptr, Kind::Exit);
    style.add(menu, MENU_CLIPBOARD, L"最近剪贴板", L"▤", false, clipboardMenu);

    style.divider(menu);
    style.add(menu, CMD_TOGGLE_BADGES, L"显示键盘和鼠标提示", L"⌁", app.settings.showKeyDetails);
    style.add(menu, CMD_TOGGLE_TAIL, L"三键彩蛋 · 用尾巴按键", L"✦", app.settings.tailEasterEgg);
    style.add(menu, CMD_TOGGLE_PET, L"摸猫互动", L"♥", app.settings.petInteraction);
    const bool wand = app.physics.wandEnabled();
    style.add(menu, CMD_TOGGLE_WAND, wand ? L"收起逗猫棒" : L"拿出逗猫棒", L"✧", wand);
    style.add(menu, CMD_DEMO_BURST, L"演示打字", L"▶");

    style.divider(menu);
    const wchar_t* startupLabel = startup == Startup::State::OtherPath ? L"开机启动 · 其他路径" : L"开机启动";
    style.add(menu, CMD_TOGGLE_STARTUP, startupLabel, L"⏻",
              startup == Startup::State::Current || startup == Startup::State::OtherPath);
    style.add(menu, CMD_TOGGLE_TOPMOST, L"窗口置顶", L"↑", app.settings.alwaysOnTop);
    style.add(menu, CMD_TOGGLE_CLICKTHROUGH, L"鼠标穿透", L"↗", app.settings.fullClickThrough);
    style.add(menu, CMD_TOGGLE_VISIBLE, app.isVisible ? L"隐藏小猫" : L"显示小猫", L"◐");
    style.add(menu, CMD_RESET_POS, L"移回右下角", L"↻");
    style.add(menu, CMD_QUIT, L"退出 MeoCat", L"×", false, nullptr, Kind::Exit);
    return menu;
}

void showContextMenu(AppState& app, POINT screenPt) {
    PopupMenuStyle style(app.hwnd);
    const auto clipboardSnapshot = app.clipboard.items();
    HMENU menu = createContextMenu(app, style, Startup::getState(), &clipboardSnapshot);
    if (!menu) return;

    SetForegroundWindow(app.hwnd);
    UINT cmd = style.track(menu, app.hwnd, screenPt);
    DestroyMenu(menu);
    PostMessageW(app.hwnd, WM_NULL, 0, 0);

    if (cmd >= CMD_CLIPBOARD_FIRST && cmd - CMD_CLIPBOARD_FIRST < clipboardSnapshot.size()) {
        if (!app.clipboard.copyToClipboard(app.hwnd, clipboardSnapshot[cmd - CMD_CLIPBOARD_FIRST])) {
            MessageBoxW(app.hwnd, L"未能安全恢复这条剪贴板历史，请重试。", appName, MB_OK | MB_ICONWARNING);
        }
        return;
    }

    if (cmd >= CMD_COAT_FIRST && cmd < CMD_COAT_FIRST + coatMenuCount) {
        const auto& item = coatMenuItems[cmd - CMD_COAT_FIRST];
        app.settings.coatTheme = item.coat;
        app.settings.eyeTheme = item.eyes;
        app.settings.saveToRegistry();
        return;
    }

    if (cmd >= CMD_KEYBOARD_FIRST && cmd < CMD_KEYBOARD_FIRST + keyboardMenuCount) {
        app.settings.keyboardTheme = keyboardMenuItems[cmd - CMD_KEYBOARD_FIRST].theme;
        app.settings.saveToRegistry();
        return;
    }

    switch (cmd) {
        case CMD_TOGGLE_VISIBLE:
            app.isVisible = !app.isVisible;
            ShowWindow(app.hwnd, app.isVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
            break;
        case CMD_TOGGLE_CLICKTHROUGH:
            app.settings.fullClickThrough = !app.settings.fullClickThrough;
            app.settings.saveToRegistry();
            break;
        case CMD_TOGGLE_TOPMOST:
            app.settings.alwaysOnTop = !app.settings.alwaysOnTop;
            applyWindowScaleAndPosition(app, false);
            break;
        case CMD_TOGGLE_BADGES:
            app.settings.showKeyDetails = !app.settings.showKeyDetails;
            if (!app.settings.showKeyDetails) app.physics.floatingKeycaps.clear();
            app.settings.saveToRegistry();
            break;
        case CMD_TOGGLE_STARTUP: {
            const auto startup = Startup::getState();
            bool turnOn = startup == Startup::State::Off;
            if (startup == Startup::State::OtherPath) {
                const int choice = MessageBoxW(app.hwnd,
                    L"当前启动项指向另一份程序。\n"
                    L"选「是」改用当前 MeoCat，选「否」移除启动项。",
                    L"更新开机启动", MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON3);
                if (choice != IDYES && choice != IDNO) break;
                turnOn = choice == IDYES;
            }
            if (startup == Startup::State::Unavailable || !Startup::setEnabled(turnOn)) {
                MessageBoxW(app.hwnd, L"无法修改开机启动设置。", appName, MB_OK | MB_ICONWARNING);
            }
            break;
        }
        case CMD_CLIPBOARD_TOGGLE: {
            const bool enable = !app.settings.clipboardEnabled;
            const bool changed = enable ? AddClipboardFormatListener(app.hwnd) : RemoveClipboardFormatListener(app.hwnd);
            if (changed) {
                app.clipboard.noteCurrentClipboard();
                app.settings.clipboardEnabled = enable;
                if (!enable) {
                    KillTimer(app.hwnd, TIMER_CLIPBOARD_RETRY);
                    app.clipboardRetries = 0;
                }
                app.settings.saveToRegistry();
            } else {
                MessageBoxW(app.hwnd, L"无法更改剪贴板采集状态。", appName, MB_OK | MB_ICONWARNING);
            }
            break;
        }
        case CMD_CLIPBOARD_CLEAR:
            if (MessageBoxW(app.hwnd, L"确定清空 MeoCat 保存的全部剪贴板历史吗？此操作无法撤销。",
                            L"清空剪贴板历史", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
                break;
            }
            if (!app.clipboard.clear()) {
                MessageBoxW(app.hwnd, L"无法删除剪贴板历史文件。", appName, MB_OK | MB_ICONWARNING);
            } else {
                app.clipboard.noteCurrentClipboard();
            }
            break;
        case CMD_CLIPBOARD_20:
        case CMD_CLIPBOARD_50:
        case CMD_CLIPBOARD_100: {
            const unsigned limit = cmd == CMD_CLIPBOARD_20 ? 20 : cmd == CMD_CLIPBOARD_50 ? 50 : 100;
            if (app.clipboard.setLimit(limit)) {
                app.settings.clipboardLimit = limit;
                app.settings.saveToRegistry();
            } else {
                MessageBoxW(app.hwnd, L"无法保存剪贴板保留条数。", appName, MB_OK | MB_ICONWARNING);
            }
            break;
        }
        case CMD_TOGGLE_TAIL:
            app.settings.tailEasterEgg = !app.settings.tailEasterEgg;
            app.settings.saveToRegistry();
            break;
        case CMD_TOGGLE_PET:
            app.settings.petInteraction = !app.settings.petInteraction;
            app.settings.saveToRegistry();
            break;
        case CMD_TOGGLE_WAND:
            app.physics.toggleWand();
            break;
        case CMD_EYE_LIME:  app.settings.eyeTheme = EyeColorTheme::NeonChartreuse; app.settings.saveToRegistry(); break;
        case CMD_EYE_BLUE:  app.settings.eyeTheme = EyeColorTheme::SiameseBlue; app.settings.saveToRegistry(); break;
        case CMD_EYE_AMBER: app.settings.eyeTheme = EyeColorTheme::WarmAmber; app.settings.saveToRegistry(); break;
        case CMD_EYE_CYAN:  app.settings.eyeTheme = EyeColorTheme::IceCyan; app.settings.saveToRegistry(); break;
        case CMD_EYE_IVORY: app.settings.eyeTheme = EyeColorTheme::Ivory; app.settings.saveToRegistry(); break;
        case CMD_EYE_BLUE_GOLD: app.settings.eyeTheme = EyeColorTheme::BlueGold; app.settings.saveToRegistry(); break;
        case CMD_SCALE_65:  app.settings.scale = 0.65f; applyWindowScaleAndPosition(app, true); break;
        case CMD_SCALE_85:  app.settings.scale = 0.85f; applyWindowScaleAndPosition(app, true); break;
        case CMD_SCALE_100: app.settings.scale = 1.00f; applyWindowScaleAndPosition(app, true); break;
        case CMD_SCALE_125: app.settings.scale = 1.25f; applyWindowScaleAndPosition(app, true); break;
        case CMD_SCALE_150: app.settings.scale = 1.50f; applyWindowScaleAndPosition(app, true); break;
        case CMD_DEMO_BURST:
            app.demoActive = true;
            app.demoStartTime = nowSeconds();
            app.demoStepIdx = 0;
            break;
        case CMD_RESET_POS:
            app.settings.posX = -99999;
            app.settings.posY = -99999;
            applyWindowScaleAndPosition(app, false);
            break;
        case CMD_QUIT:
            DestroyWindow(app.hwnd);
            break;
        default:
            break;
    }
}

void tickAndRender(AppState& app) {
    if (!app.isVisible || !app.hwnd) return;

    double now = nowSeconds();
    float dt = (float)(now - app.lastTickTime);
    app.lastTickTime = now;
    if (dt <= 0.0f || dt > 0.2f) dt = 1.0f / 60.0f;

    // Optional menu demo typing burst
    if (app.demoActive) {
        static const PhysicalKey demoKeys[] = {
            PhysicalKey::W, PhysicalKey::A, PhysicalKey::S, PhysicalKey::D,
            PhysicalKey::F, PhysicalKey::J, PhysicalKey::O, PhysicalKey::E,
            PhysicalKey::Space, PhysicalKey::LeftShift
        };
        int targetIdx = (int)((now - app.demoStartTime) / 0.13);
        while (app.demoStepIdx <= targetIdx && app.demoStepIdx < 10) {
            app.physics.triggerKeyDown(demoKeys[app.demoStepIdx], app.settings);
            app.demoStepIdx++;
        }
        if (app.demoStepIdx >= 10) app.demoActive = false;
    }

    RECT wr = {};
    GetWindowRect(app.hwnd, &wr);
    int w = std::max(1, (int)(wr.right - wr.left));
    int h = std::max(1, (int)(wr.bottom - wr.top));

    app.physics.step(dt, wr, app.input, app.settings, !app.isDragging);

    // Smart Click-Through + Petting Detection
    if (!app.isDragging) {
        POINT cur = {};
        GetCursorPos(&cur);
        if (PtInRect(&wr, cur)) {
            Pt cp = {
                (float)(cur.x - wr.left) / (float)w * CatPhysics::CanvasWidth,
                (float)(cur.y - wr.top)  / (float)h * CatPhysics::CanvasHeight
            };
            CatHitZone zone = CatRenderer::hitTest(cp, app.physics);
            if (!app.physics.wandEnabled() && (zone == CatHitZone::Head || zone == CatHitZone::Body)) {
                if (app.lastPetZone == zone) {
                    float d = std::hypot(cp.x - app.lastHoverCanvas.x, cp.y - app.lastHoverCanvas.y);
                    if (d > 1.2f && d < 80.0f) {
                        app.physics.registerPetStroke(cp, d, zone == CatHitZone::Head, app.settings);
                    }
                }
                app.lastHoverCanvas = cp;
                app.lastPetZone = zone;
            } else {
                app.lastPetZone = CatHitZone::None;
            }
            bool clickThrough = app.settings.fullClickThrough || (zone == CatHitZone::None);
            setClickThrough(app.hwnd, clickThrough);
        } else {
            app.lastPetZone = CatHitZone::None;
            setClickThrough(app.hwnd, true);
        }
    } else {
        app.lastPetZone = CatHitZone::None;
    }

    // Render 32-bit Premultiplied ARGB frame directly into DIB and present via UpdateLayeredWindow
    app.ensureBuffer(w, h);
    if (!app.memDC || !app.dibBits) return;

    Gdiplus::Bitmap bmp(w, h, w * 4, PixelFormat32bppPARGB, static_cast<BYTE*>(app.dibBits));
    Gdiplus::Graphics g(&bmp);
    g.Clear(Gdiplus::Color(0, 0, 0, 0));
    g.ScaleTransform((float)w / CatPhysics::CanvasWidth, (float)h / CatPhysics::CanvasHeight);
    CatRenderer::draw(g, app.physics, app.settings);

    POINT ptDst = { wr.left, wr.top };
    SIZE szWnd = { w, h };
    POINT ptSrc = { 0, 0 };
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    HDC screenDC = GetDC(nullptr);
    UpdateLayeredWindow(app.hwnd, screenDC, &ptDst, &szWnd, app.memDC, &ptSrc, 0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, screenDC);
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppState* app = g_app;
    if (app && app->taskbarCreated && msg == app->taskbarCreated) {
        restoreTrayIcon(*app);
        return 0;
    }
    switch (msg) {
        case WM_MEASUREITEM:
            if (lParam && PopupMenuStyle::measure(*reinterpret_cast<MEASUREITEMSTRUCT*>(lParam))) return TRUE;
            break;
        case WM_DRAWITEM:
            if (lParam && PopupMenuStyle::draw(*reinterpret_cast<const DRAWITEMSTRUCT*>(lParam))) return TRUE;
            break;
        case WM_INPUT:
            if (app) app->input.handleRawInput(lParam);
            if (GET_RAWINPUT_CODE_WPARAM(wParam) == RIM_INPUT) return DefWindowProcW(hwnd, msg, wParam, lParam);
            return 0;
        case WM_INPUT_DEVICE_CHANGE:
            if (app && wParam == GIDC_REMOVAL) app->input.forgetDevice(reinterpret_cast<HANDLE>(lParam));
            return 0;
        case WM_CLIPBOARDUPDATE:
            if (app && app->settings.clipboardEnabled) {
                if (!app->clipboard.readClipboard(hwnd)) {
                    app->clipboardRetries = 3;
                    SetTimer(hwnd, TIMER_CLIPBOARD_RETRY, 45, nullptr);
                } else {
                    KillTimer(hwnd, TIMER_CLIPBOARD_RETRY);
                    app->clipboardRetries = 0;
                }
            }
            return 0;

        case WM_TIMER:
            if (app && wParam == TIMER_RENDER) tickAndRender(*app);
            if (app && wParam == TIMER_TRAY_RETRY) restoreTrayIcon(*app);
            if (app && wParam == TIMER_CLIPBOARD_RETRY) {
                if (!app->settings.clipboardEnabled || !app->clipboardRetries ||
                    app->clipboard.readClipboard(hwnd) || !--app->clipboardRetries) {
                    KillTimer(hwnd, TIMER_CLIPBOARD_RETRY);
                    app->clipboardRetries = 0;
                }
            }
            return 0;

        case dpiChangedMessage:
            if (app && lParam && LOWORD(wParam)) {
                app->dpi = LOWORD(wParam);
                const auto& bounds = *reinterpret_cast<const RECT*>(lParam);
                SetWindowPos(hwnd, nullptr, bounds.left, bounds.top, bounds.right - bounds.left,
                             bounds.bottom - bounds.top, SWP_NOZORDER | SWP_NOACTIVATE);
                app->settings.posX = bounds.left;
                app->settings.posY = bounds.top;
                if (app->isDragging) {
                    GetCursorPos(&app->dragStartCursor);
                    app->dragStartWinOrigin = {bounds.left, bounds.top};
                }
                return 0;
            }
            break;
        case WM_DISPLAYCHANGE:
            if (app && app->hwnd && !MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL)) {
                app->settings.posX = app->settings.posY = -99999;
                applyWindowScaleAndPosition(*app, false);
            }
            break;

        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
            break;

        case WM_LBUTTONDOWN:
            if (app && !app->settings.fullClickThrough) {
                app->isDragging = true;
                GetCursorPos(&app->dragStartCursor);
                RECT wr = {};
                GetWindowRect(hwnd, &wr);
                app->dragStartWinOrigin = { wr.left, wr.top };
                SetCapture(hwnd);
            }
            return 0;

        case WM_MOUSEMOVE:
            if (app && app->isDragging) {
                POINT cur = {};
                GetCursorPos(&cur);
                int nx = app->dragStartWinOrigin.x + (cur.x - app->dragStartCursor.x);
                int ny = app->dragStartWinOrigin.y + (cur.y - app->dragStartCursor.y);
                SetWindowPos(hwnd, nullptr, nx, ny, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
            return 0;

        case WM_LBUTTONUP:
        case WM_CAPTURECHANGED:
        case WM_CANCELMODE:
            if (app) finishDrag(*app);
            return 0;

        case WM_RBUTTONUP:
            if (app) {
                POINT pt = {};
                GetCursorPos(&pt);
                showContextMenu(*app, pt);
            }
            return 0;

        case WM_TRAYICON:
            if (app && (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP)) {
                POINT pt = {};
                GetCursorPos(&pt);
                showContextMenu(*app, pt);
            }
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, TIMER_RENDER);
            KillTimer(hwnd, TIMER_TRAY_RETRY);
            KillTimer(hwnd, TIMER_CLIPBOARD_RETRY);
            if (app) {
                if (app->settings.clipboardEnabled) RemoveClipboardFormatListener(hwnd);
                Shell_NotifyIconW(NIM_DELETE, &app->nid);
                if (app->trayIcon) DestroyIcon(app->trayIcon);
                app->destroyBuffer();
            }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    // Single-instance guard
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Local\\KuroCatDesktopPetSingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    Gdiplus::GdiplusStartupInput gdipInput;
    ULONG_PTR gdipToken = 0;
    if (Gdiplus::GdiplusStartup(&gdipToken, &gdipInput, nullptr) != Gdiplus::Ok) {
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    AppState app;
    g_app = &app;
    app.settings.loadFromRegistry();
    app.clipboard.load(app.settings.clipboardLimit);

    app.input.onKeyDown = [&](PhysicalKey k) { app.physics.triggerKeyDown(k, app.settings); };
    app.input.onMouseClick = [&](int btn, bool down) { app.physics.triggerMouseClick(btn, down, app.settings); };

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"KuroCatOverlayWindowClass";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_MEOCAT));
    RegisterClassExW(&wc);

    int initW = (int)std::round(CatPhysics::CanvasWidth * app.settings.scale);
    int initH = (int)std::round(CatPhysics::CanvasHeight * app.settings.scale);
    int initX = 0, initY = 0;
    if (app.settings.posX != -99999 && app.settings.posY != -99999) {
        initX = app.settings.posX; initY = app.settings.posY;
    } else {
        computeDefaultOrigin(initW, initH, initX, initY);
    }

    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT
                  | (app.settings.alwaysOnTop ? WS_EX_TOPMOST : 0);

    app.hwnd = CreateWindowExW(
        exStyle,
        wc.lpszClassName,
        appName,
        WS_POPUP,
        initX, initY, initW, initH,
        nullptr, nullptr, hInst, nullptr
    );
    if (!app.hwnd) {
        Gdiplus::GdiplusShutdown(gdipToken);
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    app.dpi = windowDpi(app.hwnd);
    applyWindowScaleAndPosition(app, false);
    app.input.registerRawInput(app.hwnd);
    if (app.settings.clipboardEnabled) {
        app.clipboard.noteCurrentClipboard();
        if (!AddClipboardFormatListener(app.hwnd)) app.settings.clipboardEnabled = false;
    }

    // System tray icon
    app.trayIcon = static_cast<HICON>(LoadImageW(hInst, MAKEINTRESOURCEW(IDI_MEOCAT), IMAGE_ICON,
                    GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    app.nid.cbSize = sizeof(app.nid);
    app.nid.hWnd = app.hwnd;
    app.nid.uID = 1;
    app.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    app.nid.uCallbackMessage = WM_TRAYICON;
    app.nid.hIcon = app.trayIcon;
    wcscpy_s(app.nid.szTip, L"MeoCat 键盘桌宠");
    app.taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    restoreTrayIcon(app);

    ShowWindow(app.hwnd, SW_SHOWNOACTIVATE);
    app.lastTickTime = nowSeconds();
    tickAndRender(app);
    SetTimer(app.hwnd, TIMER_RENDER, 16, nullptr);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (IsWindow(app.hwnd)) DestroyWindow(app.hwnd);
    g_app = nullptr;
    Gdiplus::GdiplusShutdown(gdipToken);
    if (hMutex) CloseHandle(hMutex);
    return 0;
}
