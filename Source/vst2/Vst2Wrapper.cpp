// RAYGUN VST2 wrapper: hosts the same RaygunProcessor / RaygunEditor as the VST3 build behind a VST 2.4 interface.
#if defined(_WIN32)
 #define VST_EXPORT extern "C" __declspec(dllexport)
#else
 #define VST_EXPORT extern "C" __attribute__((visibility("default")))
#endif
#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "Vst2Abi.h"
#include "../DebugLog.h"
#include <cstring>
#include <atomic>

using namespace vst2;

namespace {
struct HostPlayHead : juce::AudioPlayHead {
    double bpm = 120.0;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setBpm(bpm); return p; }
};

struct Wrapper : juce::AudioProcessorListener, juce::ComponentListener {
    VEffect fx {}; HostCb host = nullptr;
    std::unique_ptr<RaygunProcessor> proc; std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::MidiBuffer midi; juce::MemoryBlock chunk; VRect rect {}; HostPlayHead hp; double sr = 44100; int bs = 512; int lastW = 0, lastH = 0;
    static inline thread_local bool fromHost = false;

    void audioProcessorParameterChanged(juce::AudioProcessor*, int idx, float v) override { if (!fromHost && host) host(&fx, MAutomate, idx, 0, nullptr, v); }
    void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails&) override {}
    void componentMovedOrResized(juce::Component& c, bool, bool resized) override {
        if (resized && host && (c.getWidth() != lastW || c.getHeight() != lastH)) { lastW = c.getWidth(); lastH = c.getHeight(); host(&fx, MSizeWindow, lastW, lastH, nullptr, 0); } }
    juce::AudioProcessorParameter* param(int i) const { auto& p = proc->getParameters(); return i >= 0 && i < p.size() ? p[i] : nullptr; }
};

void copyStr(void* dst, const char* s, size_t maxLen) { if (!dst) return; std::strncpy((char*) dst, s, maxLen); ((char*) dst)[maxLen] = 0; }
Wrapper* W(VEffect* e) { return (Wrapper*) e->object; }

void processReplacing(VEffect* e, float**, float** out, int32_t n) {
    auto* w = W(e); { static std::atomic<bool> first { true }; if (first.exchange(false)) rg::dbg("first processReplacing n=" + juce::String(n)); } float* ch[2] = { out[0], out[1] };
    if (w->host) if (auto* ti = (VTimeInfo*) w->host(e, MGetTime, 0, kTempoValid, nullptr, 0)) if (ti->flags & kTempoValid) w->hp.bpm = ti->tempo;
    juce::AudioBuffer<float> buf(ch, 2, n); juce::MidiBuffer m; m.swapWith(w->midi);
    w->proc->processBlock(buf, m);
}
void processAccumulating(VEffect* e, float** in, float** out, int32_t n) {   // legacy path: replace is fine for a synth
    processReplacing(e, in, out, n); }
void setParameter(VEffect* e, int32_t i, float v) { auto* w = W(e); if (auto* p = w->param(i)) { Wrapper::fromHost = true; p->setValueNotifyingHost(v); Wrapper::fromHost = false; } }
float getParameter(VEffect* e, int32_t i) { auto* p = W(e)->param(i); return p ? p->getValue() : 0.f; }

