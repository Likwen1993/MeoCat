#pragma once
#define NOMINMAX
#include <windows.h>
#include <cmath>
#include <vector>
#include <unordered_map>
#include <random>
#include <algorithm>
#include "KeyMap.hpp"
#include "InputEngine.hpp"

enum class CatCoatTheme : int {
    ClassicBlack = 0,
    Siamese = 1,
    GingerTabby,
    Tuxedo,
    Calico,
    BritishBlue = 6,
    Ragdoll = 7,
    WhiteTabby = 8,
    SolidWhite = 9
};

enum class KeyboardTheme : int {
    ClassicMilk = 0,
    MidnightHacker = 1,
    RetroCream = 2,
    PinkPaws,
    FishBones,
    CatEars,
    Count
};

enum class EyeColorTheme : int {
    NeonChartreuse = 0,
    SiameseBlue = 1,
    WarmAmber = 2,
    IceCyan = 3,
    Ivory = 4,
    BlueGold = 5
};

struct CatSettings {
    bool showKeyDetails = true;
    bool tailEasterEgg = true;
    bool petInteraction = true;
    bool fullClickThrough = false;
    bool alwaysOnTop = true;
    bool clipboardEnabled = false;
    unsigned clipboardLimit = 20;
    float scale = 1.0f;
    CatCoatTheme coatTheme = CatCoatTheme::ClassicBlack;
    KeyboardTheme keyboardTheme = KeyboardTheme::ClassicMilk;
    EyeColorTheme eyeTheme = EyeColorTheme::NeonChartreuse;
    int posX = -99999;
    int posY = -99999;

