#pragma once
#include "Look.h"
#include "../PluginProcessor.h"

namespace rgui {
// Every widget refuses keyboard focus: the plugin must never swallow the DAW's typing keyboard or MIDI keys.
constexpr int EDIT = -1, GLOBAL = -2;   // widget layer binding: follow the edit layer / global param (A=0, B=1 fixed)
inline String prefixFor(int lay, int editLayer) { return lay == GLOBAL ? String() : RaygunProcessor::layerPrefix(lay == EDIT ? editLayer : lay); }
struct NoFocus { static void apply(Component& c) { c.setWantsKeyboardFocus(false); c.setMouseClickGrabsKeyboardFocus(false); } };

// ---------------------------------------------------------------- rebindable parameter widgets
struct Bindable {
    virtual ~Bindable() = default;
    virtual void bind(int layer) = 0;
};

class Knob : public Slider, public Bindable {
public:
    Knob(RaygunProcessor& p, String baseId, String label, int lay, float diameterHint = 0)
        : proc(p), base(baseId), lab(label), lay(lay), dia(diameterHint) {
        setSliderStyle(RotaryVerticalDrag); setTextBoxStyle(NoTextBox, false, 0, 0);
        setRotaryParameters(MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        setMouseDragSensitivity(180); NoFocus::apply(*this); setPopupMenuEnabled(false);
        bind(p.editLayer);
    }
    void bind(int layer) override {
        att.reset();
        String id = prefixFor(lay, layer) + base;
        param = proc.apvts.getParameter(id);
        att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, id, *this);
        repaint();
    }
    void setCompact(bool c) { compact = c; }
    void paint(Graphics& g) override {
        auto b = getLocalBounds().toFloat(); auto& F = Fonts::get();
        float textH = compact ? 24.f : 30.f;
        float d = dia > 0 ? dia : std::min(b.getWidth(), b.getHeight() - textH) - 2.f;
        auto k = Rectangle<float>(d, d).withCentre({ b.getCentreX(), b.getY() + d * 0.5f + 1.f });
        float prop = (float) valueToProportionOfLength(getValue());
        float a0 = getRotaryParameters().startAngleRadians, a1 = getRotaryParameters().endAngleRadians, ang = a0 + prop * (a1 - a0);
        auto c = k.getCentre(); float r = d * 0.5f;
        // value arc
        Path arcBg, arcV; arcBg.addCentredArc(c.x, c.y, r, r, 0, a0, a1, true); arcV.addCentredArc(c.x, c.y, r, r, 0, a0, ang, true);
        g.setColour(Colour(0xff3a2a22)); g.strokePath(arcBg, PathStrokeType(2.6f));
        g.setColour(isEnabled() ? kGold : kGoldDim); g.strokePath(arcV, PathStrokeType(2.6f));
        // knurled rim + body
        auto body = k.reduced(r * 0.16f); float br = body.getWidth() * 0.5f;
        for (int i = 4; i >= 1; --i) { g.setColour(Colour(0x30000000)); g.fillEllipse(body.expanded(i * 1.2f).translated(i * 0.9f, i * 1.6f)); }   // contact shadow
        g.setGradientFill(ColourGradient(Colour(0xff9aa0a6), body.getX(), body.getY(), Colour(0xff15171a), body.getRight(), body.getBottom(), false));
        g.fillEllipse(body); g.setColour(Colour(0x55ffffff)); g.drawEllipse(body.reduced(0.6f), 1.f);
        g.setColour(Colour(0xff0d0e10)); for (int i = 0; i < 24; ++i) { float a = i * MathConstants<float>::twoPi / 24.f;
            g.drawLine(c.x + std::cos(a) * br * 0.86f, c.y + std::sin(a) * br * 0.86f, c.x + std::cos(a) * br, c.y + std::sin(a) * br, 1.f); }
        auto cap = body.reduced(br * 0.2f);
        { ColourGradient cg(Colour(0xfff6f8fa), cap.getX() + cap.getWidth() * 0.3f, cap.getY() + cap.getHeight() * 0.25f, Colour(0xff4b5157), cap.getRight(), cap.getBottom(), true); cg.addColour(0.5, Colour(0xffaab0b6)); g.setGradientFill(cg); g.fillEllipse(cap);
          g.setColour(Colour(0x66000000)); g.drawEllipse(cap, 1.2f); g.setColour(Colour(0x88ffffff)); g.fillEllipse(cap.getX() + cap.getWidth() * 0.18f, cap.getY() + cap.getHeight() * 0.1f, cap.getWidth() * 0.4f, cap.getHeight() * 0.2f); }
        g.setColour(kRed); g.drawLine(c.x + std::cos(ang - MathConstants<float>::halfPi) * br * 0.25f, c.y + std::sin(ang - MathConstants<float>::halfPi) * br * 0.25f,
                                      c.x + std::cos(ang - MathConstants<float>::halfPi) * br * 0.78f, c.y + std::sin(ang - MathConstants<float>::halfPi) * br * 0.78f, 2.4f);
        // text
        String val = param ? param->getCurrentValueAsText() : String(getValue());
        float y = k.getBottom() + 1.f;
        drawText(g, lab.toUpperCase(), { b.getX() + 1, y + 1, b.getWidth(), 14.f }, F.bold(compact ? 12.f : 13.f), Colour(0xcc000000));
        drawText(g, lab.toUpperCase(), { b.getX(), y, b.getWidth(), 14.f }, F.bold(compact ? 12.f : 13.f), kCream);
        drawText(g, val, { b.getX() + 1, y + 14.f, b.getWidth(), 14.f }, F.ui(compact ? 12.f : 13.f), Colour(0xcc000000));
        drawText(g, val, { b.getX(), y + 13.f, b.getWidth(), 14.f }, F.ui(compact ? 12.f : 13.f), kGold.brighter(0.2f));
    }
private:
    RaygunProcessor& proc; String base, lab; int lay; float dia; bool compact = false;
    RangedAudioParameter* param = nullptr; std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> att;
};

class HSlider : public Slider, public Bindable {
public:
    HSlider(RaygunProcessor& p, String baseId, String label, int lay, Colour c = kGold) : proc(p), base(baseId), lab(label), lay(lay), col(c) {
        setSliderStyle(LinearHorizontal); setTextBoxStyle(NoTextBox, false, 0, 0); NoFocus::apply(*this); bind(p.editLayer); }
    void bind(int layer) override { att.reset(); String id = prefixFor(lay, layer) + base; param = proc.apvts.getParameter(id);
        att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, id, *this); repaint(); }
    void paint(Graphics& g) override {
        auto b = getLocalBounds().toFloat(); auto& F = Fonts::get(); float th = 8.f; float ty = b.getBottom() - th - 3.f; auto tr = Rectangle<float>(b.getX() + 4, ty, b.getWidth() - 8, th);
        if (lab.isNotEmpty()) { drawText(g, lab.toUpperCase(), b.withHeight(14.f), F.bold(12.f), kCream.withAlpha(0.9f), Justification::centredLeft);
            drawText(g, param ? param->getCurrentValueAsText() : String(), b.withHeight(14.f), F.ui(12.f), kGoldDim.brighter(0.3f), Justification::centredRight); }
        g.setColour(Colour(0xff0e0708)); g.fillRoundedRectangle(tr, 4.f); g.setColour(kGoldDim.withAlpha(0.6f)); g.drawRoundedRectangle(tr, 4.f, 1.f);
        float prop = (float) valueToProportionOfLength(getValue()); float x = tr.getX() + prop * tr.getWidth();
        g.setColour(col.withAlpha(0.9f)); if (getMinimum() < 0) g.fillRect(jmin(x, tr.getCentreX()), ty + 1, std::abs(x - tr.getCentreX()), th - 2); else g.fillRoundedRectangle(tr.withWidth(x - tr.getX()), 4.f);
        g.setColour(kCream); g.fillRoundedRectangle(x - 4, ty - 4, 8, th + 8, 3.f);
    }
private: RaygunProcessor& proc; String base, lab; int lay; Colour col; RangedAudioParameter* param = nullptr; std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> att;
};

