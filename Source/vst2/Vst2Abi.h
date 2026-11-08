// Minimal VST 2.4 binary interface (VEffect + host callback), written from the publicly documented ABI layout
// for interoperability. Contains no Steinberg SDK code. "VST" is a trademark of Steinberg Media Technologies GmbH.
#pragma once
#include <cstdint>

namespace vst2 {
struct VEffect;
using HostCb   = intptr_t (*)(VEffect*, int32_t opcode, int32_t index, intptr_t value, void* ptr, float opt);
using Dispatch = intptr_t (*)(VEffect*, int32_t opcode, int32_t index, intptr_t value, void* ptr, float opt);
using ProcFn   = void (*)(VEffect*, float** in, float** out, int32_t frames);
using SetParam = void (*)(VEffect*, int32_t index, float value);
using GetParam = float (*)(VEffect*, int32_t index);

struct VEffect {
    int32_t magic;                       // 'VstP'
    Dispatch dispatcher; ProcFn process; SetParam setParameter; GetParam getParameter;
    int32_t numPrograms, numParams, numInputs, numOutputs, flags;
    intptr_t resvd1, resvd2;
    int32_t initialDelay, realQualities, offQualities; float ioRatio;
    void* object; void* user;
    int32_t uniqueID, version;
    ProcFn processReplacing; void* processReplacingDouble;
    char future[56];
};
static_assert(sizeof(void*) != 8 || sizeof(VEffect) == 192, "VEffect layout mismatch (x64)");

struct VRect { int16_t top, left, bottom, right; };
struct VEvent { int32_t type, byteSize, deltaFrames, flags; char data[16]; };
struct VMidiEvent { int32_t type, byteSize, deltaFrames, flags, noteLength, noteOffset; char midiData[4]; char detune, noteOffVelocity, reserved1, reserved2; };
struct VEvents { int32_t numEvents; intptr_t reserved; VEvent* events[2]; };   // events[] is really numEvents long
struct VTimeInfo { double samplePos, sampleRate, nanoSeconds, ppqPos, tempo, barStartPos, cycleStartPos, cycleEndPos;
                  int32_t timeSigNumerator, timeSigDenominator, smpteOffset, smpteFrameRate, samplesToNextClock, flags; };

constexpr int32_t kMagic = 0x56737450;   // 'VstP'
enum Flags { HasEditor = 1, CanReplacing = 1 << 4, ProgramChunks = 1 << 5, IsSynth = 1 << 8 };
enum Op {  // host -> plugin
    Open = 0, Close = 1, SetProgram = 2, GetProgram = 3, SetProgramName = 4, GetProgramName = 5, GetParamLabel = 6, GetParamDisplay = 7, GetParamName = 8,
    SetSampleRate = 10, SetBlockSize = 11, MainsChanged = 12, EditGetRect = 13, EditOpen = 14, EditClose = 15, EditIdle = 19, Identify = 22,
    GetChunk = 23, SetChunk = 24, ProcessEvents = 25, CanBeAutomated = 26, GetProgramNameIndexed = 29, GetOutputProperties = 34, GetPlugCategory = 35,
    GetEffectName = 45, GetVendorString = 47, GetProductString = 48, GetVendorVersion = 49, CanDo = 51, GetTailSize = 52, GetVstVersion = 58,
    StartProcess = 71, StopProcess = 72, SetProcessPrecision = 77 };
enum HostOp { MAutomate = 0, MVersion = 1, MGetTime = 7, MSizeWindow = 15 };
constexpr int32_t kTempoValid = 1 << 10, kMidiType = 1;
}