    void loadFromRegistry() {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\KuroCat", 0, KEY_READ, &hKey) != ERROR_SUCCESS) return;
        auto readDword = [&](const wchar_t* name, DWORD defVal) -> DWORD {
            DWORD val = defVal, sz = sizeof(val);
            if (RegQueryValueExW(hKey, name, nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) return val;
            return defVal;
        };
        showKeyDetails    = readDword(L"ShowInputHints", 1) != 0;
        tailEasterEgg    = readDword(L"FootEasterEgg", 1) != 0;
        petInteraction   = readDword(L"PetInteraction", 1) != 0;
        fullClickThrough = readDword(L"FullClickThrough", 0) != 0;
        alwaysOnTop      = readDword(L"AlwaysOnTop", 1) != 0;
        clipboardEnabled = readDword(L"ClipboardEnabled", 0) != 0;
        const DWORD savedLimit = readDword(L"ClipboardLimit", 20);
        if (savedLimit == 50 || savedLimit == 100) clipboardLimit = savedLimit;
        scale            = std::clamp((float)readDword(L"ScalePct", 100) / 100.0f, 0.5f, 2.0f);
        const DWORD savedCoat = readDword(L"CoatTheme", 0);
        constexpr DWORD removedSilverTabby = 5;
        if (savedCoat <= static_cast<DWORD>(CatCoatTheme::SolidWhite) && savedCoat != removedSilverTabby) {
            coatTheme = static_cast<CatCoatTheme>(savedCoat);
        }
        const DWORD savedKeyboard = readDword(L"KeyboardTheme", 0);
        if (savedKeyboard < static_cast<DWORD>(KeyboardTheme::Count)) {
            keyboardTheme = static_cast<KeyboardTheme>(savedKeyboard);
        }
        const DWORD savedEye = savedCoat == removedSilverTabby ? 0 : readDword(L"EyeTheme", 0);
        if (savedEye <= static_cast<DWORD>(EyeColorTheme::BlueGold)) {
            eyeTheme = static_cast<EyeColorTheme>(savedEye);
        }
        DWORD hasPos     = readDword(L"HasPos", 0);
        if (hasPos) {
            posX = (int)readDword(L"PosX", 0);
            posY = (int)readDword(L"PosY", 0);
        }
        RegCloseKey(hKey);
    }

    void saveToRegistry() const {
        HKEY hKey = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\KuroCat", 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS) return;
        auto writeDword = [&](const wchar_t* name, DWORD val) {
            RegSetValueExW(hKey, name, 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        };
        writeDword(L"ShowInputHints", showKeyDetails ? 1 : 0);
        writeDword(L"FootEasterEgg", tailEasterEgg ? 1 : 0);
        writeDword(L"PetInteraction", petInteraction ? 1 : 0);
        writeDword(L"FullClickThrough", fullClickThrough ? 1 : 0);
        writeDword(L"AlwaysOnTop", alwaysOnTop ? 1 : 0);
        writeDword(L"ClipboardEnabled", clipboardEnabled ? 1 : 0);
        writeDword(L"ClipboardLimit", clipboardLimit);
        writeDword(L"ScalePct", (DWORD)(scale * 100.0f + 0.5f));
        writeDword(L"CoatTheme", (DWORD)coatTheme);
        writeDword(L"KeyboardTheme", (DWORD)keyboardTheme);
        writeDword(L"EyeTheme", (DWORD)eyeTheme);
        if (posX != -99999 && posY != -99999) {
            writeDword(L"HasPos", 1);
            writeDword(L"PosX", (DWORD)posX);
            writeDword(L"PosY", (DWORD)posY);
        }
        RegCloseKey(hKey);
    }
};

struct Spring1D {
    float value = 0.0f;
    float velocity = 0.0f;
    float target = 0.0f;

    explicit Spring1D(float v = 0.0f) : value(v), velocity(0.0f), target(v) {}

    void step(float dt, float stiffness = 260.0f, float damping = 22.0f) {
        float force = (target - value) * stiffness;
        float damp = velocity * damping;
        velocity += (force - damp) * dt;
        value += velocity * dt;
    }
};

struct Spring2D {
    Spring1D x;
    Spring1D y;

    explicit Spring2D(Pt p = {0.0f, 0.0f}) : x(p.x), y(p.y) {}
    Pt point() const { return { x.value, y.value }; }
    void setTarget(Pt p) { x.target = p.x; y.target = p.y; }
    void step(float dt, float stiffness = 340.0f, float damping = 25.0f) {
        x.step(dt, stiffness, damping);
        y.step(dt, stiffness, damping);
    }
};

struct FloatingKeycap {
    std::wstring label;
    Pt position;
    Pt velocity;
    float age = 0.0f;
    float lifetime = 1.25f;
    float rotation = 0.0f;
    bool isMouse = false;
};

struct PurrSparkle {
    Pt position;
    Pt velocity;
    float age = 0.0f;
    float lifetime = 0.9f;
    bool isSleepZ = false;
};

class CatPhysics {
public:
    static constexpr float CanvasWidth  = 360.0f;
    static constexpr float CanvasHeight = 320.0f;
    static constexpr Pt SeatAnchor      = { 184.0f, 226.0f };
    static constexpr Pt LeftPawRest     = { 164.0f, 235.0f };
    static constexpr Pt RightPawRest    = { 204.0f, 235.0f };
    static constexpr Pt TailTipRest    = { 281.0f, 191.0f };
    static constexpr Pt MouseBaseCenter = { 78.0f, 258.0f };
    static constexpr float BodyMotionAmount = 0.72f;
    static constexpr Pt TailRootOffset = { 42.0f, -3.12f };
    static constexpr float AsleepThreshold = 0.8f;
    static constexpr double DrowsyAfter = 45.0;
    static constexpr double SleepAfter = 60.0;
    static constexpr Pt WandRest = {250.0f, 151.0f};

    Spring2D leftPaw{LeftPawRest};
    Spring2D rightPaw{RightPawRest};
    Spring2D tailTip{TailTipRest};
    Spring1D tailReach{0.0f};

    Spring1D bodyShiftX{0.0f};
    Spring1D bodyTilt{0.0f};
    Spring1D bodySquash{1.0f};
    Spring1D headTilt{0.0f};
    Spring1D headYaw{0.0f};
    Spring1D headPitch{0.0f};
    Spring1D headOffsetY{0.0f};
    Spring1D earFold{0.0f};
    Spring1D earTwitch{0.0f};

    Spring2D eyeGaze{{0.0f, 0.0f}};
    Spring2D eyeReflection{{0.0f, 0.0f}};
    Spring1D pupilScale{1.0f};
    Spring1D huntIntensity{0.0f};
    Spring2D mouseOffset{{0.0f, 0.0f}};
    Spring1D mousePressLeft{0.0f};
    Spring1D mousePressRight{0.0f};

    bool leftPawOnMouse = false;
    bool leftPawTyping = false;
    bool rightPawTyping = false;

    std::unordered_map<PhysicalKey, float> keyGlow;
    std::vector<FloatingKeycap> floatingKeycaps;
    std::vector<PurrSparkle> sparkles;

    float time = 0.0f;
    float tailPhase = 0.0f;

    bool wandEnabled() const { return wandEnabled_; }

    void toggleWand() {
        wandEnabled_ = !wandEnabled_;
        lastWandToggleTime_ = nowSeconds();
        wandSwipeTime_ = 0.0f;
        if (wandEnabled_) {
            wandLure_ = Spring2D(WandRest);
            sleepiness = 0.0f;
            petHeadIntensity = petBodyIntensity = 0.0f;
        }
    }

    Pt wandTip() const {
        const Pt lure = wandLure_.point();
        return {lure.x, lure.y + std::sin(time * 8.3f) * 1.25f};
    }

    Pt tailRoot() const {
        return { SeatAnchor.x + bodyShiftX.value * BodyMotionAmount + TailRootOffset.x,
                 SeatAnchor.y + TailRootOffset.y };
    }

    float tailTargetAngle() const {
        const float principal = principalTailAngle();
        return tailAngleReference_ + std::remainder(principal - tailAngleReference_, 6.28318531f);
    }

    float petHeadIntensity = 0.0f;
    float petBodyIntensity = 0.0f;
    int totalPetSessions = 0;
    float sleepiness = 0.0f;

    float effectiveEyeClosure() const {
        float petClose = std::max(petHeadIntensity, petBodyIntensity * 0.62f);
        const float sleepClose = sleepiness >= AsleepThreshold ? 1.0f : sleepiness / AsleepThreshold * 0.87f;
        return std::min(1.0f, std::max(blinkProgress_, std::max(petClose, sleepClose)));
    }

    bool isHappyClosedEyes() const { return petHeadIntensity > 0.45f; }
    bool isBlissfulSquint() const { return petBodyIntensity > 0.35f && !isHappyClosedEyes(); }

    void triggerKeyDown(PhysicalKey key, const CatSettings& settings) {
        double now = nowSeconds();
        PhysicalKey vk = visualKeyOf(key);
        keyGlow[vk] = 1.0f;

        Pt pt = KeyboardLayout::canvasPointForKey(vk);
        hasLastTypedPoint_ = true;
        lastTypedPoint_ = pt;

        if (prefersViewerLeftPaw(key)) {
            recentLeftKey_ = key;
            recentLeftTime_ = now;
            leftPaw.y.velocity -= 180.0f;
        } else {
            recentRightKey_ = key;
            recentRightTime_ = now;
            rightPaw.y.velocity -= 180.0f;
        }

        bodySquash.velocity -= 1.3f;
        earTwitch.velocity += (prefersViewerLeftPaw(key) ? -3.8f : 3.8f);
        headTilt.velocity += (prefersViewerLeftPaw(key) ? -1.4f : 1.4f);
        pupilScale.target = 1.16f;
        sleepiness = 0.0f;

        if (settings.showKeyDetails) {
            spawnFloatingKeycap(keyBadgeLabel(key), pt, now);
        }
    }

    void triggerMouseClick(int button, bool isDown, const CatSettings& settings) {
        if (isDown && settings.showKeyDetails) {
            const wchar_t* labels[] = { L"左键", L"右键", L"中键" };
            const float offsets[] = { 12.0f, -12.0f, 0.0f };
            const Pt mouse = mouseOffset.point();
            spawnFloatingKeycap(labels[button],
                                { MouseBaseCenter.x + mouse.x + offsets[button], MouseBaseCenter.y + mouse.y },
                                nowSeconds(), true);
        }
        sleepiness = 0.0f;
        if (isDown) {
            if (button == 0) {
                mousePressLeft.target = 1.0f;
                leftPaw.y.velocity += 130.0f;
            } else if (button == 1) {
                mousePressRight.target = 1.0f;
                leftPaw.y.velocity += 110.0f;
            }
            pupilScale.target = 1.12f;
            earTwitch.velocity -= 2.2f;
        } else {
            if (button == 0) mousePressLeft.target = 0.0f;
            else if (button == 1) mousePressRight.target = 0.0f;
        }
    }

    void registerPetStroke(Pt canvasPoint, float delta, bool isHead, const CatSettings& settings) {
        if (!settings.petInteraction) return;
        double now = nowSeconds();
        lastPetTime_ = now;
        sleepiness = 0.0f;

        float energy = std::min(delta / 26.0f, 0.35f);
        if (isHead) {
            petHeadIntensity = std::min(1.0f, petHeadIntensity + energy + 0.08f);
            petBodyIntensity = std::max(petBodyIntensity, petHeadIntensity * 0.6f);
            if (petHeadIntensity > 0.55f && petSparkleCooldown_ <= 0.0f) {
                petSparkleCooldown_ = 0.42f;
                sparkles.push_back({
                    { SeatAnchor.x + randRange(-44.0f, 44.0f), 88.0f + randRange(-14.0f, 18.0f) },
                    { randRange(-10.0f, 10.0f), -24.0f },
                    0.0f, 1.12f, false
                });
            }
        } else {
            petBodyIntensity = std::min(1.0f, petBodyIntensity + energy + 0.07f);
        }

        petStrokeAccum_ += delta;
        if (!isCurrentlyPetting_ && petStrokeAccum_ > 45.0f) {
            isCurrentlyPetting_ = true;
            totalPetSessions++;
        }

        float dx = (canvasPoint.x - SeatAnchor.x) / 60.0f;
        headTilt.target = std::clamp(dx * 0.16f, -0.18f, 0.18f);
        headOffsetY.velocity += 18.0f;
    }

    void step(float rawDt, const RECT& windowRect, InputEngine& engine, const CatSettings& settings,
              bool allowWandPlay = true) {
        float dt = std::min(rawDt, 1.0f / 25.0f);
        time += dt;
        engine.pollMouse(dt);

        double now = nowSeconds();
        const auto& activeKeys = engine.activeKeys;
        const auto& orderedKeys = engine.pressedOrder;
        const bool wandUnderCursor = updateWand(dt, windowRect, engine.lastCursorPos, allowWandPlay);
        const bool wandBeingPlayed = wandUnderCursor &&
            now - std::max(lastWandToggleTime_, engine.lastMouseMoveTime) < 2.5;

        // 1. Petting decay & sparkles
        petSparkleCooldown_ = std::max(0.0f, petSparkleCooldown_ - dt);
        if (now - lastPetTime_ > 0.28) {
            petHeadIntensity = std::max(0.0f, petHeadIntensity - dt * 1.4f);
            petBodyIntensity = std::max(0.0f, petBodyIntensity - dt * 1.2f);
            if (petHeadIntensity < 0.05f && petBodyIntensity < 0.05f) {
                isCurrentlyPetting_ = false;
                petStrokeAccum_ = 0.0f;
            }
        }

        // 2. Sleepy idle detection
        if (!activeKeys.empty() || engine.leftMouseDown || engine.rightMouseDown || engine.otherMouseDown) {
            lastHeldInputTime_ = now;
        }
        const double lastActivity = std::max({engine.lastKeyActivityTime, engine.lastMouseMoveTime,
                                              lastPetTime_, lastHeldInputTime_, lastWandToggleTime_});
        const double idleSec = now - lastActivity;
        const bool drowsy = idleSec >= DrowsyAfter;
        const bool asleep = idleSec >= SleepAfter;
        if (asleep) {
            sleepiness = std::max(AsleepThreshold, sleepiness);
            sleepiness += (1.0f - sleepiness) * dt * 1.5f;
            sleepZCooldown_ -= dt;
            if (sleepZCooldown_ <= 0.0f) {
                sleepZCooldown_ = 2.3f;
                constexpr float rise[] = {0.0f, 15.5f, 30.0f};
                for (int i = 0; i < 3; ++i) {
                    sparkles.push_back({
                        { SeatAnchor.x + 55.0f + 20.0f * i, 106.0f - rise[i] },
                        { 8.0f, -13.0f }, -0.24f * i, 1.5f, true
                    });
                }
            }
        } else if (drowsy) {
            sleepiness = AsleepThreshold * static_cast<float>((idleSec - DrowsyAfter) / (SleepAfter - DrowsyAfter));
        } else {
            sleepiness = std::max(0.0f, sleepiness - dt * 5.0f);
            sleepZCooldown_ = 0.0f;
        }

        // 2b. Fast Mouse Movement -> Hunting Mode (big round boba pupils + low crouch!)
        float mouseSpeed = std::hypot(engine.mouseVelocity.x, engine.mouseVelocity.y);
        if (!wandUnderCursor && mouseSpeed > 460.0f && petHeadIntensity < 0.25f && petBodyIntensity < 0.25f) {
            float boost = std::min(1.0f, (mouseSpeed - 460.0f) / 1100.0f);
            huntIntensity.target = std::min(1.0f, huntIntensity.target + (0.45f + boost * 1.15f) * dt);
            sleepiness = 0.0f;
        } else {
            huntIntensity.target = std::max(0.0f, huntIntensity.target - dt * 0.65f);
        }
        huntIntensity.step(dt, 85.0f, 16.0f);
        float hunt = std::clamp(huntIntensity.value, 0.0f, 1.0f);

        // 3. The third held key is assigned to the tail.
        constexpr double tapHoldWindow = 0.14;
        bool mouseRecent = (now - engine.lastMouseMoveTime < 2.2) || engine.leftMouseDown || engine.rightMouseDown;
        bool mouseActivelyUsed = (now - engine.lastMouseMoveTime < 0.75) || engine.leftMouseDown || engine.rightMouseDown;

        const PhysicalKey tailKey = settings.tailEasterEgg && orderedKeys.size() >= 3
            ? orderedKeys[2] : PhysicalKey::None;
        std::vector<PhysicalKey> leftCands, rightCands;
        for (PhysicalKey k : orderedKeys) {
            if (k == tailKey) continue;
            if (prefersViewerLeftPaw(k)) leftCands.push_back(k);
            else rightCands.push_back(k);
        }

        PhysicalKey leftKey = leftCands.empty() ? PhysicalKey::None : leftCands.back();
        PhysicalKey rightKey = rightCands.empty() ? PhysicalKey::None : rightCands.back();
        if (leftKey == PhysicalKey::None && rightCands.size() >= 2 &&
            (!mouseActivelyUsed || tailKey != PhysicalKey::None)) {
            auto sorted = rightCands;
            std::sort(sorted.begin(), sorted.end(), [](PhysicalKey a, PhysicalKey b) {
                return KeyboardLayout::canvasPointForKey(a).x < KeyboardLayout::canvasPointForKey(b).x;
            });
            leftKey = sorted.front();
            rightKey = sorted.back();
        } else if (rightKey == PhysicalKey::None && leftCands.size() >= 2) {
            auto sorted = leftCands;
            std::sort(sorted.begin(), sorted.end(), [](PhysicalKey a, PhysicalKey b) {
                return KeyboardLayout::canvasPointForKey(a).x < KeyboardLayout::canvasPointForKey(b).x;
            });
            leftKey = sorted.front();
            rightKey = sorted.back();
        }

        if (leftKey == PhysicalKey::None && recentLeftKey_ != PhysicalKey::None &&
            recentLeftKey_ != tailKey && now - recentLeftTime_ < tapHoldWindow) {
            leftKey = recentLeftKey_;
        }
        if (rightKey == PhysicalKey::None && recentRightKey_ != PhysicalKey::None &&
            recentRightKey_ != tailKey && now - recentRightTime_ < tapHoldWindow) {
            rightKey = recentRightKey_;
        }

        // Subtle mini-mouse movement
        mouseOffset.setTarget({
            std::clamp(engine.mouseVelocity.x * 0.0045f, -5.5f, 5.5f),
            std::clamp(engine.mouseVelocity.y * 0.0045f, -4.5f, 4.5f)
        });
        mouseOffset.step(dt, 120.0f, 16.0f);
        mousePressLeft.step(dt, 400.0f, 24.0f);
        mousePressRight.step(dt, 400.0f, 24.0f);

        Pt liveMouse = { MouseBaseCenter.x + mouseOffset.point().x, MouseBaseCenter.y + mouseOffset.point().y };

        const bool swiping = wandBeingPlayed && leftKey == PhysicalKey::None && rightKey == PhysicalKey::None;
        if (swiping && wandSwipeTime_ == 0.0f) {
            wandUsesLeftPaw_ = wandLure_.x.target < SeatAnchor.x;
        }
        const float swipeAmount = swiping
            ? std::sin(std::min(1.0f, wandSwipeTime_ / 0.62f) * 3.14159265f) : 0.0f;
        const Pt lure = swiping ? wandTip() : WandRest;
        const float wandReach = std::max(0.0f, swipeAmount);
        if (swiping) {
            wandSwipeTime_ += dt;
            if (wandSwipeTime_ >= 0.88f) wandSwipeTime_ = 0.0f;
        } else {
            wandSwipeTime_ = 0.0f;
        }

        if (leftKey != PhysicalKey::None) {
            leftPaw.setTarget(KeyboardLayout::canvasPointForKey(leftKey));
            leftPawTyping = true;
            leftPawOnMouse = false;
        } else if (!wandUnderCursor && mouseRecent && sleepiness < 0.3f) {
            leftPaw.setTarget({ liveMouse.x + 2.0f, liveMouse.y - 3.0f });
            leftPawTyping = false;
            leftPawOnMouse = true;
        } else {
            leftPaw.setTarget(swiping && wandUsesLeftPaw_ ? swipeTarget(LeftPawRest, lure, wandReach) : LeftPawRest);
            leftPawTyping = false;
            leftPawOnMouse = false;
        }

        if (rightKey != PhysicalKey::None) {
            rightPaw.setTarget(KeyboardLayout::canvasPointForKey(rightKey));
            rightPawTyping = true;
        } else {
            rightPaw.setTarget(swiping && !wandUsesLeftPaw_ ? swipeTarget(RightPawRest, lure, wandReach) : RightPawRest);
            rightPawTyping = false;
        }

        if (tailKey != PhysicalKey::None) {
            tailTip.setTarget(KeyboardLayout::canvasPointForKey(tailKey));
            tailReach.target = 1.0f;
        } else {
            tailReach.target = 0.0f;
            // Keep the last aim while the tail retracts into its idle pose.
            if (tailReach.value <= 0.015f) tailTip.setTarget(TailTipRest);
        }

        leftPaw.step(dt, 360.0f, 24.0f);
        rightPaw.step(dt, 360.0f, 24.0f);
        tailTip.step(dt, 320.0f, 22.0f);
        tailReach.step(dt, 260.0f, 22.0f);

        // 4. Body lean, squash & ear motion
        float targetShiftX = 0.0f, targetBodyTilt = 0.0f;
        if (swiping) {
            targetShiftX = (lure.x - SeatAnchor.x) * 0.10f * wandReach;
            targetBodyTilt = targetShiftX * 0.006f;
        } else if (leftPawOnMouse && !rightPawTyping) {
            targetShiftX = -10.0f;
            targetBodyTilt = -0.075f;
        } else if (leftPawOnMouse && rightPawTyping) {
            targetShiftX = -3.0f;
            targetBodyTilt = (rightPaw.point().x - 196.0f) * 0.0008f;
        } else if (leftPawTyping || rightPawTyping) {
            float avgX = ((leftPawTyping ? leftPaw.point().x : 184.0f) + (rightPawTyping ? rightPaw.point().x : 184.0f)) * 0.5f;
            targetShiftX = (avgX - 184.0f) * 0.14f;
            targetBodyTilt = (avgX - 184.0f) * 0.0011f;
        }
        bodyShiftX.target = std::clamp(targetShiftX, -16.0f, 16.0f);
        bodyTilt.target = std::clamp(targetBodyTilt, -0.12f, 0.12f);
        bodySquash.target = 1.0f - hunt * 0.11f;
        headOffsetY.target = (leftPawTyping || rightPawTyping) ? 3.5f : (sleepiness * 5.0f + hunt * 10.5f);
        earFold.target = std::max(petHeadIntensity * 0.7f, petBodyIntensity * 0.45f) + sleepiness * 0.25f - hunt * 0.18f;
        earTwitch.target = 0.0f;

        bodyShiftX.step(dt, 140.0f, 18.0f);
        tailAngleReference_ = tailKey == PhysicalKey::None && tailReach.value <= 0.015f
            ? principalTailAngle() : tailTargetAngle();
        bodyTilt.step(dt, 140.0f, 18.0f);
        bodySquash.step(dt, 280.0f, 18.0f);
        headOffsetY.step(dt, 180.0f, 19.0f);
        earFold.step(dt, 120.0f, 16.0f);
        earTwitch.step(dt, 320.0f, 16.0f);

        // 5. 2.5D Head Turning (Yaw + Pitch) & Eye Tracking (Windows screen Y is top-down!)
        bool typingRecently = !activeKeys.empty() || (now - engine.lastKeyActivityTime < 0.65);
        bool mouseMovedRecently = (now - engine.lastMouseMoveTime < 1.2);
        Pt eyeOrigin = { SeatAnchor.x + bodyShiftX.value, 124.0f + headOffsetY.value };

        float winW = std::max(1.0f, (float)(windowRect.right - windowRect.left));
        float winScale = std::max(0.2f, winW / CanvasWidth);
        float headScreenX = (float)windowRect.left + eyeOrigin.x * winScale;
        float headScreenY = (float)windowRect.top  + eyeOrigin.y * winScale;

        float curDx = ((float)engine.lastCursorPos.x - headScreenX) / 290.0f;
        float curDy = ((float)engine.lastCursorPos.y - headScreenY) / 260.0f;
        float curMag = std::hypot(curDx, curDy);
        float clampedCur = std::min(1.0f, curMag);
        float cursorNX = curMag > 0.001f ? (curDx / curMag) * clampedCur : 0.0f;
        float cursorNY = curMag > 0.001f ? (curDy / curMag) * clampedCur : 0.0f;

        float lookX = cursorNX, lookY = cursorNY;
        if (swiping) {
            lookX = std::clamp((lure.x - eyeOrigin.x) / 72.0f, -1.0f, 1.0f);
            lookY = std::clamp((lure.y - eyeOrigin.y) / 90.0f, -1.0f, 1.0f);
        } else if (typingRecently && !mouseMovedRecently && hasLastTypedPoint_) {
            lookX = std::clamp((lastTypedPoint_.x - eyeOrigin.x) / 95.0f, -0.85f, 0.85f);
            lookY = std::clamp((lastTypedPoint_.y - eyeOrigin.y) / 125.0f, -0.3f, 0.92f);
        } else if (typingRecently && mouseMovedRecently && hasLastTypedPoint_) {
            float kdx = std::clamp((lastTypedPoint_.x - eyeOrigin.x) / 95.0f, -0.85f, 0.85f);
            lookX = cursorNX * 0.72f + kdx * 0.28f;
            lookY = cursorNY * 0.72f + 0.25f;
        }

        if (sleepiness > 0.45f) {
            headYaw.target = 0.0f;
            headPitch.target = 0.45f * sleepiness;
        } else {
            headYaw.target = std::clamp(lookX, -1.0f, 1.0f);
            headPitch.target = std::clamp(lookY, -1.0f, 1.0f);
        }
        float headStiff = 88.0f + hunt * 75.0f;
        float headDamp  = 14.5f + hunt * 4.5f;
        headYaw.step(dt, headStiff, headDamp);
        headPitch.step(dt, headStiff, headDamp);

        if (now - lastPetTime_ > 0.28) {
            headTilt.target = bodyTilt.target * 0.45f + headYaw.value * (0.11f + hunt * 0.05f) - headYaw.value * headPitch.value * 0.04f;
        }
        headTilt.step(dt, 130.0f, 16.0f);

        eyeGaze.setTarget({ lookX * 0.88f, lookY * 0.85f });
        eyeGaze.step(dt, 210.0f + hunt * 80.0f, 21.0f);
        eyeReflection.setTarget({cursorNX, cursorNY});
        eyeReflection.step(dt, 115.0f, 18.0f);

        if (tailReach.value > 0.3f) {
            pupilScale.target = 1.24f;
        } else if (now - engine.lastKeyActivityTime > 0.45 && now - engine.lastMouseClickTime > 0.45) {
            pupilScale.target = 1.0f;
        }
        pupilScale.step(dt, 150.0f, 18.0f);

        // 6. Blinking (no blinking during locked-on hunting!)
        if (isBlinking_) {
            blinkTimer_ += dt;
            constexpr float blinkDur = 0.14f;
            if (blinkTimer_ >= blinkDur) {
                isBlinking_ = false;
                blinkProgress_ = 0.0f;
                nextBlinkTime_ = time + randRange(2.6f, 5.6f);
            } else {
                blinkProgress_ = std::sin((blinkTimer_ / blinkDur) * 3.14159265f);
            }
        } else if (time >= nextBlinkTime_ && sleepiness < 0.5f && petHeadIntensity < 0.3f && hunt < 0.35f) {
            isBlinking_ = true;
            blinkTimer_ = 0.0f;
        }

        // 7. Tail wag speed
        float kpmBoost = std::min(2.4f, (float)engine.currentKPM() / 160.0f);
        float petBoost = std::max(petHeadIntensity, petBodyIntensity) * 1.4f;
        float tailSpeed = 1.85f + kpmBoost + petBoost + hunt * 2.8f - sleepiness * 1.1f;
        tailPhase += dt * tailSpeed;

        // 8. Keycap glow decay
        std::unordered_set<PhysicalKey> visualActive;
        for (PhysicalKey k : activeKeys) visualActive.insert(visualKeyOf(k));
        for (auto it = keyGlow.begin(); it != keyGlow.end(); ) {
            if (visualActive.count(it->first)) {
                it->second = 1.0f;
                ++it;
            } else {
                it->second -= dt * 4.5f;
                if (it->second <= 0.01f) it = keyGlow.erase(it);
                else ++it;
            }
        }
        for (PhysicalKey vk : visualActive) keyGlow[vk] = 1.0f;

        // 9. Floating keycaps
        for (int i = (int)floatingKeycaps.size() - 1; i >= 0; --i) {
            auto& b = floatingKeycaps[i];
            b.age += dt;
            b.position.x += b.velocity.x * dt;
            b.position.y += b.velocity.y * dt;
            b.velocity.x *= (1.0f - dt * 2.4f);
            b.velocity.y *= (1.0f - dt * 1.6f);
            if (b.age >= b.lifetime) floatingKeycaps.erase(floatingKeycaps.begin() + i);
        }

        // 10. Sparkles / Sleep Z
        for (int i = (int)sparkles.size() - 1; i >= 0; --i) {
            auto& sp = sparkles[i];
            sp.age += dt;
            const float visibleDt = std::clamp(sp.age, 0.0f, dt);
            sp.position.x += sp.velocity.x * visibleDt;
            sp.position.y += sp.velocity.y * visibleDt;
            if (sp.age >= sp.lifetime || (sp.isSleepZ && !asleep)) {
                sparkles.erase(sparkles.begin() + i);
            }
        }
    }

private:
    bool wandEnabled_ = false;
    bool wandUsesLeftPaw_ = false;
    Spring2D wandLure_{WandRest};
    float wandSwipeTime_ = 0.0f;
    double lastWandToggleTime_ = 0.0;

    bool updateWand(float dt, const RECT& rect, POINT cursor, bool allowWandPlay) {
        if (!wandEnabled_) return false;
        Pt target = WandRest;
        const bool follows = allowWandPlay && PtInRect(&rect, cursor);
        if (follows) {
            const float width = static_cast<float>(rect.right - rect.left);
            const float height = static_cast<float>(rect.bottom - rect.top);
            target = {
                std::clamp((cursor.x - rect.left) * CanvasWidth / width, 108.0f, 263.0f),
                std::clamp((cursor.y - rect.top) * CanvasHeight / height, 128.0f, 219.0f)
            };
        }
        wandLure_.setTarget(target);
        wandLure_.step(dt, 305.0f, 28.0f);
        return follows;
    }

    static Pt swipeTarget(Pt rest, Pt lure, float amount) {
        // Leave room for the paw pad beyond the arm's target point.
        const float dx = lure.x - rest.x;
        const float dy = lure.y - rest.y;
        const float distance = std::hypot(dx, dy);
        const float reach = amount * std::max(0.0f, 1.0f - 5.0f / std::max(1.0f, distance));
        return {rest.x + dx * reach, rest.y + dy * reach};
    }

    float tailAngleReference_ = 0.0f;

    float principalTailAngle() const {
        const Pt root = tailRoot();
        const Pt tip = tailTip.point();
        return std::atan2(tip.y - root.y, tip.x - root.x);
    }

    float nextBlinkTime_ = 3.2f;
    float blinkProgress_ = 0.0f;
    bool isBlinking_ = false;
    float blinkTimer_ = 0.0f;

    float petStrokeAccum_ = 0.0f;
    double lastPetTime_ = 0.0;
    bool isCurrentlyPetting_ = false;
    float petSparkleCooldown_ = 0.0f;
    double lastHeldInputTime_ = nowSeconds();
    float sleepZCooldown_ = 0.0f;

    PhysicalKey recentLeftKey_ = PhysicalKey::None;
    double recentLeftTime_ = 0.0;
    PhysicalKey recentRightKey_ = PhysicalKey::None;
    double recentRightTime_ = 0.0;
    bool hasLastTypedPoint_ = false;
    Pt lastTypedPoint_ = { 184.0f, 260.0f };

    int badgeCascade_ = 0;
    double lastBadgeTime_ = 0.0;

    static float randRange(float a, float b) {
        static thread_local std::mt19937 rng{ std::random_device{}() };
        std::uniform_real_distribution<float> dist(a, b);
        return dist(rng);
    }

    void spawnFloatingKeycap(const std::wstring& label, Pt pt, double now, bool isMouse = false) {
        if (label.empty()) return;
        if (now - lastBadgeTime_ < 0.38) badgeCascade_ = (badgeCascade_ + 1) % 6;
        else badgeCascade_ = 0;
        lastBadgeTime_ = now;

        static const float spread[6] = { -10.0f, 14.0f, -22.0f, 26.0f, 2.0f, 36.0f };
        float dx = spread[badgeCascade_] + randRange(-3.0f, 3.0f);
        float startX = std::clamp(pt.x + dx * 0.45f, 55.0f, 315.0f);
        float startY = pt.y - 26.0f - (float)(badgeCascade_ % 2) * 8.0f;

        floatingKeycaps.push_back({
            label,
            { startX, startY },
            { dx * 0.65f, -36.0f - randRange(0.0f, 14.0f) },
            0.0f, 1.40f, randRange(-0.045f, 0.045f), isMouse
        });
        if (floatingKeycaps.size() > 16) {
            floatingKeycaps.erase(floatingKeycaps.begin(), floatingKeycaps.begin() + (floatingKeycaps.size() - 16));
        }
    }
};
