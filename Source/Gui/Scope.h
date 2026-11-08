#pragma once
#include "Widgets.h"
#include "../engine/Wavetables.h"

namespace rgui {
// Circular wavetable viewscreen
class WaveScope : public Component, private Timer {
public:
    WaveScope(RaygunProcessor& p) : proc(p) { NoFocus::apply(*this); startTimerHz(30); timerCallback(); }
    int osc = 0;
    void setOsc(int o) { osc = o; lastTable = -1; repaint(); }
    void timerCallback() override {
        int l = proc.editLayer; String pf = RaygunProcessor::layerPrefix(l) + "o" + String(osc + 1) + "_";
        int t = (int) proc.apvts.getRawParameterValue(pf + "table")->load(); float pos = proc.apvts.getRawParameterValue(pf + "pos")->load();
        if (t != lastTable || std::abs(pos - lastPos) > 1e-4f || l != lastLayer) { if (t != lastTable) rebuild(t); lastTable = t; lastPos = pos; lastLayer = l; repaint(); }
    }
    void rebuild(int t) {
        auto& W = rg::Wavetables::get(); W.build();
        for (int f = 0; f < rg::WT_FRAMES; ++f) { Path p; const float* d = W.frame(t, 1, f);
            for (int i = 0; i <= 96; ++i) { float v = d[(i * (rg::WT_N - 1)) / 96]; if (i == 0) p.startNewSubPath(0.f, -v); else p.lineTo((float) i / 96.f, -v); }
            paths[f] = p; }
    }
    void paint(Graphics& g) override {
        auto b = getLocalBounds().toFloat(); auto c = b.getCentre(); float r = jmin(b.getWidth(), b.getHeight()) * 0.5f - 2;
        g.setGradientFill(ColourGradient(Colour(0xff0b3a3a), c.x - r * 0.4f, c.y - r * 0.5f, Colour(0xff031112), c.x + r, c.y + r, true)); g.fillEllipse(c.x - r, c.y - r, 2 * r, 2 * r);
        { Graphics::ScopedSaveState ss(g); Path clip; clip.addEllipse(c.x - r, c.y - r, 2 * r, 2 * r); g.reduceClipRegion(clip);
          g.setColour(Colour(0x2235e8d3)); for (int i = 1; i < 8; ++i) { float x = b.getX() + b.getWidth() * i / 8.f; g.drawVerticalLine((int) x, b.getY(), b.getBottom()); g.drawHorizontalLine((int) (b.getY() + b.getHeight() * i / 8.f), b.getX(), b.getRight()); }
          if (lastTable >= 0) {
              float cur = lastPos * (rg::WT_FRAMES - 1); float w = r * 1.5f, h = r * 0.28f, x0 = c.x - w * 0.5f, y0 = c.y - r * 0.55f, dy = r * 1.05f / rg::WT_FRAMES;
              for (int f = 0; f < rg::WT_FRAMES; ++f) { bool hi = std::abs(f - cur) < 0.6f;
                  auto tr = AffineTransform::scale(w, h).translated(x0 + (rg::WT_FRAMES - f) * 0.5f, y0 + f * dy + h * 0.5f);
                  g.setColour(hi ? Colour(0xffff7a3d) : kTeal.withAlpha(0.18f + 0.32f * (f / 31.f))); g.strokePath(paths[f], PathStrokeType(hi ? 2.2f : 1.1f), tr); } } }
        { Graphics::ScopedSaveState ss(g); Path clip; clip.addEllipse(c.x - r, c.y - r, 2 * r, 2 * r); g.reduceClipRegion(clip);
          g.setGradientFill(ColourGradient(Colour(0x00000000), c.x, c.y, Colour(0xaa000000), c.x + r, c.y + r, true)); g.fillEllipse(c.x - r, c.y - r, 2 * r, 2 * r);   // vignette
          g.setGradientFill(ColourGradient(Colour(0x44ffffff), c.x - r * 0.5f, c.y - r * 0.9f, Colour(0x00ffffff), c.x, c.y - r * 0.2f, false)); Path gl; gl.addEllipse(c.x - r * 0.85f, c.y - r * 1.05f, r * 1.7f, r * 0.95f); g.fillPath(gl); }
        g.setColour(Colour(0xff0a0506)); g.drawEllipse(c.x - r - 2, c.y - r - 2, 2 * r + 4, 2 * r + 4, 4.f);
        { ColourGradient cg(Colour(0xfffff0b0), c.x, c.y - r, Colour(0xff6d4d12), c.x, c.y + r, false); cg.addColour(0.5, Colour(0xffc99a36)); g.setGradientFill(cg); }
        g.drawEllipse(c.x - r, c.y - r, 2 * r, 2 * r, 3.5f);
        g.setColour(Colour(0x55000000)); g.drawEllipse(c.x - r + 4, c.y - r + 4, 2 * r - 8, 2 * r - 8, 1.f);
        if (lastTable >= 0 && r > 60) drawText(g, String(rg::TABLE_NAMES[lastTable]).toUpperCase() + String::fromUTF8(" \xc2\xb7 POS ") + String((int) std::round(lastPos * 100)), { c.x - r * 0.8f, c.y - r * 0.8f, r * 1.6f, 16.f }, Fonts::get().title(r > 80 ? 9.5f : 8.f), kTeal);
    }
    // click the top of the screen to pick a table; drag vertically anywhere else to scan the wavetable position
    void mouseDown(const MouseEvent& e) override {
        String pf = RaygunProcessor::layerPrefix(proc.editLayer) + "o" + String(osc + 1) + "_";
        if (e.position.y < getHeight() * 0.3f) {
            PopupMenu m; for (int i = 0; i < rg::NTABLES; ++i) m.addItem(i + 1, rg::TABLE_NAMES[i], true, i == lastTable);
            m.showMenuAsync(PopupMenu::Options().withTargetComponent(this), [this, pf](int r) { if (r > 0) { auto* p = proc.apvts.getParameter(pf + "table"); p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1((float) (r - 1))); p->endChangeGesture(); } });
            return; }
        dragParam = proc.apvts.getParameter(pf + "pos"); dragStart = dragParam->getValue(); dragParam->beginChangeGesture(); }
    void mouseDrag(const MouseEvent& e) override { if (dragParam) dragParam->setValueNotifyingHost(jlimit(0.f, 1.f, dragStart - e.getDistanceFromDragStartY() / 150.f)); }
    void mouseUp(const MouseEvent&) override { if (dragParam) { dragParam->endChangeGesture(); dragParam = nullptr; } }
    void mouseDoubleClick(const MouseEvent&) override { auto* p = proc.apvts.getParameter(RaygunProcessor::layerPrefix(proc.editLayer) + "o" + String(osc + 1) + "_pos"); p->beginChangeGesture(); p->setValueNotifyingHost(p->getDefaultValue()); p->endChangeGesture(); }
    int lastTable = -1, lastLayer = -1; float lastPos = -1;
private:
    RaygunProcessor& proc; Path paths[rg::WT_FRAMES]; RangedAudioParameter* dragParam = nullptr; float dragStart = 0;
};

