#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <BinaryData.h>

namespace rgui {
using namespace juce;
// ---- palette (original RAYGUN look: oxide red, brass gold, plasma teal)
inline const Colour kBg      { 0xff120a0b };
inline const Colour kBg2     { 0xff1c0d10 };
inline const Colour kRed     { 0xffd6232f };
inline const Colour kRedDeep { 0xff7d1219 };
inline const Colour kGold    { 0xffe6c46b };
inline const Colour kGoldDim { 0xff9a7f3c };
inline const Colour kTeal    { 0xff35e8d3 };
inline const Colour kCream   { 0xfff4e8cc };
inline const Colour kSteel   { 0xff9da3a8 };
inline const Colour kDarkSteel { 0xff3b4045 };

struct Fonts {
    Typeface::Ptr orbitron, rajdhaniSemi, rajdhaniBold;
    Fonts() {
        auto mk = [](const char* name) { int sz = 0; auto* d = BinaryData::getNamedResource(name, sz); return d ? Typeface::createSystemTypefaceFor(d, (size_t) sz) : Typeface::Ptr(); };
        orbitron = mk("Orbitron_ttf"); rajdhaniSemi = mk("RajdhaniSemiBold_ttf"); rajdhaniBold = mk("RajdhaniBold_ttf");
    }
    static Fonts& get() { static Fonts f; return f; }
    Font title(float h) const { return orbitron ? Font(FontOptions(orbitron)).withHeight(h) : Font(FontOptions((float) h, Font::bold)); }
    Font ui(float h) const { return rajdhaniSemi ? Font(FontOptions(rajdhaniSemi)).withHeight(h) : Font(FontOptions(h)); }
    Font bold(float h) const { return rajdhaniBold ? Font(FontOptions(rajdhaniBold)).withHeight(h) : Font(FontOptions(h, Font::bold)); }
};

inline void drawText(Graphics& g, const String& t, Rectangle<float> r, const Font& f, Colour c, Justification j = Justification::centred) {
    g.setColour(c); g.setFont(f); g.drawText(t, r, j, true);
}

inline void drawPanel(Graphics& g, Rectangle<float> r, const String& title, Colour accent, float radius = 10.f) {
    g.setGradientFill(ColourGradient(Colour(0xff2a0d12), r.getX(), r.getY(), Colour(0xff150809), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, radius);
    g.setColour(Colour(0x14ffffff)); g.drawRoundedRectangle(r.reduced(2.5f), radius - 2, 1.f);          // inner highlight
    g.setColour(Colour(0x66000000)); g.drawRoundedRectangle(r.reduced(4.f).translated(0, 1.5f), radius - 3, 1.f);
    g.setColour(accent.withAlpha(0.85f)); g.drawRoundedRectangle(r.reduced(0.75f), radius, 1.5f);
    if (title.isNotEmpty()) drawText(g, title.toUpperCase(), r.withHeight(22).withTrimmedLeft(12), Fonts::get().title(11.f), accent, Justification::centredLeft);
}

struct Look : public LookAndFeel_V4 {
    Look() {
        setColour(PopupMenu::backgroundColourId, Colour(0xff1b0b0e)); setColour(PopupMenu::textColourId, kCream);
        setColour(PopupMenu::highlightedBackgroundColourId, kRedDeep); setColour(PopupMenu::highlightedTextColourId, kGold);
        setColour(TextEditor::backgroundColourId, Colour(0xff0e0708)); setColour(TextEditor::textColourId, kCream);
        setColour(TextEditor::outlineColourId, kGold); setColour(TextEditor::focusedOutlineColourId, kTeal);
        setColour(TextEditor::highlightColourId, kRedDeep); setColour(CaretComponent::caretColourId, kTeal);
        setColour(ListBox::backgroundColourId, Colours::transparentBlack);
        setColour(AlertWindow::backgroundColourId, Colour(0xff1b0b0e)); setColour(AlertWindow::textColourId, kCream);
        setColour(TextButton::buttonColourId, Colour(0xff2a0d12)); setColour(TextButton::textColourOffId, kGold);
        setDefaultSansSerifTypefaceName("Segoe UI");
    }
    Font getPopupMenuFont() override { return Fonts::get().ui(17.f); }
    Font getTextButtonFont(TextButton&, int) override { return Fonts::get().bold(15.f); }
    Font getAlertWindowMessageFont() override { return Fonts::get().ui(16.f); }
    Font getAlertWindowTitleFont() override { return Fonts::get().title(14.f); }
    Typeface::Ptr getTypefaceForFont(const Font& f) override {
        auto& F = Fonts::get(); return F.rajdhaniSemi ? F.rajdhaniSemi : LookAndFeel_V4::getTypefaceForFont(f); }
};
} // namespace rgui
