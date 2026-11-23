#pragma once
#define NOMINMAX
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <cmath>
#include <algorithm>
#include <memory>
#include "CatPhysics.hpp"
#include "FlatCatRenderer.hpp"

enum class CatHitZone {
    None,
    Head,
    Body
};

class CatRenderer {
public:
    static constexpr float RadToDeg = 57.2957795f;
    enum class Preview { Coat, Keyboard, Eyes };

    static CatHitZone hitTest(Pt p, const CatPhysics& physics) {
        const float cx = FlatCatRenderer::seatX(physics);
        const float breath = std::sin(physics.time * 2.1f) * 1.6f;
        const Pt offset = FlatCatRenderer::headOffset(physics, breath);
        const Pt headCenter = {cx + offset.x, 136.0f + offset.y + FlatCatRenderer::BodyDrop};
        float hdx = (p.x - headCenter.x) / 48.0f;
        float hdy = (p.y - headCenter.y) / 42.0f;
        if (hdx * hdx + hdy * hdy <= 1.0f) return CatHitZone::Head;

        float bdx = (p.x - cx) / 58.0f;
        float bdy = (p.y - (196.0f + FlatCatRenderer::BodyDrop)) / 56.0f;
        if (bdx * bdx + bdy * bdy <= 1.0f) return CatHitZone::Body;

        if (p.x >= 58.0f && p.x <= 294.0f && p.y >= 232.0f && p.y <= 296.0f) return CatHitZone::Body;
        return CatHitZone::None;
    }

    static void draw(Gdiplus::Graphics& g, const CatPhysics& physics, const CatSettings& settings) {
        using namespace Gdiplus;
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

        float breath = std::sin(physics.time * 2.1f) * 1.6f;

        drawMouse(g, physics, settings.keyboardTheme);
        drawKeyboard(g, physics, settings.keyboardTheme, settings.eyeTheme, settings.showKeyDetails);

        FlatCatRenderer::draw(g, physics, settings.coatTheme, settings.eyeTheme, breath);

        // Floating Keycap Badges
        if (settings.showKeyDetails) {
            drawFloatingKeycaps(g, physics);
        }

        // Petting Sparkles & Sleep Z.
        drawSparkles(g, physics, settings.eyeTheme);
    }

    static std::unique_ptr<Gdiplus::Bitmap> menuPreview(Preview part, const CatSettings& settings) {
        using namespace Gdiplus;
        constexpr int width = 154;
        constexpr int height = 74;
        auto bitmap = std::make_unique<Bitmap>(width, height, PixelFormat32bppARGB);
        if (bitmap->GetLastStatus() != Ok) return {};
        Graphics g(bitmap.get());
        g.Clear(Color(0, 0, 0, 0));
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        CatPhysics pose;
        pose.huntIntensity.value = part == Preview::Eyes ? 0.18f : 0.0f;
        pose.eyeReflection.x.value = -0.20f;

        if (part == Preview::Keyboard) {
            g.TranslateTransform(width * 0.5f, height * 0.5f);
            g.ScaleTransform(0.55f, 0.55f);
            g.TranslateTransform(-180.0f, -260.0f);
            drawKeyboard(g, pose, settings.keyboardTheme, settings.eyeTheme, false);
            return bitmap;
        }

        const float scale = part == Preview::Eyes ? 1.53f : 0.90f;
        const float focusY = part == Preview::Eyes ? 140.0f : 134.0f;
        g.TranslateTransform(width * 0.5f, height * 0.5f);
        g.ScaleTransform(scale, scale);
        g.TranslateTransform(-CatPhysics::SeatAnchor.x, -focusY);
        draw(g, pose, settings);
        return bitmap;
    }

private:
    static Gdiplus::Color eyePrimary(EyeColorTheme t, BYTE a = 255) {
        switch (t) {
            case EyeColorTheme::NeonChartreuse: return Gdiplus::Color(a, 201, 242, 64);
            case EyeColorTheme::SiameseBlue:    return Gdiplus::Color(a, 94, 199, 250);
            case EyeColorTheme::WarmAmber:      return Gdiplus::Color(a, 250, 194, 56);
            case EyeColorTheme::IceCyan:        return Gdiplus::Color(a, 115, 235, 245);
            case EyeColorTheme::Ivory:         return Gdiplus::Color(a, 255, 253, 247);
        }
        return Gdiplus::Color(a, 201, 242, 64);
    }

