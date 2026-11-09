#include "PluginEditor.h"
#include "Gui/GunArt.h"
using namespace juce;
#if defined(RAYGUN_VST2_BUILD) && JUCE_WINDOWS
extern "C" __declspec(dllexport) void rg_logWindowInfo(void* hwnd, const char* tag);
#endif
using namespace rgui;

// =====================================================================================================================
//  small helpers
// =====================================================================================================================
namespace {
Knob* K(Page& pg, RaygunProcessor& p, const String& id, const String& label, int lay, float x, float y, float w = 64, float h = 64) {
    auto* k = pg.add<Knob>(p, id, label, lay); k->setBounds((int) x, (int) y, (int) w, (int) h); k->setCompact(true); return k; }
ChoiceBox* C(Page& pg, RaygunProcessor& p, const String& id, const String& label, int lay, float x, float y, float w, float h = 40) {
    auto* c = pg.add<ChoiceBox>(p, id, label, lay); c->setBounds((int) x, (int) y, (int) w, (int) h); return c; }

struct FireButton : Pill {
    FireButton() : Pill("FIRE", kRed) {}
    void paint(Graphics& g) override {
        auto b = getLocalBounds().toFloat().reduced(2); float d = jmin(b.getWidth(), b.getHeight()); auto r = Rectangle<float>(d, d).withCentre(b.getCentre());
        g.setColour(Colour(0xff0a0506)); g.fillEllipse(r.expanded(2));
        g.setGradientFill(ColourGradient(down ? Colour(0xffff6a6a) : Colour(0xffff4a52), r.getX() + d * 0.3f, r.getY() + d * 0.2f, Colour(0xff7d1219), r.getRight(), r.getBottom(), true)); g.fillEllipse(r);
        g.setColour(Colour(0x55ffffff)); g.fillEllipse(r.reduced(d * 0.18f).withHeight(d * 0.3f).translated(0, d * 0.05f));
        g.setColour(kGold); g.drawEllipse(r, 2.5f); drawText(g, "FIRE", r, Fonts::get().title(13.f), kCream);
    }
};
struct IconButton : Pill {
    int icon;
    IconButton(int i) : Pill(""), icon(i) {}
    void paint(Graphics& g) override {
        auto b = getLocalBounds().toFloat().reduced(1); g.setColour(down ? kGold : Colour(0xff1d0b0f)); g.fillRoundedRectangle(b, 8.f); g.setColour(kGold); g.drawRoundedRectangle(b, 8.f, 1.5f);
        g.setColour(down ? Colour(0xff1a0c06) : kGold); auto c = b.getCentre();
        if (icon == 0) for (int i = -1; i <= 1; ++i) g.fillRoundedRectangle(c.x - 9, c.y + i * 6 - 1.5f, 18, 3, 1.5f);
        else { Path p; p.addStar(c, 8, 5.5f, 9.f); g.fillPath(p); g.setColour(down ? kGold : Colour(0xff1d0b0f)); g.fillEllipse(c.x - 3.5f, c.y - 3.5f, 7, 7); }
    }
};

String presetSub(const rg::PresetInfo* p) { return p ? p->type.toUpperCase() + String::fromUTF8(" \xc2\xb7 ") + p->character + (p->key.isNotEmpty() ? String::fromUTF8(" \xc2\xb7 ") + p->key : String()) : String("INIT"); }

// ---------------------------------------------------------------------------------------------------------------------
//  MAIN page: the ray gun
// ---------------------------------------------------------------------------------------------------------------------
struct MainPage : Page {
    RaygunProcessor& proc; WaveScope* scope = nullptr; Pill *osc1 = nullptr, *osc2 = nullptr, *fire = nullptr, *panic = nullptr; std::function<const rg::PresetInfo*()> presetGetter;
    Image speckle;