// Click -> popup list (wheel steps). Works for choice and int params.
class ChoiceBox : public Component, public Bindable {
public:
    ChoiceBox(RaygunProcessor& p, String baseId, String label, int lay) : proc(p), base(baseId), lab(label), lay(lay) { NoFocus::apply(*this); bind(p.editLayer); }
    void bind(int layer) override {
        att.reset(); String id = prefixFor(lay, layer) + base;
        param = proc.apvts.getParameter(id);
        att = std::make_unique<ParameterAttachment>(*param, [this](float) { repaint(); }, nullptr); att->sendInitialUpdate(); }
    StringArray choices() const { if (auto* c = dynamic_cast<AudioParameterChoice*>(param)) return c->choices; return {}; }
    void paint(Graphics& g) override {
        auto b = getLocalBounds().toFloat().reduced(1); auto& F = Fonts::get();
        g.setColour(Colour(0xff0e0708)); g.fillRoundedRectangle(b.withTrimmedTop(lab.isEmpty() ? 0.f : 13.f), 6.f);
        g.setColour(kGoldDim); g.drawRoundedRectangle(b.withTrimmedTop(lab.isEmpty() ? 0.f : 13.f), 6.f, 1.2f);
        if (lab.isNotEmpty()) drawText(g, lab.toUpperCase(), b.withHeight(13.f), F.bold(12.f), kCream.withAlpha(0.85f), Justification::centredLeft);
        auto tb = b.withTrimmedTop(lab.isEmpty() ? 0.f : 13.f);
        drawText(g, param ? param->getCurrentValueAsText() : String(), tb.reduced(6, 0), F.bold(15.f), kCream, Justification::centredLeft);
        Path tri; float cx = tb.getRight() - 12, cy = tb.getCentreY(); tri.addTriangle(cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3); g.setColour(kGold); g.fillPath(tri);
    }
    void mouseDown(const MouseEvent&) override {
        auto ch = choices(); if (ch.isEmpty() || !param) return;
        PopupMenu m; int cur = (int) std::round(param->convertFrom0to1(param->getValue()));
        for (int i = 0; i < ch.size(); ++i) m.addItem(i + 1, ch[i], true, i == cur);
        m.showMenuAsync(PopupMenu::Options().withTargetComponent(this).withMinimumWidth(getWidth()), [this](int r) { if (r > 0 && att) { att->setValueAsCompleteGesture((float) (r - 1)); } });
    }
    void mouseWheelMove(const MouseEvent&, const MouseWheelDetails& w) override {
        auto ch = choices(); if (ch.isEmpty() || !param || !att) return;
        int cur = (int) std::round(param->convertFrom0to1(param->getValue())); int nx = jlimit(0, ch.size() - 1, cur + (w.deltaY > 0 ? -1 : 1)); att->setValueAsCompleteGesture((float) nx);
    }
private:
    RaygunProcessor& proc; String base, lab; int lay; RangedAudioParameter* param = nullptr; std::unique_ptr<ParameterAttachment> att;
};