    static Gdiplus::Color blendColor(Gdiplus::Color a, Gdiplus::Color b, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        auto lerp = [t](BYTE x, BYTE y) -> BYTE { return (BYTE)std::clamp((int)(x + (y - x) * t + 0.5f), 0, 255); };
        return Gdiplus::Color(lerp(a.GetA(), b.GetA()), lerp(a.GetR(), b.GetR()), lerp(a.GetG(), b.GetG()), lerp(a.GetB(), b.GetB()));
    }

    static void addRoundedRect(Gdiplus::GraphicsPath& p, float x, float y, float w, float h, float r) {
        float d = std::min(r * 2.0f, std::min(w, h));
        if (d <= 0.5f) {
            p.AddRectangle(Gdiplus::RectF(x, y, w, h));
            return;
        }
        p.AddArc(x, y, d, d, 180.0f, 90.0f);
        p.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
        p.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
        p.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
        p.CloseFigure();
    }

    static void addQuadPoly(Gdiplus::GraphicsPath& p, Pt tl, Pt tr, Pt br, Pt bl) {
        Gdiplus::PointF pts[4] = { {tl.x, tl.y}, {tr.x, tr.y}, {br.x, br.y}, {bl.x, bl.y} };
        p.AddPolygon(pts, 4);
    }

    static void addQuadBezier(Gdiplus::GraphicsPath& p, Pt p0, Pt ctrl, Pt p2) {
        // Convert quadratic Bezier (p0, ctrl, p2) -> exact cubic Bezier for GDI+
        Gdiplus::PointF c1(p0.x + (2.0f / 3.0f) * (ctrl.x - p0.x), p0.y + (2.0f / 3.0f) * (ctrl.y - p0.y));
        Gdiplus::PointF c2(p2.x + (2.0f / 3.0f) * (ctrl.x - p2.x), p2.y + (2.0f / 3.0f) * (ctrl.y - p2.y));
        p.AddBezier(Gdiplus::PointF(p0.x, p0.y), c1, c2, Gdiplus::PointF(p2.x, p2.y));
    }