// XY "targeting scope": X = Bright macro, Y = Motion macro
class XYPad : public Component, private Timer {
public:
    XYPad(RaygunProcessor& p) : proc(p) { NoFocus::apply(*this); startTimerHz(30); }
    void timerCallback() override { float x = proc.apvts.getRawParameterValue("macro1")->load(), y = proc.apvts.getRawParameterValue("macro6")->load(); if (x != px || y != py) { px = x; py = y; repaint(); } }
    void paint(Graphics& g) override {
        auto b = getLocalBounds().toFloat();
        g.setGradientFill(ColourGradient(Colour(0xff0b2a2a), b.getX(), b.getY(), Colour(0xff031010), b.getRight(), b.getBottom(), false)); g.fillRoundedRectangle(b, 6.f);
        g.setColour(Colour(0x3335e8d3)); for (int i = 1; i < 10; ++i) { g.drawVerticalLine((int) (b.getX() + b.getWidth() * i / 10.f), b.getY(), b.getBottom()); g.drawHorizontalLine((int) (b.getY() + b.getHeight() * i / 10.f), b.getX(), b.getRight()); }
        float x = b.getX() + px * b.getWidth(), y = b.getBottom() - py * b.getHeight();
        g.setColour(Colour(0xffff5a3c)); g.drawHorizontalLine((int) y, b.getX(), b.getRight()); g.drawVerticalLine((int) x, b.getY(), b.getBottom()); g.drawEllipse(x - 7, y - 7, 14, 14, 1.8f);
        drawText(g, "TARGETING SCOPE", b.withHeight(16).reduced(6, 0), Fonts::get().title(9.f), kTeal, Justification::centredLeft);
        drawText(g, String::fromUTF8("X BRIGHT \xc2\xb7 Y MOTION"), b.removeFromBottom(16).reduced(6, 0), Fonts::get().ui(11.f), kTeal.withAlpha(0.8f), Justification::centredRight);
        g.setGradientFill(ColourGradient(Colour(0x30ffffff), b.getX(), b.getY(), Colour(0x00ffffff), b.getCentreX(), b.getCentreY(), false)); Path gl; gl.addTriangle(b.getX(), b.getY(), b.getRight() * 0.7f, b.getY(), b.getX(), b.getBottom() * 0.8f); g.fillPath(gl);
        g.setColour(Colour(0xff0a0506)); g.drawRoundedRectangle(b.reduced(0.5f), 6.f, 2.f);
    }
    void mouseDown(const MouseEvent& e) override { for (auto* id : { "macro1", "macro6" }) proc.apvts.getParameter(id)->beginChangeGesture(); mouseDrag(e); }
    void mouseDrag(const MouseEvent& e) override {
        float x = jlimit(0.f, 1.f, e.position.x / getWidth()), y = jlimit(0.f, 1.f, 1.f - e.position.y / getHeight());
        proc.apvts.getParameter("macro1")->setValueNotifyingHost(x); proc.apvts.getParameter("macro6")->setValueNotifyingHost(y); }
    void mouseUp(const MouseEvent&) override { for (auto* id : { "macro1", "macro6" }) proc.apvts.getParameter(id)->endChangeGesture(); }
private:
    RaygunProcessor& proc; float px = -1, py = -1;
};