// Segmented selector for a choice param (mode / quality / filter type...)
class SegBar : public Component, public Bindable {
public:
    SegBar(RaygunProcessor& p, String baseId, int lay, StringArray labelsOverride = {}) : proc(p), base(baseId), lay(lay), labels(labelsOverride) { NoFocus::apply(*this); bind(p.editLayer); }
    void bind(int layer) override {
        att.reset(); String id = prefixFor(lay, layer) + base; param = proc.apvts.getParameter(id);
        att = std::make_unique<ParameterAttachment>(*param, [this](float) { repaint(); }, nullptr); att->sendInitialUpdate(); }
    StringArray names() const { if (labels.size()) return labels; if (auto* c = dynamic_cast<AudioParameterChoice*>(param)) return c->choices; return {}; }
    void paint(Graphics& g) override {
        auto b = getLocalBounds().toFloat().reduced(1); auto n = names(); if (n.isEmpty()) return;
        g.setColour(Colour(0xff150809)); g.fillRoundedRectangle(b, 7.f); g.setColour(kGoldDim); g.drawRoundedRectangle(b, 7.f, 1.4f);
        int cur = param ? (int) std::round(param->convertFrom0to1(param->getValue())) : 0; float w = b.getWidth() / n.size();
        for (int i = 0; i < n.size(); ++i) {
            auto r = Rectangle<float>(b.getX() + i * w, b.getY(), w, b.getHeight()).reduced(1.5f);
            if (i == cur) { g.setColour(kGold); g.fillRoundedRectangle(r, 5.f); }
            drawText(g, n[i].toUpperCase(), r, Fonts::get().bold(14.f), i == cur ? Colour(0xff1a0c06) : kGold);
        }
    }
    void mouseDown(const MouseEvent& e) override { auto n = names(); if (n.isEmpty() || !att) return; int i = jlimit(0, n.size() - 1, (int) (e.position.x / (getWidth() / (float) n.size()))); att->setValueAsCompleteGesture((float) i); }
private:
    RaygunProcessor& proc; String base; int lay; StringArray labels; RangedAudioParameter* param = nullptr; std::unique_ptr<ParameterAttachment> att;
};

