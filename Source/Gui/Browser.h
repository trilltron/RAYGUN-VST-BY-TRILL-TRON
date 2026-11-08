#pragma once
#include "Widgets.h"

namespace rgui {
// Full-window preset browser ("Preset Arsenal"): type / character / style / genre chips, search, list, details, load into A / B.
class Browser : public Component, private ListBoxModel {
public:
    std::function<void()> onClose, onLoaded;
    explicit Browser(RaygunProcessor& p) : proc(p) {
        NoFocus::apply(*this);
        search.setTextToShowWhenEmpty("Search name, tag, author...  (click to type)", kGoldDim.withAlpha(0.8f));
        search.setFont(Fonts::get().ui(18.f)); search.setWantsKeyboardFocus(true); search.setMouseClickGrabsKeyboardFocus(true);
        search.onTextChange = [this] { refilter(); };
        // hand the keyboard straight back to the DAW
        search.onReturnKey = [this] { search.giveAwayKeyboardFocus(); }; search.onEscapeKey = [this] { search.giveAwayKeyboardFocus(); };
        search.onFocusLost = [this] { search.setHighlightedRegion({}); };
        addAndMakeVisible(search);
        list.setModel(this); list.setRowHeight(33); list.setWantsKeyboardFocus(false); list.setMouseClickGrabsKeyboardFocus(false); list.getViewport()->setWantsKeyboardFocus(false);
        list.getViewport()->setScrollBarsShown(true, false); addAndMakeVisible(list);
        addAndMakeVisible(close); close.text = "X"; close.onClick = [this] { if (onClose) onClose(); };
        for (auto* b : { &loadA, &loadB, &favBtn, &clearBtn }) addAndMakeVisible(*b);
        loadA.text = "Load into Layer A"; loadA.col = kRed; loadB.text = "Load into Layer B"; loadB.col = kTeal;
        loadA.onClick = [this] { loadSelected(0); }; loadB.onClick = [this] { loadSelected(1); };
        favBtn.text = "Favorites"; favBtn.onClick = [this] { favOnly = !favOnly; favBtn.toggled = favOnly; refilter(); };
        clearBtn.text = "Clear"; clearBtn.onClick = [this] { selType.clear(); selChar.clear(); selStyle.clear(); selGenre.clear(); favOnly = false; favBtn.toggled = false; search.clear(); syncChips(); refilter(); };
        buildChips(); refilter();
    }
    void reload() { buildChips(); resized(); refilter(); }
    void show() { setVisible(true); toFront(false); refilter();
        for (int i = 0; i < proc.presets.presets.size(); ++i) if (proc.presets.presets.getReference(i).name == proc.presetName[proc.editLayer]) selectedIndex = i;
        repaint(); }

