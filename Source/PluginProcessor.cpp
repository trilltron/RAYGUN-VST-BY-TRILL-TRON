#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>
using namespace juce;

static String fmtFor(const rg::PDef& d, float v) {
    String id(d.id);
    if (id.contains("f_cut")) return v >= 1000 ? String(v / 1000.f, 2) + " kHz" : String((int) v) + " Hz";
    if (id.endsWith("_atk") || id.endsWith("_dec") || id.endsWith("_rel") || id == "glide" || id == "delay_time")
        return v < 1.f ? String((int) (v * 1000)) + " ms" : String(v, 2) + " s";
    if (id.contains("l1_rate") || id.contains("l2_rate")) return String(v, 2) + " Hz";
    if (id == "vol" || id == "master") return String(v, 1) + " dB";
    if (id.contains("fine")) return String(v, 0) + " ct";
    if (id == "pan") return std::abs(v) < 0.01f ? String("C") : String(v < 0 ? "L" : "R") + String((int) (std::abs(v) * 100));
    if (d.kind == rg::KI || id.endsWith("_semi")) return String((int) v);
    return String((int) std::round(v * 100)) + "%";
}

AudioProcessorValueTreeState::ParameterLayout RaygunProcessor::createLayout() {
    AudioProcessorValueTreeState::ParameterLayout L;
    auto make = [&](const String& pid, const rg::PDef& d) -> std::unique_ptr<RangedAudioParameter> {
        String nm = d.name;
        if (d.kind == rg::KC) { StringArray ch; ch.addTokens(d.choices, "|", ""); return std::make_unique<AudioParameterChoice>(ParameterID(pid, 1), nm, ch, (int) d.def); }
        if (d.kind == rg::KB) return std::make_unique<AudioParameterBool>(ParameterID(pid, 1), nm, d.def > 0.5f);
        if (d.kind == rg::KI) return std::make_unique<AudioParameterInt>(ParameterID(pid, 1), nm, (int) d.mn, (int) d.mx, (int) d.def);
        NormalisableRange<float> r(d.mn, d.mx);
        if (d.skew == rg::SLog) r = NormalisableRange<float>(d.mn, d.mx,
            [](float s, float e, float v) { return s * std::pow(e / s, v); }, [](float s, float e, float v) { return std::log(v / s) / std::log(e / s); }, [](float, float, float v) { return v; });
        else if (d.skew == rg::SSq) r.setSkewForCentre(d.mn + (d.mx - d.mn) * 0.25f);
        rg::PDef dd = d;
        return std::make_unique<AudioParameterFloat>(ParameterID(pid, 1), nm, r, d.def,
            AudioParameterFloatAttributes().withStringFromValueFunction([dd](float v, int) { return fmtFor(dd, v); }));
    };
    for (int l = 0; l < 2; ++l) {
        auto grp = std::make_unique<AudioProcessorParameterGroup>(l ? "layerB" : "layerA", l ? "Layer B" : "Layer A", "|");
        for (int i = 0; i < rg::NLAYER; ++i) grp->addChild(make(layerPrefix(l) + rg::LAYER_DEFS[i].id, rg::LAYER_DEFS[i]));
        L.add(std::move(grp));
    }
    auto g = std::make_unique<AudioProcessorParameterGroup>("global", "Global", "|");
    for (int i = 0; i < rg::NGLOBAL; ++i) g->addChild(make(rg::GLOBAL_DEFS[i].id, rg::GLOBAL_DEFS[i]));
    L.add(std::move(g));
    return L;
}

RaygunProcessor::RaygunProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "RAYGUN", createLayout()) {
    for (int l = 0; l < 2; ++l) for (int i = 0; i < rg::NLAYER; ++i) {
        String id = layerPrefix(l) + rg::LAYER_DEFS[i].id;
        layerRaw[l][i] = apvts.getRawParameterValue(id); layerParams[l][i] = apvts.getParameter(id); }
    for (int i = 0; i < rg::NGLOBAL; ++i) { globalRaw[i] = apvts.getRawParameterValue(rg::GLOBAL_DEFS[i].id); globalParams[i] = apvts.getParameter(rg::GLOBAL_DEFS[i].id); }
    for (int t = 0; t < rg::NTABLES; ++t) {   // stem-derived wavetables
        int size = 0; auto* d = BinaryData::getNamedResource((String(rg::TABLE_FILES[t]) + "_rgwt").toRawUTF8(), size);
        if (d && size >= rg::WT_FRAMES * rg::WT_N * 2) rg::Wavetables::get().setRawInt16(t, (const int16_t*) d, (size_t) size / 2);
    }
    rg::Wavetables::get().build();
    presets.loadAll(BinaryData::factory_json, (size_t) BinaryData::factory_jsonSize);
    synth.prepare(44100.f, 512);
}