class Switch : public Component, public Bindable {
public:
    Switch(RaygunProcessor& p, String baseId, String label, int lay) : proc(p), base(baseId), lab(label), lay(lay) { NoFocus::apply(*this); bind(p.editLayer); }
    void bind(int layer) override {
        att.reset(); String id = prefixFor(lay, layer) + base; param = proc.apvts.getParameter(id);
        att = std::make_unique<ParameterAttachment>(*param, [this](float v) { on = v > 0.5f; repaint(); }, nullptr); att->sendInitialUpdate(); }
    void paint(Graphics& g) override {
        auto b = getLocalBounds().toFloat().reduced(1);
        g.setColour(on ? kGold : Colour(0xff150809)); g.fillRoundedRectangle(b, 7.f); g.setColour(kGoldDim); g.drawRoundedRectangle(b, 7.f, 1.4f);
        drawText(g, lab.toUpperCase(), b, Fonts::get().bold(14.f), on ? Colour(0xff1a0c06) : kGold);
    }
    void mouseDown(const MouseEvent&) override { if (att) att->setValueAsCompleteGesture(on ? 0.f : 1.f); }
    bool on = false;
private:
    RaygunProcessor& proc; String base, lab; int lay; RangedAudioParameter* param = nullptr; std::unique_ptr<ParameterAttachment> att;
};

// Plain clickable pill (no parameter)
class Pill : public Component {
public:
    Pill(String t, Colour accent = kGold) : text(t), col(accent) { NoFocus::apply(*this); }
    std::function<void()> onClick, onDown, onUp; bool toggled = false; bool toggleStyle = false; String text; Colour col;
    void paint(Graphics& g) override {
        auto b = getLocalBounds().toFloat().reduced(1); bool on = toggled || down;
        float rad = jmin(10.f, b.getHeight() * 0.5f);
        g.setColour(Colour(0x66000000)); g.fillRoundedRectangle(b.translated(0, 2), rad);
        g.setGradientFill(on ? ColourGradient(col.brighter(0.45f), 0, b.getY(), col.darker(0.35f), 0, b.getBottom(), false) : ColourGradient(Colour(0xff3a1a1f), 0, b.getY(), Colour(0xff150709), 0, b.getBottom(), false));
        g.fillRoundedRectangle(b, rad); g.setColour(Colour(on ? 0x66ffffff : 0x28ffffff)); g.drawRoundedRectangle(b.reduced(1.5f).withHeight(b.getHeight() * 0.5f), rad, 1.f);
        g.setColour(col.withAlpha(0.9f)); g.drawRoundedRectangle(b, rad, 1.5f);
        drawText(g, text.toUpperCase(), b, Fonts::get().bold(jmin(16.f, b.getHeight() * 0.62f)), on ? Colour(0xff1a0c06) : col);
    }
    void mouseDown(const MouseEvent&) override { down = true; repaint(); if (onDown) onDown(); }
    void mouseUp(const MouseEvent& e) override { down = false; repaint(); if (onUp) onUp(); if (getLocalBounds().contains(e.getPosition()) && onClick) onClick(); }
    bool down = false;
};
} // namespace rgui