    void paint(Graphics& g) override {
        g.fillAll(kBg); auto& F = Fonts::get();
        g.setColour(kGold); g.fillRect(0, 51, getWidth(), 2);
        // little ray-gun logo
        g.setColour(kRed); g.fillRoundedRectangle(14, 16, 18, 18, 9); g.fillRoundedRectangle(28, 18, 36, 14, 7); g.setColour(kSteel); g.fillRect(60, 22, 26, 6);
        Path horn; horn.addTriangle(86, 25, 100, 15, 100, 35); g.setColour(kRed); g.fillPath(horn);
        drawText(g, "RAYGUN", { 112, 4, 160, 30 }, F.title(26.f), kGold, Justification::centredLeft); drawText(g, "PRESET ARSENAL", { 113, 32, 200, 16 }, F.title(9.f), kCream.withAlpha(0.8f), Justification::centredLeft);
        drawText(g, String(shown.size()) + " of " + String(proc.presets.presets.size()) + " presets", { 620, 8, 300, 36 }, F.bold(19.f), kCream, Justification::centredRight);
        // left column
        g.setColour(Colour(0xff0e0708)); g.fillRect(0, 53, 275, getHeight() - 53); g.setColour(kGoldDim.withAlpha(0.5f)); g.drawVerticalLine(275, 53, (float) getHeight());
        for (auto& s : sections) drawText(g, s.title, { 14.f, (float) s.y, 200.f, 16.f }, F.title(10.f), kGold, Justification::centredLeft);
        // header row of list
        drawText(g, "NAME", { 330, 56, 150, 18 }, F.title(9.5f), kGold, Justification::centredLeft); drawText(g, "TYPE", { 478, 56, 70, 18 }, F.title(9.5f), kGold, Justification::centredLeft);
        drawText(g, "CHARACTER", { 548, 56, 100, 18 }, F.title(9.5f), kGold, Justification::centredLeft); drawText(g, "TAGS", { 650, 56, 100, 18 }, F.title(9.5f), kGold, Justification::centredLeft);
        // details
        g.setColour(Colour(0xff0e0708)); g.fillRect(775, 53, getWidth() - 775, getHeight() - 53); g.setColour(kGoldDim.withAlpha(0.5f)); g.drawVerticalLine(775, 53, (float) getHeight());
        if (auto* pi = current()) {
            float x = 790; drawText(g, "LOADED / SELECTED", { x, 66, 200, 14 }, F.title(9.f), kGold, Justification::centredLeft);
            drawText(g, pi->name, { x, 82, 200, 30 }, F.title(16.f), kCream, Justification::centredLeft);
            g.setColour(kCream.withAlpha(0.85f)); g.setFont(F.ui(14.5f)); g.drawFittedText(pi->desc, (int) x, 116, 192, 58, Justification::topLeft, 4);
            int y = 186; auto kv = [&](const String& k, const String& v, Colour c) { drawText(g, k, { x, (float) y, 70, 18 }, F.ui(14.f), kGold, Justification::centredLeft); drawText(g, v, { x + 76, (float) y, 120, 18 }, F.ui(14.f), c, Justification::centredLeft); y += 22; };
            kv("Type", pi->type, kCream); kv("Character", pi->character, kTeal); kv("Key / BPM", (pi->key.isEmpty() ? String("-") : pi->key) + (pi->bpm > 0 ? " / " + String(pi->bpm) : String()), kCream); kv("Author", pi->author, kCream);
            drawText(g, "MACROS", { x, 290, 100, 16 }, F.title(10.f), kGold, Justification::centredLeft);
            StringArray m; for (auto* n : rg::MACRO_NAMES) m.add(String(n).toUpperCase());
            g.setColour(kCream.withAlpha(0.85f)); g.setFont(F.ui(14.f)); g.drawFittedText(m.joinIntoString(" \xc2\xb7 "), (int) x, 310, 192, 50, Justification::topLeft, 3);
        }
    }
    void resized() override {
        search.setBounds(270, 10, 330, 32); close.setBounds(getWidth() - 60, 10, 46, 32);
        list.setBounds(276, 76, 498, getHeight() - 76);
        loadA.setBounds(787, getHeight() - 98, 200, 40); loadB.setBounds(787, getHeight() - 50, 200, 40);
        favBtn.setBounds(12, getHeight() - 52, 100, 34); clearBtn.setBounds(194, getHeight() - 52, 72, 34);
        layoutChips();
    }
private:
    struct Chip : Pill { using Pill::Pill; String cat, value; };
    struct Section { String title, cat; int y = 0; };
    RaygunProcessor& proc; TextEditor search; ListBox list { {}, nullptr }; Pill close { "X" }, loadA { "A", kRed }, loadB { "B", kTeal }, favBtn { "Fav" }, clearBtn { "Clear" };
    OwnedArray<Chip> chips; Array<Section> sections; StringArray selType, selChar, selStyle, selGenre; bool favOnly = false;
    Array<int> shown; int selectedRow = -1; int selectedIndex = -1;

