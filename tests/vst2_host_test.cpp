// Minimal VST2 host: loads the wrapper library and drives it through the 2.4 opcode set like a DAW would.
#include "../Source/vst2/Vst2Abi.h"
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
using namespace vst2;
static int fails = 0; static int automates = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); ++fails; } else printf("ok   : %s\n", msg); } while (0)
static intptr_t host(VEffect*, int32_t op, int32_t idx, intptr_t, void*, float) { if (op == MVersion) return 2400; if (op == MAutomate) ++automates; return 0; }
int main(int, char** argv) {
    void* h = dlopen(argv[1], RTLD_NOW); if (!h) { printf("dlopen: %s\n", dlerror()); return 2; }
    auto entry = (VEffect * (*)(HostCb)) dlsym(h, "VSTPluginMain"); CHECK(entry != nullptr, "exports VSTPluginMain");
    CHECK(entry(nullptr) == nullptr, "rejects a null host callback");
    VEffect* e = entry(host); CHECK(e && e->magic == kMagic, "returns AEffect with 'VstP' magic");
    CHECK((e->flags & IsSynth) && (e->flags & HasEditor) && (e->flags & CanReplacing) && e->numOutputs == 2 && e->numInputs == 0, "flags: synth, editor, replacing, 2 outs");
    printf("     params=%d\n", e->numParams); CHECK(e->numParams >= 190, "exposes all automatable parameters");
    char buf[256] = {}; e->dispatcher(e, GetEffectName, 0, 0, buf, 0); CHECK(!strcmp(buf, "RAYGUN"), "effect name RAYGUN");
    e->dispatcher(e, GetVendorString, 0, 0, buf, 0); CHECK(!strcmp(buf, "Trill Tron"), "vendor Trill Tron");
    CHECK(e->dispatcher(e, GetPlugCategory, 0, 0, nullptr, 0) == 2, "category = synth");
    CHECK(e->dispatcher(e, GetVstVersion, 0, 0, nullptr, 0) == 2400, "VST 2.4");
    CHECK(e->dispatcher(e, CanDo, 0, 0, (void*) "receiveVstMidiEvent", 0) == 1, "canDo receiveVstMidiEvent");
    e->dispatcher(e, GetParamName, 5, 0, buf, 0); printf("     param[5] name=\"%s\"\n", buf); CHECK(strlen(buf) > 0, "param names readable");
    VRect* r = nullptr; e->dispatcher(e, EditGetRect, 0, 0, &r, 0); CHECK(r && r->right == 1000 && r->bottom == 600, "editor rect 1000x600");
    CHECK(e->numPrograms >= 500, "exposes all presets as VST programs");
    char pn[256] = {}; e->dispatcher(e, GetProgramNameIndexed, 0, 0, pn, 0); CHECK(!strcmp(pn, "Init"), "program 0 is Init");
    e->dispatcher(e, GetProgramNameIndexed, 1, 0, pn, 0); printf("     program 1 = \"%s\"\n", pn); CHECK(strlen(pn) > 0 && strcmp(pn, "Init"), "program 1 has a preset name");
    CHECK(e->dispatcher(e, GetProgram, 0, 0, nullptr, 0) == 0, "starts on program 0");
    e->dispatcher(e, SetProgram, 0, 7, nullptr, 0); CHECK(e->dispatcher(e, GetProgram, 0, 0, nullptr, 0) == 7, "setProgram(7) loads preset 7");
    char cur[256] = {}; e->dispatcher(e, GetProgramName, 0, 0, cur, 0); e->dispatcher(e, GetProgramNameIndexed, 7, 0, pn, 0); CHECK(!strcmp(cur, pn), "current program name matches");
    e->dispatcher(e, SetProgram, 0, 0, nullptr, 0); CHECK(e->dispatcher(e, GetProgram, 0, 0, nullptr, 0) == 0, "setProgram(0) returns to Init");
    // set up + play a note
    e->dispatcher(e, Open, 0, 0, nullptr, 0); e->dispatcher(e, SetSampleRate, 0, 0, nullptr, 48000.f); e->dispatcher(e, SetBlockSize, 0, 256, nullptr, 0); e->dispatcher(e, MainsChanged, 0, 1, nullptr, 0);
    // set parameters via host: mode = A (index of "mode" found by name scan)
    int modeIdx = -1; for (int i = 0; i < e->numParams; ++i) { e->dispatcher(e, GetParamName, i, 0, buf, 0); if (!strcmp(buf, "Layer Mode")) modeIdx = i; }
    CHECK(modeIdx >= 0, "found 'Layer Mode' parameter"); e->setParameter(e, modeIdx, 0.f); CHECK(std::fabs(e->getParameter(e, modeIdx)) < 1e-4, "setParameter/getParameter round-trip");
    int before = automates; e->setParameter(e, modeIdx, 0.25f); CHECK(automates == before, "host-set params are not echoed back as automation");
    e->setParameter(e, modeIdx, 0.f);
    struct { VEvents h; VEvent* more; } evs; VMidiEvent on {}; on.type = kMidiType; on.byteSize = sizeof(VMidiEvent); on.deltaFrames = 10; on.midiData[0] = (char) 0x90; on.midiData[1] = 60; on.midiData[2] = 100;
    evs.h.numEvents = 1; evs.h.reserved = 0; evs.h.events[0] = (VEvent*) &on;
    CHECK(e->dispatcher(e, ProcessEvents, 0, 0, &evs, 0) == 1, "processEvents accepts MIDI");
    std::vector<float> L(256), R(256); float* outs[2] = { L.data(), R.data() }; float pk = 0; bool finite = true;
    for (int b = 0; b < 40; ++b) { e->processReplacing(e, nullptr, outs, 256); for (int i = 0; i < 256; ++i) { pk = std::fmax(pk, std::fabs(L[i])); if (!std::isfinite(L[i]) || !std::isfinite(R[i])) finite = false; } }
    printf("     peak=%.3f\n", pk); CHECK(finite && pk > 0.01f, "note-on produces finite audio");
    // state chunk round trip
    void* data = nullptr; intptr_t sz = e->dispatcher(e, GetChunk, 0, 0, &data, 0); CHECK(sz > 100 && data, "getChunk returns state");
    std::vector<char> copy((char*) data, (char*) data + sz); e->setParameter(e, modeIdx, 1.f);
    e->dispatcher(e, SetChunk, 0, (intptr_t) copy.size(), copy.data(), 0); CHECK(std::fabs(e->getParameter(e, modeIdx)) < 1e-3, "setChunk restores state");
    // note off + decay
    on.midiData[0] = (char) 0x80; on.deltaFrames = 0; e->dispatcher(e, ProcessEvents, 0, 0, &evs, 0);
    e->dispatcher(e, MainsChanged, 0, 0, nullptr, 0); e->dispatcher(e, Close, 0, 0, nullptr, 0); CHECK(true, "close without crash");
    printf(fails ? "\n%d FAILED\n" : "\nALL PASSED\n", fails); return fails ? 1 : 0; }