intptr_t dispatch(VEffect* e, int32_t op, int32_t idx, intptr_t val, void* ptr, float opt) {
    auto* w = W(e);
    { static std::atomic<int> n { 0 }; if (op != EditIdle && op != GetParamName && op != GetParamDisplay && op != GetParamLabel && op != CanBeAutomated && op != GetProgram && op != 66 && n++ < 600) rg::dbg("opcode " + juce::String(op) + " idx=" + juce::String(idx) + " val=" + juce::String((juce::int64) val) + " ptr=" + juce::String((int) (ptr != nullptr))); }
    switch (op) {
    case Open: return 0;
    case Close: {
        w->proc->removeListener(w); w->editor.reset(); w->proc.reset(); delete w; e->object = nullptr; return 0; }
    case SetProgram: return 0;
    case GetProgram: return 0;
    case SetProgramName: return 0;
    case GetProgramName: case GetProgramNameIndexed: copyStr(ptr, w->proc->presetName[0].toRawUTF8(), 24); return 1;
    case GetParamLabel: copyStr(ptr, "", 24); return 0;
    case GetParamDisplay: if (auto* p = w->param(idx)) copyStr(ptr, p->getText(p->getValue(), 24).toRawUTF8(), 24); return 0;
    case GetParamName: if (auto* p = w->param(idx)) copyStr(ptr, p->getName(24).toRawUTF8(), 24); return 0;
    case SetSampleRate: w->sr = opt; return 0;
    case SetBlockSize: w->bs = (int) val; return 0;
    case MainsChanged:
        if (val) { w->proc->setPlayHead(&w->hp); w->proc->setPlayConfigDetails(0, 2, w->sr, w->bs); w->proc->prepareToPlay(w->sr, w->bs); }
        else w->proc->releaseResources();
        return 0;
    case EditGetRect: {
        rg::dbg("EditGetRect called");
        int pct = juce::jlimit(75, 200, w->proc->uiScalePercent); w->rect = { 0, 0, (int16_t) (600 * pct / 100), (int16_t) (1000 * pct / 100) };
        if (ptr) *(VRect**) ptr = &w->rect; return 1; }
    case EditOpen: {
        rg::dbg("EditOpen parentHWND=" + juce::String::toHexString((juce::int64) (juce::pointer_sized_int) ptr) + " isMsgThread=" + juce::String((int) juce::MessageManager::existsAndIsCurrentThread()) + " scale=" + juce::String(juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() ? juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->scale : 0.0));
        if (!w->editor) w->editor.reset(w->proc->createEditor());
        w->editor->addComponentListener(w); w->lastW = w->editor->getWidth(); w->lastH = w->editor->getHeight();
        w->editor->setTopLeftPosition(0, 0); w->editor->addToDesktop(0, ptr); w->editor->setVisible(true);
        if (auto* re = dynamic_cast<RaygunEditor*>(w->editor.get())) re->forceSoftwareRenderer();
        rg::dbg("EditOpen done: editor=" + juce::String(w->editor->getWidth()) + "x" + juce::String(w->editor->getHeight()) + " peer=" + juce::String((int) (w->editor->getPeer() != nullptr)) + " visible=" + juce::String((int) w->editor->isVisible()) + " onDesktop=" + juce::String((int) w->editor->isOnDesktop()));
        w->editor->repaint(); return 1; }
    case EditClose: if (w->editor) { w->editor->removeComponentListener(w); w->editor->removeFromDesktop(); w->editor.reset(); } return 0;
    case EditIdle: return 0;
    case Identify: return ('N' << 24) | ('v' << 16) | ('E' << 8) | 'f';
    case GetChunk: { w->chunk.reset(); w->proc->getStateInformation(w->chunk); if (ptr) *(void**) ptr = w->chunk.getData(); return (intptr_t) w->chunk.getSize(); }
    case SetChunk: if (ptr && val > 0) w->proc->setStateInformation(ptr, (int) val); return 0;
    case ProcessEvents: {
        auto* ev = (VEvents*) ptr; if (!ev) return 0;
        for (int i = 0; i < ev->numEvents; ++i) { auto* me = (VMidiEvent*) ev->events[i];
            if (me->type != kMidiType) continue;
            w->midi.addEvent(juce::MidiMessage((juce::uint8) me->midiData[0], (juce::uint8) me->midiData[1], (juce::uint8) me->midiData[2]), juce::jmax(0, me->deltaFrames)); }
        return 1; }
    case CanBeAutomated: return 1;
    case GetOutputProperties: if (ptr) { std::memset(ptr, 0, 128); copyStr(ptr, "Out", 63); ((int32_t*) ptr)[16] = 1 | 2; } return 1;   // active + stereo
    case GetPlugCategory: return 2;   // synth
    case GetEffectName: case GetProductString: copyStr(ptr, "RAYGUN", 63); return 1;
    case GetVendorString: copyStr(ptr, "Trill Tron", 63); return 1;
    case GetVendorVersion: return 1000;
    case CanDo: { juce::String s((const char*) ptr); return (s == "receiveVstEvents" || s == "receiveVstMidiEvent" || s == "sizeWindow") ? 1 : 0; }
    case GetTailSize: return 0;
    case GetVstVersion: return 2400;
    case StartProcess: case StopProcess: case SetProcessPrecision: return 0;
    default: return 0;
    }
}
} // namespace


VST_EXPORT VEffect* VSTPluginMain(HostCb cb) {
    rg::dbg("VSTPluginMain entered, hostCb=" + juce::String(cb ? "yes" : "NULL"));
    intptr_t hv = cb ? cb(nullptr, MVersion, 0, 0, nullptr, 0) : 0;
    rg::dbg("host audioMasterVersion -> " + juce::String((juce::int64) hv));
    if (!cb || hv == 0) { rg::dbg("REFUSING: not a VST2 host"); return nullptr; }   // not a VST2 host
    static bool inited = false; if (!inited) { juce::initialiseJuce_GUI(); inited = true; }   // never shut down: host unloads the DLL at exit
    auto* w = new Wrapper(); w->host = cb; w->proc = std::make_unique<RaygunProcessor>(); w->proc->addListener(w);
    auto& f = w->fx; f.magic = kMagic; f.dispatcher = dispatch; f.process = processAccumulating; f.setParameter = setParameter; f.getParameter = getParameter;
    f.numPrograms = 1; f.numParams = w->proc->getParameters().size(); f.numInputs = 0; f.numOutputs = 2; f.flags = HasEditor | CanReplacing | ProgramChunks | IsSynth;
    f.uniqueID = ('R' << 24) | ('G' << 16) | ('u' << 8) | 'n'; f.version = 1000; f.processReplacing = processReplacing; f.object = w;
    return &f;
}
#if defined(_WIN32) && defined(_MSC_VER)
 #pragma comment(linker, "/EXPORT:main=VSTPluginMain")
#endif