    static void drawMouse(Gdiplus::Graphics& g, const CatPhysics& physics, KeyboardTheme theme) {
        using namespace Gdiplus;
        Pt center = { CatPhysics::MouseBaseCenter.x + physics.mouseOffset.point().x,
                      CatPhysics::MouseBaseCenter.y + physics.mouseOffset.point().y };
        float pressShift = std::max(physics.mousePressLeft.value, physics.mousePressRight.value) * 1.6f;

        GraphicsState st = g.Save();
        g.TranslateTransform(center.x, center.y + pressShift);
        g.RotateTransform(0.22f * RadToDeg);

        SolidBrush shadowBrush(Color(60, 0, 0, 0));
        g.FillEllipse(&shadowBrush, -13.5f, -9.5f, 27.0f, 31.0f);

        Color bodyCol, borderCol, wheelCol;
        if (theme == KeyboardTheme::MidnightHacker) {
            bodyCol = Color(255, 51, 54, 61); borderCol = Color(255, 20, 20, 26); wheelCol = Color(255, 199, 242, 64);
        } else if (theme == KeyboardTheme::RetroCream) {
            bodyCol = Color(255, 227, 222, 209); borderCol = Color(255, 51, 48, 46); wheelCol = Color(255, 224, 107, 61);
        } else {
            bodyCol = Color(255, 244, 242, 236); borderCol = Color(255, 36, 36, 41); wheelCol = Color(255, 245, 176, 41);
        }

        SolidBrush bodyBrush(bodyCol);
        g.FillEllipse(&bodyBrush, -13.5f, -16.5f, 27.0f, 33.0f);

        if (physics.mousePressLeft.value > 0.05f || physics.mousePressRight.value > 0.05f) {
            GraphicsPath clipPath;
            clipPath.AddEllipse(-13.5f, -16.5f, 27.0f, 33.0f);
            GraphicsState cs = g.Save();
            g.SetClip(&clipPath);
            if (physics.mousePressLeft.value > 0.05f) {
                SolidBrush hb(Color((BYTE)(96 * physics.mousePressLeft.value), 201, 242, 64));
                g.FillRectangle(&hb, 0.0f, 0.0f, 14.0f, 17.0f);
            }
            if (physics.mousePressRight.value > 0.05f) {
                SolidBrush hb(Color((BYTE)(96 * physics.mousePressRight.value), 201, 242, 64));
                g.FillRectangle(&hb, -14.0f, 0.0f, 14.0f, 17.0f);
            }
            g.Restore(cs);
        }

        // Front button split seam pointing away from cat (bottom)
        Pen seamPen(Color(82, borderCol.GetR(), borderCol.GetG(), borderCol.GetB()), 1.05f);
        g.DrawLine(&seamPen, 0.0f, 16.0f, 0.0f, 1.5f);

        // Amber scroll wheel on front (bottom) half
        GraphicsPath wheelPath;
        addRoundedRect(wheelPath, -1.9f, 2.5f, 3.8f, 8.5f, 1.9f);
        SolidBrush wheelBrush(wheelCol);
        g.FillPath(&wheelBrush, &wheelPath);

        Pen borderPen(borderCol, 1.7f);
        g.DrawEllipse(&borderPen, -13.5f, -16.5f, 27.0f, 33.0f);
        g.Restore(st);
    }

    // MARK: - Mini 65% Mechanical Keyboard (180° facing the cat)

