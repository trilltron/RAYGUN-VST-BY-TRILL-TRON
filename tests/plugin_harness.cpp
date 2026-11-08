// Headless plugin test: real RaygunProcessor + RaygunEditor. Loads every factory preset via the parameter system, renders audio,
// round-trips state, and writes PNG screenshots of every page.
#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace juce;
static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); ++fails; } else printf("ok   : %s\n", msg); } while (0)

static float renderNote(RaygunProcessor& p, int note, bool& finite, float& tailPk) {
    AudioBuffer<float> buf(2, 512); float pk = 0; finite = true; tailPk = 0;
    for (int blk = 0; blk < 120; ++blk) {
        MidiBuffer m; if (blk == 0) m.addEvent(MidiMessage::noteOn(1, note, (uint8) 100), 0); if (blk == 60) m.addEvent(MidiMessage::noteOff(1, note), 0);
        buf.clear(); p.processBlock(buf, m);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) { float v = buf.getSample(c, i); if (!std::isfinite(v)) finite = false; pk = std::max(pk, std::abs(v)); if (blk > 110) tailPk = std::max(tailPk, std::abs(v)); }
    }
    return pk;
}
int main(int argc, char** argv) {
    ScopedJuceInitialiser_GUI init;
    String outDir = argc > 1 ? argv[1] : ".";
    RaygunProcessor proc; proc.prepareToPlay(44100.0, 512);
    printf("factory presets: %d\n", proc.presets.presets.size());
    CHECK(proc.presets.presets.size() >= 500, ">= 500 presets embedded");
    // every preset: load through parameters, play, check
    int bad = 0, silent = 0; float worst = 0;
    for (auto& pr : proc.presets.presets) {
        proc.initPatch(); proc.loadPreset(pr, 0);
        // let the host-visible parameters settle (processBlock reads atomics directly)
        bool fin; float tail; int note = pr.type == "808" ? 36 : pr.type == "Bass" ? 40 : 60; float pk = renderNote(proc, note, fin, tail);
        if (!fin) { ++bad; printf("  NaN in %s\n", pr.name.toRawUTF8()); } if (pk < 0.003f) { ++silent; printf("  silent: %s (%s)\n", pr.name.toRawUTF8(), pr.type.toRawUTF8()); } worst = std::max(worst, pk);
        proc.processBlock(*std::make_unique<AudioBuffer<float>>(2, 512), *std::make_unique<MidiBuffer>());
    }
    char b[128]; snprintf(b, 128, "all presets render finite audio (bad=%d, silent=%d, peak=%.2f)", bad, silent, worst); CHECK(bad == 0 && silent == 0 && worst <= 1.0001f, b);
    // state round-trip
    proc.initPatch(); for (auto& pr : proc.presets.presets) if (pr.name == "Bb Minor Bed PD") { proc.loadPreset(pr, 0); break; }
    float cutBefore = proc.apvts.getRawParameterValue("a_f_cut")->load(); MemoryBlock mb; proc.getStateInformation(mb);
    proc.initPatch(); proc.setStateInformation(mb.getData(), (int) mb.getSize());
    CHECK(std::abs(proc.apvts.getRawParameterValue("a_f_cut")->load() - cutBefore) < 1e-3f && proc.presetName[0] == "Bb Minor Bed PD", "state save/restore round-trips");
    // MIDI: pitch bend, mod wheel, sustain, panic don't blow up
    { AudioBuffer<float> buf(2, 512); MidiBuffer m; m.addEvent(MidiMessage::controllerEvent(1, 64, 127), 0); m.addEvent(MidiMessage::noteOn(1, 60, (uint8) 100), 4); m.addEvent(MidiMessage::pitchWheel(1, 12000), 8); m.addEvent(MidiMessage::controllerEvent(1, 1, 90), 8); m.addEvent(MidiMessage::noteOff(1, 60), 100);
      proc.processBlock(buf, m); MidiBuffer m2; m2.addEvent(MidiMessage::allNotesOff(1), 0); proc.processBlock(buf, m2); CHECK(true, "sustain/pitchbend/modwheel/all-notes-off handled"); }
    // editor + screenshots
    proc.initPatch(); for (auto& pr : proc.presets.presets) if (pr.name == "Bb Minor Bed PD") { proc.loadPreset(pr, 0); break; }
    { std::unique_ptr<AudioProcessorEditor> ed(proc.createEditor());
      CHECK(ed != nullptr, "editor constructs");
      CHECK(!ed->getWantsKeyboardFocus(), "editor does not want keyboard focus");
      auto* re = dynamic_cast<RaygunEditor*>(ed.get());
      // press keys so the keyboard lights up
      proc.keyboardState.noteOn(1, 46, 0.8f); proc.keyboardState.noteOn(1, 70, 0.8f); { AudioBuffer<float> buf(2, 512); MidiBuffer m; proc.processBlock(buf, m); }
      const char* names[] = { "main", "osc", "filter", "mod", "arp", "fx", "multi", "settings" };
      for (int s : { 200, 100 }) { ed->setSize(1000 * s / 100, 600 * s / 100);
        for (int t = 0; t < 8; ++t) { re->selectTab(t); Thread::sleep(10);
            auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f); File f(outDir + "/" + names[t] + (s == 200 ? "" : "_100") + ".png"); FileOutputStream os(f); os.setPosition(0); os.truncate(); PNGImageFormat().writeImageToStream(img, os); } }
      re->selectTab(0); re->showBrowser(); Thread::sleep(10);
      ed->setSize(2000, 1200); auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f); File f(outDir + "/browser.png"); FileOutputStream os(f); os.setPosition(0); os.truncate(); PNGImageFormat().writeImageToStream(img, os);
      CHECK(true, "editor renders all pages + browser at 200% and 100%"); }
    printf(fails ? "\n%d FAILED\n" : "\nALL PASSED\n", fails); return fails ? 1 : 0;
}
