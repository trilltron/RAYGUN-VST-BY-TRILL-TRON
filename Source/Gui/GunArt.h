// Static ray-gun artwork, drawn once into a cached 2x image. Pure vector + procedural texture (no bitmap assets).
#pragma once
#include "Look.h"

namespace rgui {
struct Art {
    static void shadow(Graphics& g, const Path& p, int r = 14, Point<int> off = { 4, 8 }, float a = 0.8f) { DropShadow(Colours::black.withAlpha(a), r, off).drawForPath(g, p); }

    // inner bevel: light on the top-left edge, dark on the bottom-right edge
    static void bevel(Graphics& g, const Path& p, float d = 2.4f, float hi = 0.55f, float lo = 0.6f) {
        Graphics::ScopedSaveState s(g); g.reduceClipRegion(p, {});
        g.setColour(Colours::white.withAlpha(hi)); g.strokePath(p, PathStrokeType(d), AffineTransform::translation(d * 0.7f, d * 0.9f));
        g.setColour(Colours::black.withAlpha(lo)); g.strokePath(p, PathStrokeType(d * 1.5f), AffineTransform::translation(-d * 0.7f, -d * 0.9f));
    }
    static void innerShadow(Graphics& g, const Path& p, float w = 10.f, float a = 0.6f) {
        Graphics::ScopedSaveState s(g); g.reduceClipRegion(p, {});
        for (int i = 0; i < 5; ++i) { g.setColour(Colours::black.withAlpha(a * (1.f - i / 5.f) * 0.5f)); g.strokePath(p, PathStrokeType(w * (1.f - i / 6.f) * 2.f)); }
    }
    static void texture(Graphics& g, const Path& p, const Image& tex, Point<float> origin, float alpha = 0.9f) {
        Graphics::ScopedSaveState s(g); g.reduceClipRegion(p, {}); g.setOpacity(alpha); g.drawImageAt(tex, (int) origin.x, (int) origin.y);
    }
    // chipped / worn paint along the edges + a few scratches
    static void wear(Graphics& g, const Path& p, int seed, Colour chip, int count = 90) {
        Random r(seed); float len = p.getLength(); if (len < 10.f) return; auto c = p.getBounds().getCentre(); Graphics::ScopedSaveState s(g); g.reduceClipRegion(p, {});
        for (int i = 0; i < count; ++i) {
            auto pt = p.getPointAlongPath(r.nextFloat() * len); auto v = c - pt; float l = std::max(1.f, v.getDistanceFromOrigin()); pt += v / l * (r.nextFloat() * 5.f);
            g.setColour(chip.withAlpha(0.25f + r.nextFloat() * 0.6f)); g.fillEllipse(pt.x, pt.y, 1.f + r.nextFloat() * 3.5f, 0.8f + r.nextFloat() * 2.2f);
            if (r.nextInt(7) == 0) { float a = r.nextFloat() * 6.28f, ll = 5.f + r.nextFloat() * 12.f; g.setColour(chip.withAlpha(0.35f)); g.drawLine(pt.x, pt.y, pt.x + std::cos(a) * ll, pt.y + std::sin(a) * ll, 0.8f); }
        }
    }
    static void brushed(Graphics& g, Rectangle<float> r, int seed, float strength = 0.09f) {
        Random rnd(seed); Graphics::ScopedSaveState s(g); g.reduceClipRegion(r.toNearestInt());
        for (int i = 0; i < (int) (r.getHeight() * 3); ++i) { float y = r.getY() + rnd.nextFloat() * r.getHeight(); bool l = rnd.nextBool(); g.setColour((l ? Colours::white : Colours::black).withAlpha(rnd.nextFloat() * strength)); g.drawHorizontalLine((int) y, r.getX() + rnd.nextFloat() * 20.f, r.getRight()); }
    }
    static ColourGradient redGrad(Point<float> a, Point<float> b) {
        ColourGradient cg(Colour(0xfff2505a), a.x, a.y, Colour(0xff5a0a11), b.x, b.y, false); cg.addColour(0.35, Colour(0xffd0262f)); cg.addColour(0.7, Colour(0xff8f141c)); return cg; }
    static ColourGradient goldGrad(float y0, float y1) {
        ColourGradient cg(Colour(0xfffff0b0), 0, y0, Colour(0xff6d4d12), 0, y1, false); cg.addColour(0.25, Colour(0xffe9c764)); cg.addColour(0.55, Colour(0xffb88a2a)); cg.addColour(0.8, Colour(0xff8c6420)); return cg; }
    static ColourGradient chromeV(float y0, float y1) {
        ColourGradient cg(Colour(0xff23272b), 0, y0, Colour(0xff15181a), 0, y1, false);
        cg.addColour(0.07, Colour(0xfff1f4f6)); cg.addColour(0.16, Colour(0xffb4bac0)); cg.addColour(0.3, Colour(0xff6a7076)); cg.addColour(0.5, Colour(0xff3f4449)); cg.addColour(0.68, Colour(0xff565c61)); cg.addColour(0.82, Colour(0xff8a9096)); cg.addColour(0.92, Colour(0xff2c3035)); return cg; }