// Keyboard with Layer A / Layer B zones. Never takes focus; clicks play notes via MidiKeyboardState.
class KeyboardView : public Component, private Timer {
public:
    KeyboardView(RaygunProcessor& p, int lo = 36, int hi = 96) : proc(p), lowN(lo), highN(hi) { NoFocus::apply(*this); startTimerHz(30); }
    void timerCallback() override { repaint(); }
    static bool isBlack(int n) { int m = n % 12; return m == 1 || m == 3 || m == 6 || m == 8 || m == 10; }
    int whiteCount() const { int c = 0; for (int n = lowN; n <= highN; ++n) c += !isBlack(n); return c; }
    Rectangle<float> keyRect(int note) const {
        float ww = getWidth() / (float) whiteCount(); int wi = 0; for (int n = lowN; n < note; ++n) wi += !isBlack(n);
        if (!isBlack(note)) return { wi * ww, 0.f, ww, (float) getHeight() };
        return { wi * ww - ww * 0.3f, 0.f, ww * 0.6f, getHeight() * 0.62f }; }
    int noteAt(Point<float> p) const {
        for (int n = lowN; n <= highN; ++n) if (isBlack(n) && keyRect(n).contains(p)) return n;
        for (int n = lowN; n <= highN; ++n) if (!isBlack(n) && keyRect(n).contains(p)) return n; return -1; }
    void paint(Graphics& g) override {
        int mode = (int) proc.apvts.getRawParameterValue("mode")->load(), split = (int) proc.apvts.getRawParameterValue("split")->load(), xf = (int) proc.apvts.getRawParameterValue("xfade")->load();
        auto zone = [&](int n) { // 0 = A, 1 = B, 2 = both
            if (mode == 0) return 0; if (mode == 1) return 1; if (mode == 3) return n >= split ? 1 : (n >= split - xf ? 2 : 0); return 2; };
        for (int pass = 0; pass < 2; ++pass) for (int n = lowN; n <= highN; ++n) {
            if (isBlack(n) != (pass == 1)) continue; auto r = keyRect(n).reduced(0.5f, 0); int z = zone(n);
            bool down = heldUi == n || proc.noteHeld[n].load();
            Colour base = pass == 0 ? Colour(0xfff6e4e6) : Colour(0xff2a0b10);
            if (z == 1) base = pass == 0 ? Colour(0xffdff7f4) : Colour(0xff0e2b2c);
            if (z == 2) base = pass == 0 ? Colour(0xffefe9e4) : Colour(0xff201a1a);
            if (down) base = (z == 1) ? kTeal : (z == 0 ? Colour(0xffea4553) : kGold);
            g.setColour(base); g.fillRoundedRectangle(r, 2.f); if (pass == 0) { g.setColour(Colour(0x55000000)); g.drawRoundedRectangle(r, 2.f, 0.8f); }
        }
        if (mode == 3) { auto r = keyRect(split); float x = r.getX(); g.setColour(kGold); g.fillRect(x - 1.5f, 0.f, 3.f, (float) getHeight()); }
    }
    void mouseDown(const MouseEvent& e) override { press(noteAt(e.position)); }
    void mouseDrag(const MouseEvent& e) override { int n = noteAt(e.position); if (n != heldUi) { release(); press(n); } }
    void mouseUp(const MouseEvent&) override { release(); }
private:
    void press(int n) { if (n >= 0) { heldUi = n; proc.keyboardState.noteOn(1, n, 0.8f); } }
    void release() { if (heldUi >= 0) { proc.keyboardState.noteOff(1, heldUi, 0.f); heldUi = -1; } }
    RaygunProcessor& proc; int lowN, highN, heldUi = -1;
};

class Meter : public Component, private Timer {
public:
    Meter(RaygunProcessor& p) : proc(p) { NoFocus::apply(*this); startTimerHz(30); }
    void timerCallback() override { repaint(); }
    void paint(Graphics& g) override {
        float l = proc.meterL.load(), r = proc.meterR.load(); int bars = 6; float w = getWidth() / (float) bars;
        for (int i = 0; i < bars; ++i) { float lv = (i % 2 ? l : r) * (0.55f + 0.09f * i); float h = jmax(3.f, jlimit(0.f, 1.f, lv * 1.6f) * getHeight() * (0.45f + 0.1f * (i % 3)));
            g.setColour(i < 4 ? kTeal : kGold); g.fillRect(i * w + 1, getHeight() - h, w - 3, h); }
    }
private: RaygunProcessor& proc;
};
} // namespace rgui
