#pragma once
#include <gdiplus.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include "CatPhysics.hpp"

namespace FlatCatRenderer {

using namespace Gdiplus;

// Coordinates are kept in the illustration's original units.
inline constexpr float Scale = 0.24f;
inline constexpr float OriginX = 665.0f;
inline constexpr float OriginY = 852.0f;
inline constexpr float MotionAmount = CatPhysics::BodyMotionAmount;
inline constexpr float BodyDrop = 6.5f;
inline constexpr float PointFaceCenter = 673.0f;

inline float seatX(const CatPhysics& cat) {
    return CatPhysics::SeatAnchor.x + cat.bodyShiftX.value * MotionAmount;
}

inline Pt headOffset(const CatPhysics& cat, float breath) {
    return {(cat.bodyTilt.value * 27.0f + cat.headYaw.value * 10.0f) * MotionAmount,
            (cat.headOffsetY.value + cat.headPitch.value * 4.8f) * MotionAmount + breath};
}

struct HeadPose {
    Matrix transform;

    HeadPose(const CatPhysics& cat, float breath) {
        const Pt offset = headOffset(cat, breath);
        const float yaw = cat.headYaw.value;
        const float pitch = cat.headPitch.value;
        transform.Translate(OriginX + offset.x / Scale, 489.0f + offset.y / Scale);
        transform.Rotate(cat.headTilt.value * MotionAmount * 57.29578f);
        transform.Scale(1.0f - 0.12f * MotionAmount * yaw * yaw,
                        1.0f - 0.08f * MotionAmount * pitch * pitch);
        transform.Translate(-OriginX, -489.0f);
    }
};

template <class Motion>
inline void deformPath(GraphicsPath& path, Motion motion) {
    const INT count = path.GetPointCount();
    std::vector<PointF> points(count);
    std::vector<BYTE> types(count);
    path.GetPathPoints(points.data(), count);
    path.GetPathTypes(types.data(), count);
    motion(points.data(), count);
    GraphicsPath displaced(points.data(), types.data(), count);
    path.Reset();
    path.AddPath(&displaced, FALSE);
}

inline const Color ink(52, 43, 34);
inline const Color cream(255, 249, 237);
inline const Color orange(240, 165, 91);
inline const Color charcoal(77, 69, 66);
inline const Color pink(245, 174, 164);

struct Palette {
    Color body, outline, limbLine, hindPaw, tail, marking, sock, ear, whisker;
};

inline Palette palette(CatCoatTheme coat) {
    switch (coat) {
        case CatCoatTheme::ClassicBlack:
            return {{44, 44, 47}, {25, 24, 25}, {108, 105, 107}, {38, 37, 40},
                    {44, 44, 47}, {44, 44, 47}, {44, 44, 47}, pink, {252, 246, 239}};
        case CatCoatTheme::Tuxedo:
            return {{255, 251, 244}, {35, 33, 34}, {35, 33, 34}, {240, 235, 228},
                    {52, 50, 52}, {52, 50, 52}, {255, 251, 244}, pink, {255, 249, 242}};
        case CatCoatTheme::BritishBlue:
            return {{126, 135, 159}, {43, 46, 58}, {50, 52, 64}, {112, 122, 148},
                    {126, 135, 159}, {126, 135, 159}, {126, 135, 159}, pink, {251, 247, 243}};
        case CatCoatTheme::Ragdoll:
            return {{253, 244, 231}, {63, 48, 40}, {71, 56, 47}, {239, 224, 206},
                    {158, 128, 113}, {154, 122, 106}, {253, 244, 231}, pink, {255, 249, 239}};
        case CatCoatTheme::Siamese:
            return {{253, 236, 216}, {61, 43, 34}, {66, 47, 38}, {114, 84, 69},
                    {88, 63, 51}, {107, 78, 63}, {110, 80, 65}, {107, 78, 63}, {255, 242, 225}};
        case CatCoatTheme::GingerTabby:
            return {{253, 174, 85}, {73, 45, 28}, {73, 45, 28}, {245, 218, 185},
                    {253, 174, 85}, {218, 123, 51}, {255, 239, 215}, {247, 177, 146}, {255, 248, 231}};
        default:
            return {cream, ink, ink, {248, 232, 211}, cream, charcoal, cream, pink, cream};
    }
}

inline void brushPath(Graphics& g, GraphicsPath& path, Color color) {
    SolidBrush brush(color);
    g.FillPath(&brush, &path);
}

inline void strokePath(Graphics& g, GraphicsPath& path, Color color, float width) {
    Pen pen(color, width);
    pen.SetLineJoin(LineJoinRound);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    g.DrawPath(&pen, &path);
}

inline void ellipse(Graphics& g, Color color, float x, float y, float w, float h) {
    SolidBrush brush(color);
    g.FillEllipse(&brush, x, y, w, h);
}

inline void tail(Graphics& g, CatCoatTheme coat, const Palette& fur) {
    GraphicsPath shape;
    shape.AddBezier(840.0f, 839.0f, 871.0f, 780.0f, 924.0f, 641.0f, 995.0f, 653.0f);
    shape.AddBezier(995.0f, 653.0f, 1044.0f, 657.0f, 1073.0f, 695.0f, 1045.0f, 733.0f);
    shape.AddBezier(1045.0f, 733.0f, 1016.0f, 754.0f, 1001.0f, 754.0f, 967.0f, 823.0f);
    shape.AddBezier(967.0f, 823.0f, 944.0f, 869.0f, 919.0f, 900.0f, 883.0f, 888.0f);
    shape.AddBezier(883.0f, 888.0f, 862.0f, 881.0f, 850.0f, 860.0f, 840.0f, 839.0f);
    shape.CloseFigure();
    brushPath(g, shape, fur.tail);
    if (coat != CatCoatTheme::Calico && coat != CatCoatTheme::Tuxedo &&
        coat != CatCoatTheme::GingerTabby) {
        strokePath(g, shape, fur.outline, 14.5f);
        return;
    }
    const auto state = g.Save();
    g.SetClip(&shape, CombineModeIntersect);
    if (coat == CatCoatTheme::GingerTabby) {
        for (float step : {0.0f, 47.0f, 94.0f}) {
            GraphicsPath stripe;
            stripe.AddBezier(862.0f + step, 735.0f - step * 0.80f,
                             891.0f + step, 746.0f - step * 0.80f,
                             924.0f + step, 781.0f - step * 0.80f,
                             941.0f + step, 823.0f - step * 0.80f);
            strokePath(g, stripe, fur.marking, 27.0f);
        }
    }
    if (coat == CatCoatTheme::Calico) {
        GraphicsPath band;
        band.AddBezier(910.0f, 665.0f, 912.0f, 700.0f, 941.0f, 733.0f, 986.0f, 729.0f);
        band.AddBezier(986.0f, 729.0f, 1009.0f, 697.0f, 989.0f, 651.0f, 970.0f, 626.0f);
        band.AddLine(970.0f, 626.0f, 910.0f, 665.0f);
        band.CloseFigure();
        brushPath(g, band, orange);
    }
    GraphicsPath tip;
    tip.AddBezier(942.0f, 638.0f, 979.0f, 651.0f, 990.0f, 696.0f, 986.0f, 729.0f);
    tip.AddBezier(986.0f, 729.0f, 1012.0f, 742.0f, 1012.0f, 765.0f, 1028.0f, 781.0f);
    tip.AddLine(1028.0f, 781.0f, 1098.0f, 655.0f);
    tip.CloseFigure();
    brushPath(g, tip, coat == CatCoatTheme::Calico ? fur.marking : fur.sock);
    g.Restore(state);
    strokePath(g, shape, fur.outline, 14.5f);
}

inline void drawCalicoFace(Graphics& g) {
    GraphicsPath left;
    left.AddBezier(480.0f, 302.0f, 564.0f, 267.0f, 637.0f, 328.0f, 688.0f, 379.0f);
    left.AddBezier(688.0f, 379.0f, 686.0f, 429.0f, 669.0f, 435.0f, 678.0f, 461.0f);
    left.AddBezier(678.0f, 461.0f, 696.0f, 566.0f, 590.0f, 623.0f, 488.0f, 598.0f);
    left.AddLine(488.0f, 598.0f, 440.0f, 454.0f);
    left.CloseFigure();
    GraphicsPath right;
    right.AddBezier(677.0f, 359.0f, 704.0f, 330.0f, 759.0f, 273.0f, 821.0f, 286.0f);
    right.AddLine(821.0f, 286.0f, 902.0f, 511.0f);
    right.AddBezier(902.0f, 511.0f, 888.0f, 608.0f, 789.0f, 620.0f, 751.0f, 551.0f);
    right.AddBezier(751.0f, 551.0f, 717.0f, 499.0f, 742.0f, 472.0f, 700.0f, 440.0f);
    right.AddBezier(700.0f, 440.0f, 674.0f, 418.0f, 678.0f, 390.0f, 677.0f, 359.0f);
    right.CloseFigure();
    brushPath(g, left, orange);
    brushPath(g, right, charcoal);
}

inline void drawEars(Graphics& g, Color color) {
    GraphicsPath earL;
    earL.AddBezier(522.0f, 438.0f, 517.0f, 408.0f, 523.0f, 318.0f, 553.0f, 322.0f);
    earL.AddBezier(553.0f, 322.0f, 579.0f, 324.0f, 608.0f, 365.0f, 629.0f, 391.0f);
    earL.AddBezier(629.0f, 391.0f, 589.0f, 400.0f, 530.0f, 454.0f, 522.0f, 438.0f);
    earL.CloseFigure();
    brushPath(g, earL, color);
    GraphicsPath earR;
    earR.AddBezier(742.0f, 394.0f, 760.0f, 366.0f, 781.0f, 323.0f, 798.0f, 321.0f);
    earR.AddBezier(798.0f, 321.0f, 816.0f, 318.0f, 839.0f, 416.0f, 831.0f, 441.0f);
    earR.AddBezier(831.0f, 441.0f, 826.0f, 454.0f, 801.0f, 427.0f, 742.0f, 394.0f);
    earR.CloseFigure();
    brushPath(g, earR, color);
}

template <class Motion>
inline void drawCalicoFlanks(Graphics& g, Motion motion) {
    ellipse(g, orange, 402.0f, 627.0f, 129.0f, 116.0f);
    ellipse(g, orange, 824.0f, 681.0f, 106.0f, 118.0f);
    GraphicsPath side;
    side.AddBezier(872.0f, 614.0f, 831.0f, 626.0f, 793.0f, 651.0f, 786.0f, 685.0f);
    side.AddBezier(786.0f, 685.0f, 777.0f, 712.0f, 799.0f, 734.0f, 825.0f, 742.0f);
    side.AddBezier(825.0f, 742.0f, 868.0f, 714.0f, 885.0f, 721.0f, 905.0f, 714.0f);
    side.CloseFigure();
    deformPath(side, motion);
    brushPath(g, side, charcoal);
}

inline void softPatch(Graphics& g, GraphicsPath& patch, Color color) {
    const float widths[] = {20.0f, 14.0f, 8.0f, 4.0f};
    const BYTE opacity[] = {25, 55, 100, 160};
    for (size_t i = 0; i < std::size(widths); ++i) {
        strokePath(g, patch, Color(opacity[i], color.GetR(), color.GetG(), color.GetB()), widths[i]);
    }
    brushPath(g, patch, color);
}

inline void drawPointEars(Graphics& g, Color color) {
    GraphicsPath crown;
    crown.AddLine(457.0f, 263.0f, 879.0f, 263.0f);
    const float cx = PointFaceCenter;
    crown.AddLine(879.0f, 263.0f, cx + 196.0f, 484.0f);
    crown.AddBezier(cx + 196.0f, 484.0f, cx + 123.0f, 356.0f,
                    cx - 123.0f, 356.0f, cx - 196.0f, 484.0f);
    crown.CloseFigure();
    brushPath(g, crown, color);
}

inline void drawRagdollMarkings(Graphics& g, Color color) {
    // Trace half of the mask from the forehead to the white blaze, then mirror it.
    const PointF half[] = {
        {0.0f, 406.0f},
        {79.0f, 406.0f}, {143.0f, 444.0f}, {159.0f, 510.0f},
        {174.0f, 570.0f}, {156.0f, 599.0f}, {119.0f, 589.0f},
        {66.0f, 575.0f}, {43.0f, 542.0f}, {37.0f, 487.0f},
        {33.0f, 450.0f}, {18.0f, 428.0f}, {0.0f, 428.0f}
    };
    const auto point = [&](size_t index, float sign) {
        return PointF(PointFaceCenter + half[index].X * sign, half[index].Y);
    };
    GraphicsPath mask;
    for (size_t i = 0; i + 3 < std::size(half); i += 3) {
        mask.AddBezier(point(i, 1.0f), point(i + 1, 1.0f), point(i + 2, 1.0f), point(i + 3, 1.0f));
    }
    for (size_t i = std::size(half) - 1; i >= 3; i -= 3) {
        mask.AddBezier(point(i, -1.0f), point(i - 1, -1.0f), point(i - 2, -1.0f), point(i - 3, -1.0f));
    }
    mask.CloseFigure();
    softPatch(g, mask, color);
}

inline void drawTuxedoMask(Graphics& g, Color color) {
    GraphicsPath mask;
    mask.AddLine(453.0f, 262.0f, 888.0f, 262.0f);
    mask.AddLine(888.0f, 262.0f, 899.0f, 594.0f);
    mask.AddBezier(899.0f, 594.0f, 815.0f, 642.0f, 742.0f, 585.0f, 724.0f, 506.0f);
    mask.AddBezier(724.0f, 506.0f, 716.0f, 462.0f, 704.0f, 420.0f, 698.0f, 425.0f);
    mask.AddBezier(698.0f, 425.0f, 687.0f, 430.0f, 695.0f, 540.0f, 638.0f, 575.0f);
    mask.AddBezier(638.0f, 575.0f, 582.0f, 618.0f, 498.0f, 616.0f, 458.0f, 590.0f);
    mask.CloseFigure();
    brushPath(g, mask, color);
}

inline void drawFaceMarkings(Graphics& g, CatCoatTheme coat, const Palette& fur) {
    switch (coat) {
        case CatCoatTheme::Calico:
            drawCalicoFace(g);
            break;
        case CatCoatTheme::Tuxedo:
            drawTuxedoMask(g, fur.marking);
            break;
        case CatCoatTheme::Ragdoll:
            drawPointEars(g, fur.tail);
            drawRagdollMarkings(g, fur.marking);
            break;
        case CatCoatTheme::Siamese: {
            drawPointEars(g, fur.ear);
            GraphicsPath mask;
            mask.AddEllipse(PointFaceCenter - 157.0f, 425.0f, 314.0f, 187.0f);
            softPatch(g, mask, fur.marking);
            break;
        }
        case CatCoatTheme::GingerTabby:
            for (float x : {640.0f, 683.0f, 726.0f}) {
                GraphicsPath stripe;
                stripe.AddBezier(x - 6.0f, 374.0f, x - 3.0f, 386.0f, x + 5.0f, 407.0f, x + 4.0f, 420.0f);
                strokePath(g, stripe, fur.marking, 23.0f);
            }
            break;
        default:
            break;
    }
}

template <class Motion>
inline void drawFlanks(Graphics& g, CatCoatTheme coat, const Palette& fur, Motion motion) {
    if (coat == CatCoatTheme::Calico) {
        drawCalicoFlanks(g, motion);
    } else if (coat == CatCoatTheme::Tuxedo) {
        for (float x : {355.0f, 785.0f}) {
            GraphicsPath spot;
            spot.AddEllipse(x, 619.0f, 177.0f, 174.0f);
            deformPath(spot, motion);
            brushPath(g, spot, fur.marking);
        }
    } else if (coat == CatCoatTheme::GingerTabby) {
        for (float side : {-1.0f, 1.0f}) {
            for (float y : {497.0f, 553.0f, 636.0f, 691.0f}) {
                const float edge = side < 0.0f ? 475.0f : 874.0f;
                const float length = y == 553.0f ? 35.0f : 65.0f;
                GraphicsPath stripe;
                stripe.AddBezier(edge, y, edge - side * 23.0f, y + 10.0f,
                                 edge - side * 44.0f, y + 16.0f, edge - side * length, y + 17.0f);
                deformPath(stripe, motion);
                strokePath(g, stripe, fur.marking, y < 600.0f ? 21.0f : 27.0f);
            }
        }
        GraphicsPath bib;
        bib.AddBezier(692.0f, 535.0f, 708.0f, 532.0f, 719.0f, 573.0f, 804.0f, 596.0f);
        bib.AddBezier(804.0f, 596.0f, 849.0f, 606.0f, 794.0f, 636.0f, 761.0f, 647.0f);
        bib.AddBezier(761.0f, 647.0f, 777.0f, 705.0f, 803.0f, 778.0f, 779.0f, 870.0f);
        bib.AddLine(779.0f, 870.0f, 542.0f, 870.0f);
        bib.AddBezier(542.0f, 870.0f, 539.0f, 754.0f, 576.0f, 681.0f, 594.0f, 643.0f);
        bib.AddBezier(594.0f, 643.0f, 546.0f, 632.0f, 507.0f, 608.0f, 540.0f, 601.0f);
        bib.AddBezier(540.0f, 601.0f, 635.0f, 581.0f, 672.0f, 536.0f, 692.0f, 535.0f);
        bib.CloseFigure();
        deformPath(bib, motion);
        brushPath(g, bib, fur.sock);
    }
}

inline void catBody(Graphics& g, const HeadPose& pose, GraphicsPath& outline, CatCoatTheme coat, const Palette& fur) {
    outline.AddBezier(497.0f, 480.0f, 494.0f, 409.0f, 512.0f, 287.0f, 554.0f, 298.0f);
    outline.AddBezier(554.0f, 298.0f, 588.0f, 304.0f, 628.0f, 346.0f, 649.0f, 379.0f);
    outline.AddBezier(649.0f, 379.0f, 664.0f, 375.0f, 686.0f, 375.0f, 700.0f, 379.0f);
    outline.AddBezier(700.0f, 379.0f, 727.0f, 337.0f, 761.0f, 292.0f, 791.0f, 296.0f);
    outline.AddBezier(791.0f, 296.0f, 836.0f, 300.0f, 847.0f, 414.0f, 852.0f, 474.0f);
    outline.AddBezier(852.0f, 474.0f, 884.0f, 512.0f, 883.0f, 571.0f, 845.0f, 605.0f);
    outline.AddBezier(845.0f, 605.0f, 864.0f, 659.0f, 898.0f, 729.0f, 871.0f, 806.0f);
    outline.AddBezier(871.0f, 806.0f, 860.0f, 842.0f, 842.0f, 860.0f, 796.0f, 864.0f);
    outline.AddBezier(796.0f, 864.0f, 744.0f, 872.0f, 625.0f, 871.0f, 565.0f, 851.0f);
    outline.AddBezier(565.0f, 851.0f, 453.0f, 827.0f, 430.0f, 755.0f, 498.0f, 604.0f);
    outline.AddBezier(498.0f, 604.0f, 461.0f, 576.0f, 465.0f, 524.0f, 497.0f, 480.0f);
    outline.CloseFigure();
    // Blend the head movement down the shoulders while keeping the bottom seated.
    auto headMotion = [&](PointF* points, INT count) {
        std::vector<PointF> moved(points, points + count);
        pose.transform.TransformPoints(moved.data(), count);
        for (INT i = 0; i < count; ++i) {
            const float t = std::clamp((755.0f - points[i].Y) / 145.0f, 0.0f, 1.0f);
            const float follow = t * t * (3.0f - 2.0f * t);
            points[i].X += (moved[i].X - points[i].X) * follow;
            points[i].Y += (moved[i].Y - points[i].Y) * follow;
        }
    };
    deformPath(outline, headMotion);
    brushPath(g, outline, fur.body);
    const auto clipped = g.Save();
    g.SetClip(&outline, CombineModeIntersect);
    drawFlanks(g, coat, fur, headMotion);
    const auto face = g.Save();
    g.MultiplyTransform(&pose.transform);
    drawFaceMarkings(g, coat, fur);
    if (coat != CatCoatTheme::Siamese) drawEars(g, fur.ear);
    g.Restore(face);
    g.Restore(clipped);
    strokePath(g, outline, fur.outline, 14.5f);
}

inline Color eyeColor(EyeColorTheme theme, CatCoatTheme coat) {
    switch (theme) {
        case EyeColorTheme::SiameseBlue: return Color(138, 200, 230);
        case EyeColorTheme::IceCyan: return Color(152, 222, 223);
        case EyeColorTheme::NeonChartreuse:
            return coat == CatCoatTheme::ClassicBlack || coat == CatCoatTheme::Tuxedo
                ? Color(234, 226, 110) : Color(194, 224, 114);
        case EyeColorTheme::WarmAmber:
            return coat == CatCoatTheme::BritishBlue ? Color(255, 211, 118) : Color(240, 222, 124);
        case EyeColorTheme::Ivory: return Color(255, 253, 247);
    }
    return Color(240, 222, 124);
}

inline void eye(Graphics& g, float x, float y, float width, float height,
                const CatPhysics& cat, Color iris, Color outline) {
    const float closure = cat.effectiveEyeClosure();
    if (closure > 0.88f) {
        GraphicsPath lid;
        const float curve = cat.isHappyClosedEyes() ? -20.0f : 9.0f;
        lid.AddBezier(x + 6.0f, y, x + width * 0.32f, y + curve,
                      x + width * 0.68f, y + curve, x + width - 6.0f, y);
        strokePath(g, lid, outline, 7.4f);
        return;
    }

    const auto state = g.Save();
    g.TranslateTransform(x, y);
    g.ScaleTransform(1.0f, std::max(0.10f, 1.0f - closure));
    GraphicsPath eyeShape;
    eyeShape.AddEllipse(0.0f, -height * 0.5f, width, height);
    brushPath(g, eyeShape, iris);
    const auto pupilClip = g.Save();
    g.SetClip(&eyeShape, CombineModeIntersect);

    const Pt gaze = cat.eyeGaze.point();
    const Pt reflection = cat.eyeReflection.point();
    const float horizontalTravel = gaze.x < 0.0f ? 0.51f : 0.30f;
    const float pupilX = width * std::clamp(0.70f + gaze.x * horizontalTravel, 0.25f, 0.80f);
    const float pupilY = height * std::clamp(-0.095f + gaze.y * 0.20f, -0.24f, 0.19f);
    const float size = std::clamp(cat.pupilScale.value, 0.90f, 1.12f);
    const float rx = width * 0.28f * size;
    const float ry = height * 0.38f * size;
    GraphicsPath pupil;
    pupil.AddEllipse(pupilX - rx, pupilY - ry, 2.0f * rx, 2.0f * ry);
    brushPath(g, pupil, Color(37, 33, 27));
    // The single catchlight remains inside both the pupil and the eye opening.
    g.SetClip(&pupil, CombineModeIntersect);
    const float glintX = std::max(width * 0.18f,
        pupilX - 8.0f + 9.0f * reflection.x + 2.7f * std::sin(cat.time * 1.4f));
    const float glintY = pupilY - 14.0f + 7.0f * reflection.y + 2.4f * std::cos(cat.time * 1.2f);
    ellipse(g, Color(255, 252, 246), glintX - 7.0f, glintY - 9.0f, 14.0f, 18.0f);
    g.Restore(pupilClip);
    strokePath(g, eyeShape, outline, 6.4f);
    g.Restore(state);
}

inline void whiskers(Graphics& g, Color color, bool centered) {
    const std::array<float, 3> start = {523.0f, 548.0f, 571.0f};
    const std::array<float, 3> end = {541.0f, 554.0f, 569.0f};
    const std::array<float, 3> extent = {141.0f, 135.0f, 119.0f};
    for (int side : {-1,1}) {
        for (size_t i = 0; i < start.size(); ++i) {
            const float outside = centered ? PointFaceCenter + side * extent[i] : (side < 0 ? 534.0f : 835.0f);
            const float inside = centered ? PointFaceCenter + side * 87.0f : (side < 0 ? 596.0f : 785.0f);
            GraphicsPath line;
            line.AddBezier(outside, start[i], outside - side * 18.0f, start[i] - 1.5f,
                           inside + side * 16.0f, end[i] - 5.0f, inside, end[i]);
            strokePath(g, line, color, 5.2f);
        }
    }
}

inline PointF toArt(Pt point, float seatX, float seatY) {
    return {OriginX + (point.x - seatX) / Scale,
            OriginY + (point.y - seatY) / Scale};
}

inline PointF pawPosition(float seatX, Pt position, Pt rest, PointF idle) {
    constexpr float bodySeatY = CatPhysics::SeatAnchor.y + BodyDrop;
    const PointF at = toArt(position, seatX, bodySeatY);
    const PointF resting = toArt(rest, CatPhysics::SeatAnchor.x, bodySeatY);
    const float reach = std::clamp(std::hypot(position.x - rest.x, position.y - rest.y) / 26.0f,
                                   0.0f, 1.0f);
    return {at.X + (idle.X - resting.X) * (1.0f - reach),
            at.Y + (idle.Y - resting.Y) * (1.0f - reach)};
}

inline void toeLines(Graphics& g, const Matrix& transform, float spread, float depth, Color color) {
    const auto state = g.Save();
    g.MultiplyTransform(&transform);
    for (float toe : {-spread, spread}) {
        GraphicsPath crease;
        crease.AddBezier(toe, depth - 14.0f, toe + 2.0f, depth - 11.0f,
                         toe + 4.0f, depth - 5.0f, toe + 3.0f, depth);
        strokePath(g, crease, color, 5.5f);
    }
    g.Restore(state);
}

inline void hindPaw(Graphics& g, float x, float y, float angle, const Palette& fur) {
    Matrix pose;
    pose.Translate(x, y);
    pose.Rotate(angle);
    const auto state = g.Save();
    g.MultiplyTransform(&pose);
    GraphicsPath foot;
    foot.AddEllipse(-46.0f, -27.0f, 92.0f, 54.0f);
    brushPath(g, foot, fur.hindPaw);
    strokePath(g, foot, fur.outline, 8.5f);
    g.Restore(state);
    toeLines(g, pose, 13.0f, 24.0f, fur.outline);
}

inline void foreleg(Graphics& g, PointF paw, bool left, GraphicsPath& body, CatCoatTheme coat,
                    const Palette& fur) {
    const PointF shoulder = left ? PointF(602.0f, 752.0f) : PointF(743.0f, 728.0f);
    const float dx = paw.X - shoulder.X;
    const float dy = paw.Y - shoulder.Y;
    const float distance = std::hypot(dx, dy);
    if (distance < 1.0f) return;
    const float ux = dx / distance;
    const float uy = dy / distance;
    const float side = left ? 1.0f : -1.0f;
    const auto point = [&](float across, float along) {
        return PointF(paw.X + uy * side * across + ux * along,
                      paw.Y - ux * side * across + uy * along);
    };

    // Measure the sleeve across its direction of travel, including diagonal reaches.
    constexpr float shoulderWidth = 29.0f;
    constexpr float elbowWidth = 44.0f;
    constexpr float wristWidth = 33.0f;
    const float start = -distance;
    const float elbow = start * 0.72f;
    const float wrist = start * 0.29f;
    GraphicsPath inner;
    inner.AddBezier(point(shoulderWidth, start), point(elbowWidth, elbow),
                    point(wristWidth, wrist), point(wristWidth, -11.0f));
    GraphicsPath cap;
    cap.AddBezier(point(wristWidth, -11.0f), point(44.0f, 18.0f),
                  point(29.0f, 38.0f), point(0.0f, 39.0f));
    cap.AddBezier(point(0.0f, 39.0f), point(-29.0f, 40.0f),
                  point(-42.0f, 17.0f), point(-wristWidth, -11.0f));
    GraphicsPath outer;
    outer.AddBezier(point(-wristWidth, -11.0f), point(-wristWidth, wrist),
                    point(-elbowWidth, elbow), point(-shoulderWidth, start));
    GraphicsPath fill;
    fill.AddPath(&inner, FALSE);
    fill.AddPath(&cap, TRUE);
    fill.AddPath(&outer, TRUE);
    fill.CloseFigure();
    if (coat == CatCoatTheme::Siamese) {
        const float gradientLength = distance + 52.0f;
        LinearGradientBrush sock(point(0.0f, start - 6.0f), point(0.0f, 46.0f), fur.body, fur.sock);
        const Color colors[] = {fur.body, fur.body, fur.sock, fur.sock};
        const float featherStart = std::clamp(1.0f - 116.0f / gradientLength, 0.05f, 0.8f);
        const float featherEnd = std::clamp(1.0f - 62.0f / gradientLength, featherStart + 0.05f, 0.95f);
        const REAL stops[] = {0.0f, featherStart, featherEnd, 1.0f};
        sock.SetInterpolationColors(colors, stops, 4);
        g.FillPath(&sock, &fill);
    } else if (coat == CatCoatTheme::GingerTabby) {
        // Leave the upper legs continuous with the pale chest and wrap the paws in cream.
        LinearGradientBrush sleeve(point(0.0f, start - 6.0f), point(0.0f, 46.0f), fur.sock, fur.body);
        const Color colors[] = {fur.sock, fur.sock, fur.body, fur.body};
        const REAL stops[] = {0.0f, 0.15f, 0.50f, 1.0f};
        sleeve.SetInterpolationColors(colors, stops, 4);
        g.FillPath(&sleeve, &fill);
        const auto clipped = g.Save();
        g.SetClip(&fill, CombineModeIntersect);
        GraphicsPath sock;
        sock.AddBezier(point(-51.0f, 4.0f), point(-30.0f, -20.0f),
                       point(29.0f, -22.0f), point(51.0f, 5.0f));
        sock.AddLine(point(51.0f, 5.0f), point(51.0f, 49.0f));
        sock.AddLine(point(51.0f, 49.0f), point(-51.0f, 49.0f));
        sock.CloseFigure();
        brushPath(g, sock, fur.sock);
        g.Restore(clipped);
    } else {
        brushPath(g, fill, fur.body);
    }
    strokePath(g, inner, fur.limbLine, 8.5f);
    strokePath(g, cap, fur.outline, 8.5f);
    const auto state = g.Save();
    g.SetClip(&body, CombineModeExclude);
    strokePath(g, outer, fur.outline, 8.5f);
    g.Restore(state);

    const Matrix pose(uy, -ux, ux, uy, paw.X, paw.Y);
    toeLines(g, pose, 12.0f, 36.0f, fur.outline);
}

inline void drawTail(Graphics& g, float seatX, const CatPhysics& cat, CatCoatTheme coat, const Palette& fur) {
    const auto state = g.Save();
    const PointF root = toArt(cat.tailRoot(), seatX, CatPhysics::SeatAnchor.y);
    const PointF rest(1016.0f, 700.0f);
    const PointF target = toArt(cat.tailTip.point(), seatX, CatPhysics::SeatAnchor.y);
    const float reach = std::clamp(cat.tailReach.value, 0.0f, 1.0f);
    const float idleAngle = 8.0f * MotionAmount * (1.0f - 0.85f * cat.sleepiness) * std::sin(cat.tailPhase);
    const float restAngle = std::atan2(rest.Y - root.Y, rest.X - root.X);
    const float targetAngle = cat.tailTargetAngle();
    const float angle = idleAngle + (targetAngle - restAngle) * 57.29578f * reach
                      - idleAngle * reach;
    const float stretch = std::hypot(target.X - root.X, target.Y - root.Y)
                        / std::hypot(rest.X - root.X, rest.Y - root.Y);
    const float axis = restAngle * 57.29578f;
    g.TranslateTransform(root.X, root.Y);
    g.RotateTransform(angle + axis);
    g.ScaleTransform(std::max(0.15f, 1.0f + (stretch - 1.0f) * reach), 1.0f);
    g.RotateTransform(-axis);
    g.TranslateTransform(-root.X, -root.Y);
    tail(g, coat, fur);
    g.Restore(state);
}

inline void draw(Graphics& g, const CatPhysics& cat, CatCoatTheme coat, EyeColorTheme theme, float breath) {
    const HeadPose pose(cat, breath);
    const Palette fur = palette(coat);
    const float seat = seatX(cat);
    const auto canvas = g.Save();
    g.TranslateTransform(seat, CatPhysics::SeatAnchor.y);
    g.ScaleTransform(Scale, Scale);
    g.TranslateTransform(-OriginX, -OriginY);

    drawTail(g, seat, cat, coat, fur);
    g.TranslateTransform(0.0f, BodyDrop / Scale);
    GraphicsPath body;
    catBody(g, pose, body, coat, fur);

    const auto face = g.Save();
    g.MultiplyTransform(&pose.transform);
    const Color iris = eyeColor(theme, coat);
    const bool centeredFace = coat == CatCoatTheme::Siamese || coat == CatCoatTheme::Ragdoll;
    if (centeredFace) {
        constexpr float width = 85.0f;
        for (float offset : {-69.0f, 69.0f}) {
            eye(g, PointFaceCenter + offset - width * 0.5f, 495.0f, width, 84.0f, cat, iris, fur.outline);
        }
    } else {
        eye(g, 583.0f, 498.5f, 88.0f, 83.0f, cat, iris, fur.outline);
        eye(g, 723.0f, 491.5f, 82.0f, 85.0f, cat, iris, fur.outline);
    }
    whiskers(g, fur.whisker, centeredFace);
    g.Restore(face);

    hindPaw(g, 606.0f, 849.0f, -8.0f, fur);
    hindPaw(g, 741.0f, 855.0f, 8.0f, fur);
    foreleg(g, pawPosition(seat, cat.leftPaw.point(), CatPhysics::LeftPawRest, {627.0f, 846.0f}), true, body, coat, fur);
    foreleg(g, pawPosition(seat, cat.rightPaw.point(), CatPhysics::RightPawRest, {720.0f, 852.0f}), false, body, coat, fur);
    g.Restore(canvas);
}

} // namespace FlatCatRenderer
