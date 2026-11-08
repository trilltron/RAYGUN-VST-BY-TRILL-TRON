#pragma once
#include "PluginProcessor.h"
#include "DebugLog.h"
#include "Gui/Scope.h"
#include "Gui/Browser.h"

// Logical canvas is 1000x600; the whole UI is drawn as vectors and scaled by a transform (75%..200%, fixed 5:3 aspect).
namespace rgui {
constexpr int W = 1000, H = 600, HEADER = 46, FOOTER = 44, PAGE_H = H - HEADER - FOOTER;

struct Page : Component {
    OwnedArray<Component> owned; std::vector<Bindable*> binds;
    template <class T, class... A> T* add(A&&... a) { auto* c = new T(std::forward<A>(a)...); owned.add(c); addAndMakeVisible(c); if (auto* b = dynamic_cast<Bindable*>(c)) binds.push_back(b); return c; }
    void rebind(int layer) { for (auto* b : binds) b->bind(layer); repaint(); }
    struct PanelDef { Rectangle<float> r; String title; Colour c; };
    std::vector<PanelDef> panels;
    void paint(Graphics& g) override { for (auto& p : panels) drawPanel(g, p.r, p.title, p.c); }
};
} // namespace rgui

class RaygunEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit RaygunEditor(RaygunProcessor&);
    ~RaygunEditor() override;
    void paint(juce::Graphics&) override { }
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override { return false; }   // never consume the host's keyboard
    void selectTab(int i);
    // Some hosts (e.g. FL Studio) show a black window with JUCE 8's Direct2D renderer: always use the software renderer.
    void forceSoftwareRenderer() {
        if (auto* peer = getPeer()) {
            rg::dbg("renderer: engines=" + peer->getAvailableRenderingEngines().joinIntoString(",") + " current=" + juce::String(peer->getCurrentRenderingEngine())); auto engines = peer->getAvailableRenderingEngines();
            for (int i = 0; i < engines.size(); ++i) if (engines[i].containsIgnoreCase("software") && peer->getCurrentRenderingEngine() != i) peer->setCurrentRenderingEngine(i); } }
    void parentHierarchyChanged() override { forceSoftwareRenderer(); }
    void visibilityChanged() override { forceSoftwareRenderer(); }
    void showBrowser() { browser->show(); }
private:
    struct Content : juce::Component { RaygunEditor& ed; int paints = 0; explicit Content(RaygunEditor& e) : ed(e) {} void paint(juce::Graphics& g) override; };
    void timerCallback() override;
    void setEditLayer(int l);
    void stepPreset(int dir);
    void openMenu();
    void paintHeader(juce::Graphics&);
    void headerClick(juce::Point<float>);
    const rg::PresetInfo* presetFor(int layer) const;

    RaygunProcessor& proc;
    rgui::Look look;
    juce::ComponentBoundsConstrainer constrainer;
    Content content { *this };
    juce::OwnedArray<rgui::Page> pages;
    juce::OwnedArray<rgui::Pill> tabs;
    rgui::Pill editPill { "Edit A" };
    std::unique_ptr<rgui::SegBar> modeBar, qualBar;
    std::unique_ptr<rgui::Browser> browser;
    int tab = 0; int lastCpuCells = -1;
    juce::Rectangle<float> presetBox { 60, 5, 290, 36 };
    juce::Rectangle<float> prevR, nextR, starR, nameR;
    friend struct rgui::Page;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RaygunEditor)
};