    explicit MainPage(RaygunProcessor& p) : proc(p) {
        scope = add<WaveScope>(p); scope->setBounds(228, 108, 196, 196);
        auto addOsc = [&](Pill*& pl, const char* t, int x, int o) { pl = add<Pill>(t, kTeal); pl->setBounds(x, 282, 72, 22); pl->onClick = [this, o] { scope->setOsc(o); osc1->toggled = o == 0; osc2->toggled = o == 1; osc1->repaint(); osc2->repaint(); }; };
        addOsc(osc1, "OSC 1", 262, 0); addOsc(osc2, "OSC 2", 338, 1); osc1->toggled = true;
        auto* master = add<Knob>(p, "master", "Master", GLOBAL, 84.f); master->setBounds(76, 184, 100, 118);
        // 8 macro knobs along the barrel
        for (int i = 0; i < 8; ++i) { auto* k = add<Knob>(p, "macro" + String(i + 1), rg::MACRO_NAMES[i], GLOBAL, 40.f); k->setBounds(470 + (int) (i * 45.2f), 166, 46, 70); k->setCompact(true); }
        auto* xy = add<XYPad>(p); xy->setBounds(553, 47, 162, 91);
        auto* meter = add<Meter>(p); meter->setBounds(868, 196, 44, 44);
        auto* blend = add<HSlider>(p, "blend", "A / B Blend", GLOBAL); blend->setBounds(257, 346, 108, 32);
        K(*this, p, "maxv", "Voices", EDIT, 252, 394, 40, 62); K(*this, p, "glide", "Glide", EDIT, 296, 394, 40, 62); K(*this, p, "bend", "Bend", EDIT, 340, 394, 40, 62);
        fire = add<FireButton>(); fire->setBounds(412, 337, 62, 62);
        fire->onDown = [this] { proc.keyboardState.noteOn(1, 48, 0.85f); }; fire->onUp = [this] { proc.keyboardState.noteOff(1, 48, 0.f); };
        panic = add<Pill>("PANIC", kSteel); panic->setBounds(410, 408, 66, 24);
        panic->onClick = [this] { proc.keyboardState.allNotesOff(0); for (int c = 1; c <= 16; ++c) proc.keyboardState.allNotesOff(c); proc.panicFlag = true; };
        auto* kb = add<KeyboardView>(p, 36, 96); kb->setBounds(506, 344, 466, 56);
        // procedural oxidised-paint speckle (cached once)
        speckle = Image(Image::ARGB, 400, 300, true); Random r(351); Graphics g(speckle);
        for (int i = 0; i < 900; ++i) { g.setColour(Colours::white.withAlpha(r.nextFloat() * 0.16f)); g.fillEllipse(r.nextFloat() * 400, r.nextFloat() * 300, 1.f + r.nextFloat() * 4.f, 1.f + r.nextFloat() * 3.f); }
        for (int i = 0; i < 500; ++i) { g.setColour(Colours::black.withAlpha(r.nextFloat() * 0.18f)); g.fillEllipse(r.nextFloat() * 400, r.nextFloat() * 300, 1.f + r.nextFloat() * 3.f, 1.f + r.nextFloat() * 3.f); }
    }

    void paintSpeckle(Graphics& g, Path clip, Rectangle<float> at) { Graphics::ScopedSaveState s(g); g.reduceClipRegion(clip); g.setOpacity(0.9f); g.drawImage(speckle, at); }