    const rg::PresetInfo* current() const {
        if (selectedIndex >= 0 && selectedIndex < proc.presets.presets.size()) return &proc.presets.presets.getReference(selectedIndex);
        return nullptr; }
    StringArray& selFor(const String& cat) { return cat == "type" ? selType : cat == "character" ? selChar : cat == "style" ? selStyle : selGenre; }
    void buildChips() {
        chips.clear(); sections.clear();
        static const StringArray typeOrder { "Bass", "808", "Lead", "Pad", "Keys", "Pluck", "Bell", "Brass", "Strings", "Arp", "Vox", "FX" };
        auto add = [&](const String& title, const String& cat, StringArray vals) {
            sections.add({ title, cat, 0 });
            for (auto& v : vals) { auto* c = chips.add(new Chip(v, kGold)); c->cat = cat; c->value = v; c->text = v; addAndMakeVisible(c);
                c->onClick = [this, c] { auto& s = selFor(c->cat); if (s.contains(c->value)) s.removeString(c->value); else s.add(c->value); c->toggled = s.contains(c->value); refilter(); }; } };
        StringArray types; for (auto& t : typeOrder) if (proc.presets.allValues("type").contains(t)) types.add(t);
        auto chr = proc.presets.allValues("character"); chr.sort(true); auto sty = proc.presets.allValues("style"); sty.sort(true); auto gen = proc.presets.allValues("genre"); gen.sort(true);
        add("TYPE", "type", types); add("CHARACTER", "character", chr); add("STYLE", "style", sty); add("GENRE", "genre", gen); layoutChips();
    }
    void layoutChips() {
        int x = 12, y = 60, secIdx = -1; String last; int rowH = 30;
        for (auto* c : chips) {
            if (c->cat != last) { last = c->cat; ++secIdx; if (x != 12) y += rowH; x = 12; y += (secIdx ? 6 : 0); sections.getReference(secIdx).y = y; y += 20; }
            int w = (int) Fonts::get().bold(15.f).getStringWidthFloat(c->text.toUpperCase()) + 26;
            if (x + w > 266) { x = 12; y += rowH; }
            c->setBounds(x, y, w, 26); x += w + 6; }
    }
    void syncChips() { for (auto* c : chips) c->toggled = selFor(c->cat).contains(c->value); for (auto* c : chips) c->repaint(); }
    void refilter() {
        shown.clear(); auto q = search.getText().toLowerCase().trim(); StringArray terms; terms.addTokens(q, " ", ""); terms.removeEmptyStrings();
        for (int i = 0; i < proc.presets.presets.size(); ++i) {
            auto& p = proc.presets.presets.getReference(i);
            if (selType.size() && !selType.contains(p.type)) continue;
            if (selChar.size() && !selChar.contains(p.character)) continue;
            if (selStyle.size()) { bool ok = false; for (auto& s : p.style) ok |= selStyle.contains(s); if (!ok) continue; }
            if (selGenre.size()) { bool ok = false; for (auto& s : p.genre) ok |= selGenre.contains(s); if (!ok) continue; }
            if (favOnly && !proc.presets.isFav(p.name)) continue;
            auto blob = p.searchBlob(); bool all = true; for (auto& t : terms) all &= blob.contains(t); if (!all) continue;
            shown.add(i); }
        std::sort(shown.begin(), shown.end(), [this](int a, int b) { return proc.presets.presets.getReference(a).name.compareIgnoreCase(proc.presets.presets.getReference(b).name) < 0; });
        list.updateContent(); list.repaint(); repaint();
    }
    void loadSelected(int layer) {
        if (auto* p = current()) { proc.loadPreset(*p, layer); proc.editLayer = p->isMulti() ? proc.editLayer : layer; if (onLoaded) onLoaded(); repaint(); } }

    // ---- ListBoxModel
    int getNumRows() override { return shown.size(); }
    void paintListBoxItem(int row, Graphics& g, int w, int h, bool selected) override {
        if (row >= shown.size()) return; auto& p = proc.presets.presets.getReference(shown[row]); auto& F = Fonts::get();
        bool loaded = p.name == proc.presetName[proc.editLayer];
        if (loaded) { g.setColour(kRedDeep); g.fillRect(0, 0, w, h); g.setColour(kRed); g.fillRect(0, 0, 3, h); } else if (selected) { g.setColour(Colour(0xff2a1216)); g.fillRect(0, 0, w, h); }
        g.setColour(Colour(0xff2a1a1c)); g.drawHorizontalLine(h - 1, 0, (float) w);
        bool fav = proc.presets.isFav(p.name); drawText(g, fav ? String::fromUTF8("\xe2\x98\x85") : String::fromUTF8("\xe2\x98\x86"), { 6, 0, 32, (float) h }, F.ui(20.f), fav ? kGold : kGoldDim.withAlpha(0.6f));
        drawText(g, p.name, { 54, 0, 148, (float) h }, F.bold(17.f), kCream, Justification::centredLeft);
        drawText(g, p.type, { 202, 0, 70, (float) h }, F.ui(15.f), kCream.withAlpha(0.85f), Justification::centredLeft);
        drawText(g, p.character, { 272, 0, 102, (float) h }, F.ui(15.f), kTeal, Justification::centredLeft);
        drawText(g, p.tags.joinIntoString(" \xc2\xb7 "), { 374, 0, 122, (float) h }, F.ui(13.5f), kGold.withAlpha(0.7f), Justification::centredLeft);
    }
    void listBoxItemClicked(int row, const MouseEvent& e) override {
        if (row < 0 || row >= shown.size()) return; auto& p = proc.presets.presets.getReference(shown[row]); selectedIndex = shown[row];
        if (e.x < 40) proc.presets.toggleFav(p.name);
        list.repaint(); repaint(); }
    void listBoxItemDoubleClicked(int row, const MouseEvent&) override { if (row >= 0 && row < shown.size()) { selectedIndex = shown[row]; loadSelected(proc.editLayer); } }
    void selectedRowsChanged(int row) override { if (row >= 0 && row < shown.size()) { selectedIndex = shown[row]; repaint(); } }
};
} // namespace rgui