void RaygunProcessor::prepareToPlay(double sr, int block) {
    sampleRate = sr; synth.prepare((float) sr, std::max(64, block));
    for (int c = 0; c < 16; ++c) { sustain[c] = false; sustained[c].clear(); }
}

void RaygunProcessor::syncParams() {
    for (int l = 0; l < 2; ++l) { auto& layer = l ? synth.B : synth.A; for (int i = 0; i < rg::NLAYER; ++i) layer.p[i] = layerRaw[l][i]->load(); }
    for (int i = 0; i < rg::NGLOBAL; ++i) synth.g[i] = globalRaw[i]->load();
}

void RaygunProcessor::handleMidi(const MidiMessage& m) {
    int ch = m.getChannel() - 1;
    if (ch < 0 || ch > 15) ch = 0;
    if (m.isNoteOn()) { synth.noteOn(ch, m.getNoteNumber(), m.getVelocity()); noteHeld[(size_t) m.getNoteNumber()] = true; }
    else if (m.isNoteOff()) { noteHeld[(size_t) m.getNoteNumber()] = false; if (sustain[ch]) sustained[ch].insert(m.getNoteNumber()); else synth.noteOff(ch, m.getNoteNumber()); }
    else if (m.isPitchWheel()) synth.setPitchBend((m.getPitchWheelValue() - 8192) / 8192.f);
    else if (m.isChannelPressure()) synth.setAftertouch(m.getChannelPressureValue() / 127.f);
    else if (m.isController()) {
        int cc = m.getControllerNumber(), v = m.getControllerValue();
        if (cc == 1) synth.setModWheel(v / 127.f);
        else if (cc == 64) { bool d = v >= 64; if (!d && sustain[ch]) { for (int n : sustained[ch]) synth.noteOff(ch, n); sustained[ch].clear(); } sustain[ch] = d; }
        else if (cc == 120) synth.panic();
        else if (cc == 123) synth.allNotesOff();
    }
}

void RaygunProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midi) {
    ScopedNoDenormals nd;
    auto t0 = Time::getHighResolutionTicks();
    const int n = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer(midi, 0, n, true);
    if (panicFlag.exchange(false)) { synth.panic(); for (auto& h : noteHeld) h = false; }
    syncParams();
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto bpm = pos->getBpm()) synth.setBpm((float) *bpm);
    float* L = buffer.getWritePointer(0); float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    std::vector<float> tmp; if (!R) tmp.assign((size_t) n, 0.f);
    int pos = 0;
    auto render = [&](int upto) { int c = upto - pos; if (c <= 0) return; synth.process(L + pos, R ? R + pos : tmp.data() + pos, c); pos = upto; };
    for (const auto meta : midi) { render(std::min(meta.samplePosition, n)); handleMidi(meta.getMessage()); }
    render(n);
    float pl = buffer.getMagnitude(0, 0, n), pr = R ? buffer.getMagnitude(1, 0, n) : pl;
    meterL = std::max(pl, meterL.load() * 0.85f); meterR = std::max(pr, meterR.load() * 0.85f);
    voiceCount = synth.A.activeVoices() + synth.B.activeVoices();
    midi.clear();
    double used = Time::highResolutionTicksToSeconds(Time::getHighResolutionTicks() - t0), avail = n / sampleRate;
    cpuSmooth += 0.1 * (used / std::max(1e-9, avail) - cpuSmooth); cpuLoad = (float) cpuSmooth;
}

void RaygunProcessor::setParamReal(RangedAudioParameter* p, float real) { p->setValueNotifyingHost(p->convertTo0to1(real)); }