    static void drawKeyboard(Gdiplus::Graphics& g, const CatPhysics& physics, KeyboardTheme theme,
                             EyeColorTheme eyeTheme, bool showKeyDetails) {
        using namespace Gdiplus;
        Pt tl = KeyboardLayout::screenQuadPoint(-0.025f, -0.08f);
        Pt tr = KeyboardLayout::screenQuadPoint(1.025f, -0.08f);
        Pt bl = KeyboardLayout::screenQuadPoint(-0.032f, 1.08f);
        Pt br = KeyboardLayout::screenQuadPoint(1.032f, 1.08f);
        constexpr float depth = 6.5f;

        // Shadow
        {
            GraphicsPath sp;
            addQuadPoly(sp, {tl.x, tl.y + depth + 2}, {tr.x, tr.y + depth + 2},
                            {br.x, br.y + depth + 3}, {bl.x, bl.y + depth + 3});
            SolidBrush sb(Color(56, 0, 0, 0));
            g.FillPath(&sb, &sp);
        }

        Color baseCol, deckCol, keyCol, borderCol, accentCol;
        if (theme == KeyboardTheme::MidnightHacker) {
            baseCol = Color(255, 15, 15, 20); deckCol = Color(255, 26, 28, 33);
            keyCol = Color(255, 54, 56, 64); borderCol = Color(242, 28, 28, 33); accentCol = Color(255, 196, 240, 64);
        } else if (theme == KeyboardTheme::RetroCream) {
            baseCol = Color(255, 64, 61, 56); deckCol = Color(255, 97, 92, 84);
            keyCol = Color(255, 230, 224, 209); borderCol = Color(216, 173, 166, 153); accentCol = Color(255, 224, 107, 61);
        } else if (theme == KeyboardTheme::PinkPaws) {
            baseCol = Color(134, 65, 89); deckCol = Color(215, 131, 156);
            keyCol = Color(255, 236, 238); borderCol = Color(204, 134, 152); accentCol = Color(242, 163, 185);
        } else if (theme == KeyboardTheme::FishBones) {
            baseCol = Color(31, 69, 88); deckCol = Color(53, 100, 123);
            keyCol = Color(211, 239, 245); borderCol = Color(91, 148, 174); accentCol = Color(249, 207, 143);
        } else if (theme == KeyboardTheme::CatEars) {
            baseCol = Color(49, 37, 82); deckCol = Color(104, 75, 153);
            keyCol = Color(236, 220, 252); borderCol = Color(145, 117, 173); accentCol = Color(201, 159, 237);
        } else {
            baseCol = Color(255, 26, 26, 31); deckCol = Color(255, 36, 36, 41);
            keyCol = Color(255, 247, 246, 240); borderCol = Color(190, 199, 196, 189); accentCol = Color(255, 247, 179, 41);
        }

        if (theme == KeyboardTheme::CatEars) {
            SolidBrush shell(deckCol);
            SolidBrush lining(Color(242, 173, 212));
            for (float u : {0.047f, 0.953f}) {
                Pt root = KeyboardLayout::screenQuadPoint(u, -0.08f);
                GraphicsPath ear;
                addQuadBezier(ear, {root.x - 14.0f, root.y + 4.0f}, {root.x - 11.0f, root.y - 14.0f}, {root.x - 3.0f, root.y - 19.0f});
                addQuadBezier(ear, {root.x - 3.0f, root.y - 19.0f}, {root.x + 3.0f, root.y - 22.0f}, {root.x + 8.0f, root.y - 11.0f});
                addQuadBezier(ear, {root.x + 8.0f, root.y - 11.0f}, {root.x + 13.0f, root.y - 3.0f}, {root.x + 15.0f, root.y + 4.0f});
                ear.CloseFigure();
                g.FillPath(&shell, &ear);
                PointF inner[] = {{root.x - 7.5f, root.y - 1.0f}, {root.x, root.y - 13.0f}, {root.x + 7.5f, root.y - 1.0f}};
                g.FillPolygon(&lining, inner, 3);
            }
        }

        {
            GraphicsPath lp;
            addQuadPoly(lp, {tl.x, tl.y + depth}, {tr.x, tr.y + depth}, {br.x, br.y + depth}, {bl.x, bl.y + depth});
            SolidBrush bb(baseCol);
            Pen bp(baseCol, 5.0f); bp.SetLineJoin(LineJoinRound);
            g.FillPath(&bb, &lp);
            g.DrawPath(&bp, &lp);
        }
        {
            GraphicsPath tp;
            addQuadPoly(tp, tl, tr, br, bl);
            SolidBrush db(deckCol);
            Pen dp(deckCol, 4.0f); dp.SetLineJoin(LineJoinRound);
            g.FillPath(&db, &tp);
            g.DrawPath(&dp, &tp);
        }

        constexpr float padU = 0.0028f;
        constexpr float padV = 0.014f;
        Color glowCol = eyePrimary(eyeTheme, 215);
        Pen keyBorderPen(borderCol, 0.7f);

        for (const auto& cap : KeyboardLayout::keycaps()) {
            float glow = 0.0f;
            auto it = physics.keyGlow.find(cap.key);
            if (showKeyDetails && it != physics.keyGlow.end()) glow = it->second;

            // 180° rotated to face the cat
            float su0 = (1.0f - (cap.xUnits + cap.widthUnits)) + padU;
            float su1 = (1.0f - cap.xUnits) - padU;
            float sv0 = (1.0f - (cap.row + 1) / 5.0f) + padV;
            float sv1 = (1.0f - cap.row / 5.0f) - padV;

            float dy = glow * 1.9f;
            Pt kTL = KeyboardLayout::screenQuadPoint(su0, sv0); kTL.y += dy;
            Pt kTR = KeyboardLayout::screenQuadPoint(su1, sv0); kTR.y += dy;
            Pt kBR = KeyboardLayout::screenQuadPoint(su1, sv1); kBR.y += dy;
            Pt kBL = KeyboardLayout::screenQuadPoint(su0, sv1); kBL.y += dy;

            GraphicsPath kp;
            addQuadPoly(kp, kTL, kTR, kBR, kBL);
            const bool decorated = theme >= KeyboardTheme::PinkPaws;
            const bool space = cap.key == PhysicalKey::Space;
            Color fill = cap.isAmberAccent || (decorated && space) ? accentCol : keyCol;
            if (glow > 0.02f) fill = blendColor(fill, glowCol, glow * 0.55f);

            SolidBrush kb(fill);
            g.FillPath(&kb, &kp);
            g.DrawPath(&keyBorderPen, &kp);
            if (decorated && (space || cap.isAmberAccent)) {
                Pt center = KeyboardLayout::screenQuadPoint((su0 + su1) * 0.5f, (sv0 + sv1) * 0.5f);
                center.y += dy;
                drawKeyMotif(g, center, theme, baseCol, space ? 1.0f : 0.67f);
            }
        }

        if (!showKeyDetails) return;

        // Radiant backlit glow halos over pressed keys
        for (const auto& kv : physics.keyGlow) {
            if (kv.second <= 0.05f) continue;
            Pt c = KeyboardLayout::canvasPointForKey(kv.first);
            float r = 17.0f + kv.second * 6.0f;
            GraphicsPath gp;
            gp.AddEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);
            PathGradientBrush pgb(&gp);
            pgb.SetCenterColor(Color((BYTE)(92 * kv.second), glowCol.GetR(), glowCol.GetG(), glowCol.GetB()));
            Color surr[1] = { Color(0, glowCol.GetR(), glowCol.GetG(), glowCol.GetB()) };
            INT cnt = 1;
            pgb.SetSurroundColors(surr, &cnt);
            g.FillPath(&pgb, &gp);
        }
    }

    static void drawKeyMotif(Gdiplus::Graphics& g, Pt p, KeyboardTheme theme, Gdiplus::Color ink, float size) {
        using namespace Gdiplus;
        const GraphicsState state = g.Save();
        g.TranslateTransform(p.x, p.y);
        g.ScaleTransform(size, size);
        SolidBrush mark(ink);
        if (theme == KeyboardTheme::PinkPaws) {
            g.FillEllipse(&mark, -2.5f, -0.1f, 5.0f, 3.1f);
            g.FillEllipse(&mark, -4.2f, -2.0f, 2.2f, 2.3f);
            g.FillEllipse(&mark, -1.2f, -3.3f, 2.4f, 2.4f);
            g.FillEllipse(&mark, 2.0f, -2.0f, 2.2f, 2.3f);
        } else if (theme == KeyboardTheme::FishBones) {
            Pen bone(ink, 0.95f);
            bone.SetStartCap(LineCapRound);
            bone.SetEndCap(LineCapRound);
            g.DrawLine(&bone, -4.8f, 0.0f, 5.0f, 0.0f);
            for (float x : {-1.5f, 1.0f}) {
                g.DrawLine(&bone, x, -2.3f, x + 1.0f, 0.0f);
                g.DrawLine(&bone, x + 1.0f, 0.0f, x, 2.3f);
            }
            PointF tail[] = {{4.0f, 0.0f}, {7.0f, -2.7f}, {7.0f, 2.7f}};
            g.FillPolygon(&mark, tail, 3);
            g.FillEllipse(&mark, -7.2f, -1.8f, 3.6f, 3.6f);
        } else {
            PointF cat[] = {{-4.2f, 2.1f}, {-4.2f, -3.1f}, {-1.2f, -1.3f},
                            {1.2f, -1.3f}, {4.2f, -3.1f}, {4.2f, 2.1f}};
            g.FillPolygon(&mark, cat, 6);
        }
        g.Restore(state);
    }

    static void drawFloatingKeycaps(Gdiplus::Graphics& g, const CatPhysics& physics) {
        using namespace Gdiplus;
        FontFamily ff(L"Segoe UI");
        FontFamily mouseFont(L"Microsoft YaHei UI");
        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);

        for (const auto& b : physics.floatingKeycaps) {
            float prog = b.age / b.lifetime;
            float scale = 1.0f;
            if (b.age < 0.09f) {
                scale = 0.72f + 0.38f * std::sin((b.age / 0.09f) * 3.14159f * 0.65f);
            } else if (b.age < 0.18f) {
                scale = 1.10f - 0.10f * ((b.age - 0.09f) / 0.09f);
            }

            float alphaF = prog < 0.62f ? 1.0f : std::max(0.0f, 1.0f - (prog - 0.62f) / 0.38f);
            BYTE a = (BYTE)(alphaF * 255.0f);
            if (a < 4) continue;

            size_t len = b.label.size();
            float w = b.isMouse ? 42.0f : (len <= 1 ? 26.0f : (len == 2 ? 32.0f : 41.0f));
            float h = 22.0f;

            GraphicsState st = g.Save();
            g.TranslateTransform(b.position.x, b.position.y);
            g.RotateTransform(b.rotation * RadToDeg);
            g.ScaleTransform(scale, scale);

            GraphicsPath basePath;
            addRoundedRect(basePath, -w * 0.5f, -h * 0.5f + 2.5f, w, h, 5.5f);
            SolidBrush baseBrush(Color((BYTE)(alphaF * 242), 38, 38, 43));
            g.FillPath(&baseBrush, &basePath);

            GraphicsPath facePath;
            addRoundedRect(facePath, -w * 0.5f, -h * 0.5f, w, h, 5.2f);
            SolidBrush faceBrush(b.isMouse ? Color(a, 233, 246, 255) : Color(a, 249, 248, 243));
            Pen facePen(b.isMouse ? Color(a, 42, 93, 130) : Color(a, 41, 41, 46), 1.45f);
            g.FillPath(&faceBrush, &facePath);
            g.DrawPath(&facePen, &facePath);

            Font font(b.isMouse && mouseFont.IsAvailable() ? &mouseFont : &ff,
                      b.isMouse || len <= 1 ? 11.5f : 10.4f, FontStyleBold, UnitPixel);
            SolidBrush txtBrush(Color(a, 31, 31, 36));
            g.DrawString(b.label.c_str(), -1, &font, RectF(-w * 0.5f, -h * 0.5f, w, h), &sf, &txtBrush);

            g.Restore(st);
        }
    }

    // MARK: - Petting Sparkles & Sleep Z

    static void drawSparkles(Gdiplus::Graphics& g, const CatPhysics& physics, EyeColorTheme eyeTheme) {
        using namespace Gdiplus;
        Color baseCol = eyePrimary(eyeTheme);
        FontFamily ff(L"Segoe UI");

        for (const auto& sp : physics.sparkles) {
            float p = std::min(1.0f, sp.age / sp.lifetime);
            BYTE a = (BYTE)(std::sin(p * 3.14159f) * 215.0f);
            if (a < 4) continue;

            Color c(a, baseCol.GetR(), baseCol.GetG(), baseCol.GetB());
            if (sp.isSleepZ) {
                constexpr float size = 16.0f;
                GraphicsPath letter;
                StringFormat fmt;
                letter.AddString(L"Z", 1, &ff, FontStyleBold, size + p * 4.0f,
                                 PointF(sp.position.x, sp.position.y), &fmt);
                Pen outline(Color(a, 49, 41, 45), 1.65f);
                outline.SetLineJoin(LineJoinRound);
                SolidBrush fill(c);
                g.DrawPath(&outline, &letter);
                g.FillPath(&fill, &letter);
            } else {
                float r = 3.5f + p * 1.5f;
                GraphicsState st = g.Save();
                g.TranslateTransform(sp.position.x, sp.position.y);
                GraphicsPath star;
                addQuadBezier(star, {0, -r}, {0.8f, -0.8f}, {r, 0});
                addQuadBezier(star, {r, 0}, {0.8f, 0.8f}, {0, r});
                addQuadBezier(star, {0, r}, {-0.8f, 0.8f}, {-r, 0});
                addQuadBezier(star, {-r, 0}, {-0.8f, -0.8f}, {0, -r});
                star.CloseFigure();
                SolidBrush sb(c);
                g.FillPath(&sb, &star);
                g.Restore(st);
            }
        }
    }
};