    Image body;   // cached 2x render of the static artwork
    void paint(Graphics& g) override {
        auto& F = Fonts::get();
        if (!body.isValid()) { body = Image(Image::ARGB, W * 2, PAGE_H * 2, true); Graphics gi(body); gi.addTransform(AffineTransform::scale(2.f)); Art::drawBody(gi, speckle); }
        g.setImageResamplingQuality(Graphics::highResamplingQuality); g.drawImage(body, 0, 0, W, PAGE_H, 0, 0, W * 2, PAGE_H * 2);
        // ---- dial LEDs (live)
        { Point<float> c(126, 228); float prop = jlimit(0.f, 1.f, (proc.apvts.getRawParameterValue("master")->load() + 40.f) / 46.f);
          for (int i = 0; i < 28; ++i) { float a = MathConstants<float>::pi * 0.75f + i / 27.f * MathConstants<float>::pi * 1.5f - MathConstants<float>::halfPi;
              bool lit = i / 27.f <= prop; Colour col = i < 16 ? kTeal : (i < 23 ? Colour(0xffffb13b) : Colour(0xffff4d3a)); float x = c.x + std::cos(a) * 96, y = c.y + std::sin(a) * 96;
              g.setColour(Colour(0xaa000000)); g.fillEllipse(x - 5.5f, y - 4.5f, 11, 11);
              if (lit) { g.setColour(col.withAlpha(0.35f)); g.fillEllipse(x - 8, y - 8, 16, 16); g.setGradientFill(ColourGradient(col.brighter(0.7f), x - 2, y - 2, col.darker(0.2f), x + 4, y + 4, true)); }
              else g.setColour(Colour(0xff3a1014)); g.fillEllipse(x - 4.5f, y - 4.5f, 9, 9); } }
        // ---- keyboard & description panels
        drawPanel(g, { 500, 317, 478, 92 }, "", kGold, 8.f);
        int mode = (int) proc.apvts.getRawParameterValue("mode")->load(), split = (int) proc.apvts.getRawParameterValue("split")->load();
        static const char* nn[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        String cap2 = mode == 3 ? String::fromUTF8("LAYER A  \xc2\xb7  SPLIT ") + String(nn[split % 12]) + String(split / 12 - 2) + String::fromUTF8("  \xc2\xb7  LAYER B") : mode == 4 ? String::fromUTF8("VELOCITY SPLIT  \xc2\xb7  A SOFT / B HARD") : mode == 2 ? "LAYER A + B" : mode == 0 ? "LAYER A" : "LAYER B";
        drawText(g, cap2, { 512, 320, 300, 20 }, F.title(10.f), kGold, Justification::centredLeft);
        drawText(g, String::fromUTF8("MIDI IN \xe2\x97\x8f host only"), { 780, 320, 190, 20 }, F.ui(13.f), kCream.withAlpha(0.7f), Justification::centredRight);
        drawPanel(g, { 500, 420, 478, 66 }, "", kGold, 8.f);
        const rg::PresetInfo* pi = presetGetter ? presetGetter() : nullptr;
        g.setColour(kCream); g.setFont(F.ui(15.5f)); g.drawFittedText(pi ? pi->desc : String("Init patch. Open the preset browser to load a sound."), 514, 424, 450, 34, Justification::topLeft, 2);
        float tx = 514; StringArray tags; if (pi) { tags.addArray(pi->tags); if (pi->key.isNotEmpty()) tags.add("KEY " + pi->key); }
        for (auto& t : tags) { if (tx > 900) break; float w = F.bold(13.f).getStringWidthFloat(t.toUpperCase()) + 18; bool key = t.startsWithIgnoreCase("KEY");
            g.setColour(Colour(0xff1d0b0f)); g.fillRoundedRectangle(tx, 460, w, 20, 10); g.setColour(key ? kTeal : kGold); g.drawRoundedRectangle(tx, 460, w, 20, 10, 1.3f); drawText(g, t.toUpperCase(), { tx, 460, w, 20 }, F.bold(13.f), key ? kTeal : kGold); tx += w + 8; }
    }
};

// ---------------------------------------------------------------------------------------------------------------------
//  OSC page
// ---------------------------------------------------------------------------------------------------------------------
Page* makeOscPage(RaygunProcessor& p) {
    auto* pg = new Page();
    for (int o = 1; o <= 2; ++o) {
        float x = o == 1 ? 14.f : 506.f; pg->panels.push_back({ { x, 10, 480, 250 }, String("OSC ") + String(o), o == 1 ? kRed : kGold });
        auto* sc = pg->add<WaveScope>(p); sc->setBounds((int) x + 14, 40, 150, 150); sc->setOsc(o - 1);
        String id = "o" + String(o) + "_";
        C(*pg, p, id + "table", "Table", EDIT, x + 14, 200, 150);
        K(*pg, p, id + "pos", "Position", EDIT, x + 180, 40); K(*pg, p, id + "lvl", "Level", EDIT, x + 252, 40); K(*pg, p, id + "warp", "Warp", EDIT, x + 324, 40);
        C(*pg, p, id + "warpmode", "Warp mode", EDIT, x + 392, 44, 78);
        K(*pg, p, id + "oct", "Octave", EDIT, x + 180, 126); K(*pg, p, id + "semi", "Semi", EDIT, x + 252, 126); K(*pg, p, id + "fine", "Fine", EDIT, x + 324, 126);
        if (o == 2) K(*pg, p, "fm", "FM > Osc1", EDIT, x + 396, 126);
    }
    pg->panels.push_back({ { 14, 270, 972, 230 }, "SUB / NOISE / UNISON", kGold });
    float y = 306; K(*pg, p, "sub_lvl", "Sub", EDIT, 40, y, 72, 70); C(*pg, p, "sub_wave", "Sub wave", EDIT, 130, y + 4, 110);
    K(*pg, p, "noise_lvl", "Noise", EDIT, 270, y, 72, 70);
    K(*pg, p, "uni_n", "Unison", EDIT, 420, y, 72, 70); K(*pg, p, "uni_det", "Detune", EDIT, 520, y, 72, 70); K(*pg, p, "uni_wid", "Width", EDIT, 620, y, 72, 70);
    pg->panels.push_back({ { 40, 410, 920, 70 }, "", kGoldDim });
    return pg;
}
Page* makeFilterPage(RaygunProcessor& p) {
    auto* pg = new Page();
    pg->panels = { { { 14, 10, 486, 250 }, "FILTER", kRed }, { { 506, 10, 480, 120 }, "AMP ENVELOPE", kGold }, { { 506, 140, 480, 120 }, "MOD ENVELOPE (ENV 2)", kGold }, { { 14, 270, 972, 150 }, "VOICE", kGold } };
    auto* seg = pg->add<SegBar>(p, "f_type", EDIT); seg->setBounds(28, 40, 458, 36);
    K(*pg, p, "f_cut", "Cutoff", EDIT, 36, 100, 82, 80); K(*pg, p, "f_res", "Resonance", EDIT, 130, 100, 82, 80); K(*pg, p, "f_drive", "Drive", EDIT, 224, 100, 82, 80); K(*pg, p, "f_key", "Keytrack", EDIT, 318, 100, 82, 80); K(*pg, p, "f_env", "Env amt", EDIT, 412, 100, 82, 80);
    const char* en[] = { "Atk", "Dec", "Sus", "Rel" };
    for (int i = 0; i < 4; ++i) { K(*pg, p, String("a_") + String(en[i]).toLowerCase().substring(0, 3), en[i], EDIT, 540.f + i * 112, 42, 80, 78); K(*pg, p, String("e_") + String(en[i]).toLowerCase().substring(0, 3), en[i], EDIT, 540.f + i * 112, 172, 80, 78); }
    auto* v = pg->add<SegBar>(p, "voice", EDIT); v->setBounds(28, 308, 230, 36);
    K(*pg, p, "glide", "Glide", EDIT, 290, 296, 80, 80); K(*pg, p, "maxv", "Max voices", EDIT, 384, 296, 80, 80); K(*pg, p, "bend", "Bend range", EDIT, 478, 296, 80, 80);
    K(*pg, p, "oct", "Layer oct", EDIT, 572, 296, 80, 80); K(*pg, p, "vol", "Layer vol", EDIT, 666, 296, 80, 80); K(*pg, p, "pan", "Layer pan", EDIT, 760, 296, 80, 80);
    return pg;
}
Page* makeModPage(RaygunProcessor& p) {
    auto* pg = new Page();
    pg->panels = { { { 14, 10, 640, 490 }, "MOD MATRIX  (8 SLOTS)", kRed }, { { 666, 10, 320, 120 }, "LFO 1", kGold }, { { 666, 138, 320, 120 }, "LFO 2", kGold }, { { 666, 266, 320, 234 }, "MACROS (GLOBAL)", kTeal } };
    for (int s = 1; s <= 8; ++s) { float y = 40.f + (s - 1) * 56.f; String b = "m" + String(s) + "_";
        C(*pg, p, b + "src", s == 1 ? "Source" : "", EDIT, 28, y + (s == 1 ? 0 : 13), 190, s == 1 ? 53 : 40); C(*pg, p, b + "dst", s == 1 ? "Destination" : "", EDIT, 228, y + (s == 1 ? 0 : 13), 190, s == 1 ? 53 : 40);
        auto* a = pg->add<HSlider>(p, b + "amt", "", EDIT); a->setBounds(432, (int) y + 22, 206, 26); }
    for (int l = 1; l <= 2; ++l) { float y = l == 1 ? 40.f : 168.f; K(*pg, p, "l" + String(l) + "_rate", "Rate", EDIT, 684, y, 80, 80); C(*pg, p, "l" + String(l) + "_shape", "Shape", EDIT, 790, y + 14, 180, 52); }
    for (int i = 0; i < 8; ++i) K(*pg, p, "macro" + String(i + 1), rg::MACRO_NAMES[i], GLOBAL, 680.f + (i % 4) * 76, 296.f + (i / 4) * 100.f, 70, 80);
    return pg;
}
Page* makeArpPage(RaygunProcessor& p) {
    auto* pg = new Page();
    pg->panels = { { { 14, 10, 972, 190 }, "ARPEGGIATOR  (per layer, tempo follows host BPM)", kGold }, { { 14, 210, 972, 290 }, "NOTES", kGoldDim } };
    auto* on = pg->add<Switch>(p, "arp_on", "Arp on", EDIT); on->setBounds(30, 50, 130, 44);
    C(*pg, p, "arp_rate", "Rate", EDIT, 180, 46, 140, 52); C(*pg, p, "arp_mode", "Pattern", EDIT, 336, 46, 160, 52);
    K(*pg, p, "arp_oct", "Octaves", EDIT, 530, 40, 90, 90); K(*pg, p, "arp_gate", "Gate", EDIT, 630, 40, 90, 90);
    pg->panels.push_back({ { 30, 236, 940, 250 }, "", Colours::transparentBlack });
    return pg;
}
Page* makeFxPage(RaygunProcessor& p) {
    auto* pg = new Page();
    pg->panels = { { { 14, 10, 236, 490 }, "CHARGE", kRed }, { { 258, 10, 236, 490 }, "CHORUS", kGold }, { { 502, 10, 236, 490 }, "DELAY", kGold }, { { 746, 10, 240, 490 }, "REVERB", kGold } };
    K(*pg, p, "charge", "Charge", EDIT, 52, 100, 160, 160);
    K(*pg, p, "chorus", "Mix", EDIT, 296, 100, 160, 160);
    K(*pg, p, "delay_mix", "Mix", EDIT, 540, 60, 150, 110); K(*pg, p, "delay_time", "Time", EDIT, 540, 190, 150, 110); K(*pg, p, "delay_fb", "Feedback", EDIT, 540, 320, 150, 110);
    K(*pg, p, "verb_mix", "Mix", EDIT, 790, 80, 150, 130); K(*pg, p, "verb_size", "Size", EDIT, 790, 250, 150, 130);
    return pg;
}

struct MultiPage : Page {
    RaygunProcessor& proc; Pill* browseA; Pill* browseB; std::function<void(int)> onBrowse;
    explicit MultiPage(RaygunProcessor& p) : proc(p) {
        panels = { { { 14, 10, 972, 150 }, "LAYER MODE", kGold }, { { 14, 170, 480, 330 }, "BARREL A", kRed }, { { 506, 170, 480, 330 }, "BARREL B", kTeal } };
        auto* mode = add<SegBar>(p, "mode", GLOBAL); mode->setBounds(28, 40, 470, 38);
        K(*this, p, "split", "Split point", GLOBAL, 520, 36, 76, 76); K(*this, p, "xfade", "Crossfade", GLOBAL, 604, 36, 76, 76);
        auto* bl = add<HSlider>(p, "blend", "A / B blend (layer mode)", GLOBAL); bl->setBounds(28, 96, 330, 36);
        K(*this, p, "vsplit", "Vel split", GLOBAL, 700, 36, 76, 76); K(*this, p, "vfade", "Vel fade", GLOBAL, 784, 36, 76, 76);
        for (int l = 0; l < 2; ++l) { float x = l == 0 ? 28.f : 520.f;
            K(*this, p, "vol", "Volume", l, x, 244, 70, 76); K(*this, p, "pan", "Pan", l, x + 80, 244, 70, 76); K(*this, p, "oct", "Octave", l, x + 160, 244, 70, 76);
            K(*this, p, "keylo", "Key low", l, x + 240, 244, 70, 76); K(*this, p, "keyhi", "Key high", l, x + 320, 244, 70, 76); K(*this, p, "midich", "MIDI ch", l, x + 400, 244, 56, 76);
            auto* v = add<SegBar>(p, "voice", l); v->setBounds((int) x, 340, 230, 34); K(*this, p, "maxv", "Voices", l, x + 250, 330, 60, 70); K(*this, p, "glide", "Glide", l, x + 316, 330, 60, 70);
            auto* b = add<Pill>(l == 0 ? "Browse A" : "Browse B", l == 0 ? kRed : kTeal); b->setBounds((int) x, 436, 140, 40); b->onClick = [this, l] { if (onBrowse) onBrowse(l); }; }
    }
    void paint(Graphics& g) override { Page::paint(g);
        for (int l = 0; l < 2; ++l) { float x = l == 0 ? 28.f : 520.f; drawText(g, proc.presetName[l], { x, 196, 440, 34 }, Fonts::get().title(17.f), kCream, Justification::centredLeft); } }
};

struct SettingsPage : Page {
    RaygunProcessor& proc; std::function<void(int)> onScale; std::function<void()> onFolder, onRescan;
    explicit SettingsPage(RaygunProcessor& p) : proc(p) {
        panels = { { { 14, 10, 480, 150 }, "DISPLAY SIZE", kGold }, { { 506, 10, 480, 150 }, "QUALITY", kGold }, { { 14, 170, 480, 330 }, "KEYBOARD & MIDI SAFETY", kTeal }, { { 506, 170, 480, 330 }, "PRESETS & ABOUT", kGold } };
        int pcts[] = { 75, 100, 125, 150, 200 };
        for (int i = 0; i < 5; ++i) { auto* b = add<Pill>(String(pcts[i]) + "%"); b->setBounds(28 + i * 90, 56, 80, 40); int pc = pcts[i]; b->onClick = [this, pc] { if (onScale) onScale(pc); }; }
        auto* q = add<SegBar>(p, "quality", GLOBAL); q->setBounds(520, 56, 300, 40);
        auto* f = add<Pill>("Open preset folder"); f->setBounds(520, 232, 200, 40); f->onClick = [this] { if (onFolder) onFolder(); };
        auto* r = add<Pill>("Rescan user presets"); r->setBounds(730, 232, 200, 40); r->onClick = [this] { if (onRescan) onRescan(); };
    }
    void paint(Graphics& g) override { Page::paint(g); auto& F = Fonts::get();
        g.setColour(kCream); g.setFont(F.ui(15.5f));
        g.drawFittedText("ECO: unison capped at 3, LP24 runs as LP12, chorus off, lighter reverb.  CLEAN: full quality.  HOT: extra output drive.", 520, 104, 450, 50, Justification::topLeft, 3);
        g.drawFittedText("RAYGUN never takes your computer keyboard. Clicking a knob, key or button does not grab keyboard focus, so FL Studio's typing keyboard, piano roll shortcuts and your MIDI controller keep working. Only the preset search box accepts typing - click it, type, press Enter or Esc, and focus is handed straight back.", 28, 200, 450, 160, Justification::topLeft, 8);
        g.drawFittedText("MIDI comes from the host only: notes, pitch bend, mod wheel (CC1), sustain (CC64), channel pressure. Per-layer MIDI channel and key range are on the MULTI tab.", 28, 372, 450, 100, Justification::topLeft, 5);
        drawText(g, "RAYGUN by TRILL TRON  v1.0.0", { 520, 296, 450, 24 }, F.title(13.f), kGold, Justification::centredLeft);
        g.setColour(kCream); g.setFont(F.ui(15.f));
        g.drawFittedText("Hybrid wavetable synth. Two barrels (layers), 17 wavetables - five cut from Trill Tron stems - macros, mod matrix, arp. Original design and sounds. User presets: Documents/Trill Tron/RAYGUN/User Presets.", 520, 326, 450, 120, Justification::topLeft, 6);
        drawText(g, "Fonts: Orbitron & Rajdhani (SIL OFL).", { 520, 460, 450, 20 }, F.ui(13.f), kCream.withAlpha(0.6f), Justification::centredLeft);
    }
};
} // namespace

// =====================================================================================================================
//  Editor
// =====================================================================================================================
void RaygunEditor::Content::paint(Graphics& g) { if (++paints <= 3) rg::dbg("content paint #" + String(paints) + " size=" + String(getWidth()) + "x" + String(getHeight()) + " clip=" + g.getClipBounds().toString() + " scale=" + String(getTransform().getScaleFactor())); ed.paintHeader(g); }

void RaygunEditor::headerClick(Point<float> p) {
    if (p.y > HEADER) return;
    if (prevR.expanded(4).contains(p)) stepPreset(-1);
    else if (nextR.expanded(4).contains(p)) stepPreset(1);
    else if (starR.expanded(2).contains(p)) { if (auto* pi = presetFor(proc.editLayer)) { proc.presets.toggleFav(pi->name); content.repaint(); } }
    else if (nameR.contains(p)) browser->show();
}

const rg::PresetInfo* RaygunEditor::presetFor(int layer) const {
    for (auto& p : proc.presets.presets) if (p.name == proc.presetName[layer]) return &p;
    return nullptr; }

RaygunEditor::RaygunEditor(RaygunProcessor& p) : AudioProcessorEditor(&p), proc(p) {
    setLookAndFeel(&look); setWantsKeyboardFocus(false); setMouseClickGrabsKeyboardFocus(false);
    setOpaque(true);
    addAndMakeVisible(content); content.setWantsKeyboardFocus(false); content.setMouseClickGrabsKeyboardFocus(false);
    content.setBufferedToImage(false);
    content.setOpaque(true);

    auto* main = new MainPage(p); main->presetGetter = [this] { return presetFor(proc.editLayer); };
    auto* multi = new MultiPage(p); auto* sett = new SettingsPage(p);
    pages.add(main); pages.add(makeOscPage(p)); pages.add(makeFilterPage(p)); pages.add(makeModPage(p)); pages.add(makeArpPage(p)); pages.add(makeFxPage(p)); pages.add(multi); pages.add(sett);
    for (auto* pg : pages) { content.addChildComponent(pg); pg->setBounds(0, HEADER, W, PAGE_H); pg->setWantsKeyboardFocus(false); pg->setMouseClickGrabsKeyboardFocus(false); }
    pages[0]->setVisible(true);

    // header
    auto* mb = new IconButton(0); auto* gb = new IconButton(1);
    content.addAndMakeVisible(mb); mb->setBounds(10, 6, 40, 34); mb->onClick = [this] { openMenu(); };
    content.addAndMakeVisible(gb); gb->setBounds(950, 6, 40, 34); gb->onClick = [this] { selectTab(7); };
    content.addAndMakeVisible(editPill); editPill.setBounds(358, 8, 66, 30); editPill.onClick = [this] { setEditLayer(1 - proc.editLayer); };
    modeBar = std::make_unique<SegBar>(proc, "mode", GLOBAL, StringArray { "A", "B", "A+B", "SPLIT", "VEL" }); content.addAndMakeVisible(*modeBar); modeBar->setBounds(432, 6, 246, 34);
    qualBar = std::make_unique<SegBar>(proc, "quality", GLOBAL); content.addAndMakeVisible(*qualBar); qualBar->setBounds(688, 6, 132, 34);
    static const char* tn[] = { "MAIN", "OSC", "FILTER", "MOD", "ARP/SEQ", "FX", "MULTI", "SETTINGS" };
    for (int i = 0; i < 8; ++i) { auto* t = tabs.add(new Pill(tn[i])); content.addAndMakeVisible(t); t->setBounds(12 + i * 122, H - FOOTER + 6, 116, 32); t->onClick = [this, i] { selectTab(i); }; }
    tabs[0]->toggled = true;
    multi->onBrowse = [this](int l) { setEditLayer(l); browser->show(); };
    sett->onScale = [this](int pct) { proc.uiScalePercent = pct; setSize(W * pct / 100, H * pct / 100); };
    sett->onFolder = [] { rg::PresetManager::userDir().getChildFile("User Presets").revealToUser(); };
    sett->onRescan = [this] { proc.presets.loadAll(BinaryData::factory_json, (size_t) BinaryData::factory_jsonSize); browser->reload(); };

    browser = std::make_unique<Browser>(proc); content.addChildComponent(*browser); browser->setBounds(0, 0, W, H);
    browser->onClose = [this] { browser->setVisible(false); };
    browser->onLoaded = [this] { setEditLayer(proc.editLayer); };

    setEditLayer(proc.editLayer);
    constrainer.setFixedAspectRatio((double) W / H); constrainer.setSizeLimits(W * 3 / 4, H * 3 / 4, W * 2, H * 2);
    setConstrainer(&constrainer); setResizable(true, false);
    int pct = jlimit(75, 200, proc.uiScalePercent); setSize(W * pct / 100, H * pct / 100);
    startTimerHz(30);
}

RaygunEditor::~RaygunEditor() { setLookAndFeel(nullptr); stopTimer(); }

void RaygunEditor::resized() {
    float s = (float) getWidth() / (float) W;
    content.setTransform(AffineTransform::scale(s)); content.setBounds(0, 0, W, H);
}

void RaygunEditor::selectTab(int i) {
    tab = i; for (int k = 0; k < pages.size(); ++k) pages[k]->setVisible(k == i); for (int k = 0; k < tabs.size(); ++k) { tabs[k]->toggled = k == i; tabs[k]->repaint(); }
    content.repaint(); }

void RaygunEditor::setEditLayer(int l) {
    proc.editLayer = l; for (auto* pg : pages) pg->rebind(l);
    editPill.text = l == 0 ? "Edit A" : "Edit B"; editPill.col = l == 0 ? kRed : kTeal; editPill.repaint(); content.repaint(); }

void RaygunEditor::stepPreset(int dir) {
    auto& all = proc.presets.presets; if (all.isEmpty()) return;
    Array<int> order; for (int i = 0; i < all.size(); ++i) order.add(i);
    std::sort(order.begin(), order.end(), [&](int a, int b) { return all.getReference(a).name.compareIgnoreCase(all.getReference(b).name) < 0; });
    int cur = -1; for (int i = 0; i < order.size(); ++i) if (all.getReference(order[i]).name == proc.presetName[proc.editLayer]) cur = i;
    // From "Init" / an unsaved sound (not in the list): Next -> first preset, Previous -> last preset.
    int nx = cur < 0 ? (dir > 0 ? 0 : order.size() - 1) : (cur + dir + order.size()) % order.size();
    proc.loadPreset(all.getReference(order[nx]), proc.editLayer); setEditLayer(proc.editLayer);
}

void RaygunEditor::openMenu() {
    PopupMenu m; m.addItem(1, "Init patch"); m.addItem(2, "Save preset (edit layer)..."); m.addItem(3, "Open preset folder"); m.addSeparator(); m.addItem(4, "Preset browser"); m.addItem(5, "About RAYGUN");
    m.showMenuAsync(PopupMenu::Options().withTargetComponent(&content).withMinimumWidth(220), [this](int r) {
        if (r == 1) { proc.initPatch(); setEditLayer(proc.editLayer); }
        else if (r == 2) {
            auto* w = new AlertWindow("Save preset", "Name for the edit layer's sound:", MessageBoxIconType::NoIcon); w->addTextEditor("name", proc.presetName[proc.editLayer] == "Init" ? "My Sound" : proc.presetName[proc.editLayer]); w->addButton("Save", 1, KeyPress(KeyPress::returnKey)); w->addButton("Cancel", 0, KeyPress(KeyPress::escapeKey));
            w->enterModalState(true, ModalCallbackFunction::create([this, w](int res) { if (res == 1) { auto n = w->getTextEditorContents("name").trim(); if (n.isNotEmpty() && proc.saveUserPreset(n, proc.editLayer)) { proc.presetName[proc.editLayer] = n; browser->reload(); content.repaint(); } } delete w; }), false);
        }
        else if (r == 3) rg::PresetManager::userDir().getChildFile("User Presets").revealToUser();
        else if (r == 4) browser->show();
        else if (r == 5) AlertWindow::showMessageBoxAsync(MessageBoxIconType::NoIcon, "RAYGUN by TRILL TRON", "Version 1.0.0\nTwin-barrel hybrid wavetable synth.\nAll sounds, wavetables and artwork are original.", "OK");
    });
}

void RaygunEditor::timerCallback() {
    // JUCE 8 (Windows) only queues repaints and flushes them on a vertical-blank signal. Some hosts (FL Studio's embedded plugin
    // windows) never deliver it, leaving the window black after its first paint. Flush the queue ourselves.
    if (auto* peer = getPeer()) peer->performAnyPendingRepaintsNow();
    static int ticks = 0;
#if defined(RAYGUN_VST2_BUILD) && JUCE_WINDOWS
    if (ticks == 20) if (auto* pe = getPeer()) rg_logWindowInfo(pe->getNativeHandle(), "editor +0.7s");
#endif
    if (++ticks == 40 || ticks == 400) rg::dbg("editor timer alive: ticks=" + String(ticks) + " content paints=" + String(content.paints) + " editorSize=" + String(getWidth()) + "x" + String(getHeight()) + " showing=" + String((int) isShowing()));
    int cells = jlimit(0, 6, (int) std::ceil(proc.cpuLoad.load() * 100.f / 10.f)); if (cells != lastCpuCells) { lastCpuCells = cells; }
    content.repaint(0, 0, W, HEADER); if (tab == 0) pages[0]->repaint(); }

void RaygunEditor::paintHeader(Graphics& g) {
    auto& F = Fonts::get();
    g.fillAll(kBg);
    { ColourGradient bg(Colour(0xff1b0d0f), 0, HEADER, Colour(0xff0d0708), 0, H - FOOTER, false); g.setGradientFill(bg); g.fillRect(0, HEADER, W, PAGE_H); }
    { g.setColour(Colour(0x0cffffff)); for (int y = HEADER; y < H - FOOTER; y += 4) g.drawHorizontalLine(y, 0, (float) W); }   // faint scanlines
    // header bar
    g.setGradientFill(ColourGradient(Colour(0xff2a140c), 0, 0, Colour(0xff150a0a), 0, HEADER, false)); g.fillRect(0, 0, W, HEADER); g.setColour(kGold); g.fillRect(0, HEADER - 2, W, 2);
    // footer
    g.setColour(Colour(0xff100809)); g.fillRect(0, H - FOOTER, W, FOOTER); g.setColour(kGold); g.fillRect(0, H - FOOTER, W, 2);
    // preset box
    g.setColour(Colour(0xff0e0708)); g.fillRoundedRectangle(presetBox, 9.f); g.setColour(kGoldDim); g.drawRoundedRectangle(presetBox, 9.f, 1.5f);
    prevR = { 64, 10, 26, 26 }; nextR = { 318, 10, 26, 26 }; starR = { 288, 10, 26, 26 }; nameR = { 94, 5, 190, 36 };
    Path t1; t1.addTriangle(82, 14, 82, 32, 70, 23); g.setColour(kGold); g.fillPath(t1); Path t2; t2.addTriangle(326, 14, 326, 32, 338, 23); g.fillPath(t2);
    const rg::PresetInfo* pi = presetFor(proc.editLayer);
    drawText(g, proc.presetName[proc.editLayer], { 94, 6, 190, 20 }, F.bold(20.f), kCream, Justification::centredLeft);
    drawText(g, presetSub(pi), { 94, 25, 190, 14 }, F.ui(12.5f), kGold.withAlpha(0.8f), Justification::centredLeft);
    bool fav = pi && proc.presets.isFav(pi->name); drawText(g, fav ? String::fromUTF8("\xe2\x98\x85") : String::fromUTF8("\xe2\x98\x86"), starR, F.ui(26.f), fav ? kGold : kGoldDim);
    // power cell (CPU)
    float cpu = proc.cpuLoad.load(); int cells = jlimit(0, 6, (int) std::ceil(cpu * 100.f / 8.f));
    drawText(g, "POWER CELL", { 826, 2, 100, 12 }, F.title(7.f), kGold.withAlpha(0.8f), Justification::centredLeft);
    g.setColour(Colour(0xff0e0708)); g.fillRoundedRectangle(826, 15, 78, 20, 4.f); g.setColour(kSteel); g.drawRoundedRectangle(826, 15, 78, 20, 4.f, 1.5f);
    for (int i = 0; i < 6; ++i) { g.setColour(i < cells ? (i < 4 ? kTeal : (i < 5 ? kGold : kRed)) : Colour(0xff2a2020)); g.fillRect(829.f + i * 12.5f, 18.f, 10.f, 14.f); }
    drawText(g, String((int) std::round(cpu * 100.f)) + "%", { 906, 12, 40, 26 }, F.bold(16.f), kCream, Justification::centredLeft);
}