void RaygunProcessor::resetLayer(int l) { for (int i = 0; i < rg::NLAYER; ++i) setParamReal(layerParams[l][i], rg::LAYER_DEFS[i].def); }

void RaygunProcessor::initPatch() {
    resetLayer(0); resetLayer(1); setParamReal(globalParams[rg::G_MODE], 0);
    for (int i = 0; i < 8; ++i) setParamReal(globalParams[rg::G_MACRO1 + i], 0.5f);
    presetName[0] = presetName[1] = "Init";
}

void RaygunProcessor::loadPreset(const rg::PresetInfo& p, int target) {
    auto applyLayer = [&](int l, const var& obj) {
        resetLayer(l);
        if (auto* o = obj.getDynamicObject()) for (auto& kv : o->getProperties()) {
            String id = kv.name.toString();
            for (int i = 0; i < rg::NLAYER; ++i) if (id == rg::LAYER_DEFS[i].id) { setParamReal(layerParams[l][i], (float) (double) kv.value); break; } } };
    if (p.isMulti()) {
        applyLayer(0, p.data.getProperty("a", var())); applyLayer(1, p.data.getProperty("b", var()));
        presetName[0] = presetName[1] = p.name;
        if (auto* g = p.data.getProperty("global", var()).getDynamicObject()) for (auto& kv : g->getProperties()) {
            String id = kv.name.toString(); for (int i = 0; i < rg::NGLOBAL; ++i) if (id == rg::GLOBAL_DEFS[i].id) setParamReal(globalParams[i], (float) (double) kv.value); }
    } else { applyLayer(target, p.data.getProperty("a", var())); presetName[target] = p.name; }
    if (auto* mac = p.data.getProperty("macros", var()).getArray()) { for (int i = 0; i < 8 && i < mac->size(); ++i) setParamReal(globalParams[rg::G_MACRO1 + i], (float) (double) (*mac)[i]); }
    else for (int i = 0; i < 8; ++i) setParamReal(globalParams[rg::G_MACRO1 + i], 0.5f);
}

bool RaygunProcessor::saveUserPreset(const String& name, int l) {
    auto* o = new DynamicObject(); auto* a = new DynamicObject();
    for (int i = 0; i < rg::NLAYER; ++i) { float v = layerRaw[l][i]->load(); if (std::abs(v - rg::LAYER_DEFS[i].def) > 1e-5f) a->setProperty(rg::LAYER_DEFS[i].id, v); }
    o->setProperty("name", name); o->setProperty("author", "User"); o->setProperty("type", "FX"); o->setProperty("a", var(a));
    Array<var> mac; for (int i = 0; i < 8; ++i) mac.add(globalRaw[rg::G_MACRO1 + i]->load()); o->setProperty("macros", mac);
    var v(o);
    auto f = rg::PresetManager::userDir().getChildFile("User Presets").getChildFile(File::createLegalFileName(name) + ".rgp");
    bool ok = f.replaceWithText(JSON::toString(v));
    if (ok) presets.add(v, false, f);
    return ok;
}

void RaygunProcessor::getStateInformation(MemoryBlock& dest) {
    auto state = apvts.copyState(); state.setProperty("presetA", presetName[0], nullptr); state.setProperty("presetB", presetName[1], nullptr);
    state.setProperty("editLayer", editLayer, nullptr); state.setProperty("uiScale", uiScalePercent, nullptr);
    std::unique_ptr<XmlElement> xml(state.createXml()); copyXmlToBinary(*xml, dest);
}
void RaygunProcessor::setStateInformation(const void* data, int size) {
    std::unique_ptr<XmlElement> xml(getXmlFromBinary(data, size));
    if (xml && xml->hasTagName(apvts.state.getType())) {
        auto st = ValueTree::fromXml(*xml); apvts.replaceState(st);
        presetName[0] = st.getProperty("presetA", "Init").toString(); presetName[1] = st.getProperty("presetB", "Init").toString();
        editLayer = (int) st.getProperty("editLayer", 0); uiScalePercent = (int) st.getProperty("uiScale", 100);
    }
}

AudioProcessorEditor* RaygunProcessor::createEditor() { return new RaygunEditor(*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RaygunProcessor(); }