    static void screw(Graphics& g, float x, float y, float r) {
        shadow(g, [&] { Path p; p.addEllipse(x - r, y - r, 2 * r, 2 * r); return p; }(), 3, { 1, 2 }, 0.8f);
        g.setGradientFill(ColourGradient(Colour(0xfffff0b0), x - r * 0.5f, y - r * 0.6f, Colour(0xff6d4d12), x + r, y + r, true)); g.fillEllipse(x - r, y - r, 2 * r, 2 * r);
        g.setColour(Colour(0xaa000000)); g.drawLine(x - r * 0.6f, y + r * 0.3f, x + r * 0.6f, y - r * 0.3f, 1.4f); g.setColour(Colour(0x66ffffff)); g.drawEllipse(x - r, y - r, 2 * r, 2 * r, 0.8f);
    }

    // ---------------------------------------------------------------- the whole gun (page-local coords, 1000 x 510)
    static void drawBody(Graphics& g, const Image& speck) {
        auto& F = Fonts::get();
        // spotlight behind the gun
        g.setGradientFill(ColourGradient(Colour(0xff35161a), 500, 250, Colour(0x00120a0b), 1000, 500, true)); g.fillRect(0, 0, 1000, 510);

        // ---- title (extruded gold)
        { GlyphArrangement ga; ga.addLineOfText(F.title(44.f).withExtraKerningFactor(0.12f), "RAYGUN", 18.f, 56.f); Path tp; ga.createPath(tp);
          g.setColour(Colour(0xaa000000)); g.fillPath(tp, AffineTransform::translation(3.f, 5.f));
          for (int i = 5; i >= 1; --i) { g.setColour(Colour(0xff6d4d12)); g.fillPath(tp, AffineTransform::translation(i * 0.6f, i * 0.8f)); }
          g.setGradientFill(goldGrad(14.f, 58.f)); g.fillPath(tp); g.strokePath(tp, PathStrokeType(2.2f)); }
        drawText(g, "by TRILL TRON", { 22, 62, 240, 18 }, F.title(13.f).withExtraKerningFactor(0.3f), Colour(0xaa000000), Justification::centredLeft);
        drawText(g, "by TRILL TRON", { 21, 61, 240, 18 }, F.title(13.f).withExtraKerningFactor(0.3f), kCream.withAlpha(0.92f), Justification::centredLeft);

        // ---- rear housing
        Path hous; hous.addRoundedRectangle(195.f, 77.f, 275.f, 254.f, 46.f);
        shadow(g, hous, 22, { 6, 12 }, 0.9f);
        g.setGradientFill(redGrad({ 215, 85 }, { 470, 331 })); g.fillPath(hous);
        texture(g, hous, speck, { 195, 77 }); wear(g, hous, 11, Colour(0xffe9b6a8), 110); innerShadow(g, hous, 12, 0.55f); bevel(g, hous, 3.f, 0.6f, 0.65f);
        { Path trim; trim.addRoundedRectangle(207.f, 89.f, 251.f, 230.f, 36.f); g.setColour(Colours::black.withAlpha(0.6f)); g.strokePath(trim, PathStrokeType(4.f), AffineTransform::translation(1, 2)); g.setGradientFill(goldGrad(89, 319)); g.strokePath(trim, PathStrokeType(2.6f)); }
        { Path hl; hl.startNewSubPath(236, 90); hl.lineTo(420, 90); g.setColour(Colour(0x77ffffff)); g.strokePath(hl, PathStrokeType(2.5f, PathStrokeType::curved, PathStrokeType::rounded)); }
        screw(g, 450, 150, 7); screw(g, 450, 280, 7); screw(g, 219, 100, 5); screw(g, 219, 308, 5);

        // ---- barrel (brushed chrome cylinder)
        Rectangle<float> barrel(470, 154, 364, 120); Path bp; bp.addRectangle(barrel);
        { Path sh; sh.addRectangle(barrel.translated(0, 8).reduced(4, 0)); shadow(g, sh, 14, { 0, 8 }, 0.9f); }
        g.setGradientFill(chromeV(barrel.getY(), barrel.getBottom())); g.fillRect(barrel); brushed(g, barrel, 5, 0.1f);
        g.setColour(Colour(0x66000000)); g.fillRect(barrel.getX(), barrel.getBottom() - 10, barrel.getWidth(), 10.f);
        for (int i = 0; i <= 8; ++i) { float x = 470.f + i * 45.2f; if (i % 2 == 0 && i > 0 && i < 8) { Rectangle<float> ring(x - 3.5f, 154.f, 7.f, 120.f); g.setGradientFill(ColourGradient(Colour(0xff6d4d12), ring.getX(), 0, Colour(0xff6d4d12), ring.getRight(), 0, false));
                g.fillRect(ring); ColourGradient cg(Colour(0xff6d4d12), ring.getX(), 0, Colour(0xff6d4d12), ring.getRight(), 0, false); cg.addColour(0.35, Colour(0xfffff0b0)); cg.addColour(0.7, Colour(0xffc99a36)); g.setGradientFill(cg); g.fillRect(ring); }
            else { g.setColour(Colour(0x77000000)); g.fillRect(x - 0.5f, 154.f, 1.f, 120.f); g.setColour(Colour(0x33ffffff)); g.fillRect(x + 0.5f, 154.f, 1.f, 120.f); } }
        g.setColour(Colour(0x99000000)); g.drawRect(barrel, 1.5f); bevel(g, bp, 2.f, 0.4f, 0.5f);

        // ---- end cap
        Path cap; cap.addRoundedRectangle(832.f, 144.f, 30.f, 140.f, 9.f); shadow(g, cap, 10, { 4, 6 }, 0.8f);
        g.setGradientFill(redGrad({ 832, 144 }, { 862, 284 })); g.fillPath(cap); texture(g, cap, speck, { 832, 144 }); bevel(g, cap, 2.4f); g.setGradientFill(goldGrad(144, 284)); g.drawRoundedRectangle(833.5f, 145.5f, 27.f, 137.f, 8.f, 2.2f);

        // ---- muzzle cone
        Path cone; cone.startNewSubPath(862, 160); cone.lineTo(970, 114); cone.quadraticTo(998, 214, 970, 314); cone.lineTo(862, 268); cone.closeSubPath();
        shadow(g, cone, 16, { 4, 10 }, 0.85f);
        g.setGradientFill(redGrad({ 862, 114 }, { 990, 314 })); g.fillPath(cone); texture(g, cone, speck, { 862, 114 }); wear(g, cone, 21, Colour(0xffe9b6a8), 50);
        { Graphics::ScopedSaveState s(g); g.reduceClipRegion(cone, {}); ColourGradient sh(Colour(0x33ffffff), 0, 120, Colour(0x66000000), 0, 310, false); sh.addColour(0.35, Colour(0x00000000)); g.setGradientFill(sh); g.fillRect(860, 100, 150, 230);
          for (int x : { 902, 934, 954 }) { Path st; st.addRectangle((float) x, 100.f, 8.f, 240.f); g.setGradientFill(ColourGradient(Colour(0xff6d4d12), (float) x, 0, Colour(0xff6d4d12), (float) x + 8, 0, false)); g.fillPath(st);
              ColourGradient cg(Colour(0xff6d4d12), (float) x, 0, Colour(0xff6d4d12), (float) x + 8, 0, false); cg.addColour(0.3, Colour(0xfffff0b0)); cg.addColour(0.6, Colour(0xffc99a36)); g.setGradientFill(cg); g.fillPath(st); } }
        innerShadow(g, cone, 8, 0.4f); bevel(g, cone, 2.6f, 0.5f, 0.6f); g.setGradientFill(goldGrad(114, 314)); g.strokePath(cone, PathStrokeType(2.8f));
        // plasma lens
        { for (int i = 6; i >= 1; --i) { g.setColour(Colour(0xff35e8d3).withAlpha(0.022f * (7 - i))); g.fillEllipse(972.f - i * 3.f, 120.f - i * 3.f, 40.f + i * 5.f, 190.f + i * 6.f); }
          Path lens; lens.addEllipse(960, 116, 38, 196); g.setGradientFill(ColourGradient(Colour(0xffe8fffb), 976, 140, Colour(0xff17a898), 998, 280, true)); g.fillPath(lens); g.setColour(Colour(0xaaffffff)); g.fillEllipse(967, 135, 11, 120); g.setColour(kGold); g.strokePath(lens, PathStrokeType(2.4f)); }
        { Path rec; rec.addRoundedRectangle(862.f, 192.f, 50.f, 56.f, 6.f); g.setColour(Colour(0xcc000000)); g.fillPath(rec); innerShadow(g, rec, 4, 0.8f); g.setColour(Colour(0x55ffffff)); g.drawRoundedRectangle(862.5f, 192.5f, 49.f, 55.f, 6.f, 1.f); }
        drawText(g, "OUTPUT", { 856, 250, 64, 14 }, F.title(8.5f), kGold);

        // ---- targeting scope housing
        Path scp; scp.startNewSubPath(512, 154); scp.lineTo(545, 22); scp.lineTo(722, 22); scp.lineTo(755, 154); scp.closeSubPath();
        shadow(g, scp, 18, { 4, 10 }, 0.85f); g.setGradientFill(redGrad({ 530, 22 }, { 740, 154 })); g.fillPath(scp); texture(g, scp, speck, { 512, 22 }); wear(g, scp, 31, Colour(0xffe9b6a8), 60);
        innerShadow(g, scp, 10, 0.45f); bevel(g, scp, 3.f); g.setGradientFill(goldGrad(22, 154)); g.strokePath(scp, PathStrokeType(3.2f, PathStrokeType::curved)); g.setColour(Colour(0x88ffffff)); g.drawLine(552, 29, 716, 29, 2.f);
        { Path rec; rec.addRoundedRectangle(547.f, 41.f, 174.f, 103.f, 10.f); g.setColour(Colour(0xff050304)); g.fillPath(rec); innerShadow(g, rec, 6, 0.9f); g.setGradientFill(goldGrad(41, 144)); g.strokePath(rec, PathStrokeType(2.f)); }

        // ---- master dial
        { Point<float> c(126, 228); float R = 121;
          Path bez; bez.addEllipse(c.x - R, c.y - R, 2 * R, 2 * R); shadow(g, bez, 24, { 6, 12 }, 0.9f);
          g.setGradientFill(ColourGradient(Colour(0xfff6f8fa), c.x - R * 0.7f, c.y - R * 0.8f, Colour(0xff2a2e32), c.x + R, c.y + R, true)); g.fillPath(bez);
          { Graphics::ScopedSaveState s(g); g.reduceClipRegion(bez, {}); for (int i = 0; i < 26; ++i) { float a = i * 0.2418f; g.setColour(Colour(0x14000000)); g.drawLine(c.x, c.y, c.x + std::cos(a) * R, c.y + std::sin(a) * R, 4.f); } }  // conic sheen
          bevel(g, bez, 3.f, 0.7f, 0.7f);
          Path ring; ring.addEllipse(c.x - R + 8, c.y - R + 8, 2 * R - 16, 2 * R - 16); g.setColour(Colour(0xaa000000)); g.strokePath(ring, PathStrokeType(3.f)); g.setColour(Colour(0x55ffffff)); g.strokePath(ring, PathStrokeType(1.f), AffineTransform::translation(0.f, 2.f));
          Path face; face.addEllipse(c.x - R + 11, c.y - R + 11, 2 * R - 22, 2 * R - 22);
          { ColourGradient cg(Colour(0xfff2505a), c.x - 55, c.y - 70, Colour(0xff4a080d), c.x + 100, c.y + 110, true); cg.addColour(0.45, Colour(0xffc41f2a)); g.setGradientFill(cg); g.fillPath(face); }
          texture(g, face, speck, { c.x - R, c.y - R }); wear(g, face, 41, Colour(0xffe9b6a8), 120); innerShadow(g, face, 10, 0.65f);
          { Graphics::ScopedSaveState s(g); g.reduceClipRegion(face, {}); g.setColour(Colour(0x22ffffff)); g.fillEllipse(c.x - 90, c.y - 100, 120, 60); }
          Path well; well.addEllipse(c.x - 56, c.y - 56, 112, 112); g.setColour(Colour(0x66000000)); g.fillPath(well); innerShadow(g, well, 6, 0.8f); }

        // ---- handle (chrome tube) + rubber ribbed grip + control plate
        { Path h; h.startNewSubPath(398, 316); h.cubicTo(380, 390, 386, 450, 420, 494);
          g.setColour(Colour(0x99000000)); g.strokePath(h, PathStrokeType(16.f, PathStrokeType::curved, PathStrokeType::rounded), AffineTransform::translation(4, 6));
          g.setColour(Colour(0xff1a1d20)); g.strokePath(h, PathStrokeType(14.f, PathStrokeType::curved, PathStrokeType::rounded));
          g.setColour(Colour(0xff7b8187)); g.strokePath(h, PathStrokeType(9.f, PathStrokeType::curved, PathStrokeType::rounded)); g.setColour(Colour(0xfff0f3f5)); g.strokePath(h, PathStrokeType(3.f), AffineTransform::translation(-1.5f, -1.f)); }
        Path grip; grip.addRoundedRectangle(245.f, 334.f, 132.f, 165.f, 16.f); shadow(g, grip, 16, { 4, 10 }, 0.9f);
        g.setGradientFill(ColourGradient(Colour(0xff3d4247), 245, 334, Colour(0xff0c0d0f), 377, 499, false)); g.fillPath(grip); brushed(g, { 245, 334, 132, 165 }, 8, 0.06f);
        { Graphics::ScopedSaveState s(g); g.reduceClipRegion(grip, {}); for (int y = 462; y < 500; y += 7) { g.setColour(Colour(0xff060708)); g.fillRoundedRectangle(255.f, (float) y, 112.f, 4.5f, 2.f); g.setColour(Colour(0x33ffffff)); g.drawHorizontalLine(y + 5, 258.f, 364.f); } }
        bevel(g, grip, 2.6f, 0.4f, 0.7f); g.setColour(kGoldDim); g.strokePath(grip, PathStrokeType(1.6f)); screw(g, 349, 490, 5); screw(g, 273, 490, 5);
    }
};
} // namespace rgui
