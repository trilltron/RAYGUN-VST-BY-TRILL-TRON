#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <set>
#include <array>
#include "engine/Synth.h"
#include "PresetManager.h"

class RaygunProcessor : public juce::AudioProcessor {
public:
    RaygunProcessor();
    ~RaygunProcessor() override = default;

    void prepareToPlay(double sr, int block) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "RAYGUN"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // ---- preset API (message thread)
    void loadPreset(const rg::PresetInfo& p, int targetLayer /*0=A 1=B*/);
    void resetLayer(int layer);
    void initPatch();
    bool saveUserPreset(const juce::String& name, int layer);

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    rg::PresetManager presets;
    juce::String presetName[2] { "Init", "Init" };
    int editLayer = 0;           // which layer the pages edit (0=A,1=B)
    int uiScalePercent = 100;
    std::atomic<float> meterL { 0.f }, meterR { 0.f }, cpuLoad { 0.f };
    std::atomic<int> voiceCount { 0 };
    std::array<std::atomic<bool>, 128> noteHeld {};   // host/UI notes currently down (for the on-screen keyboard)
    std::atomic<bool> panicFlag { false };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static juce::String layerPrefix(int l) { return l == 0 ? "a_" : "b_"; }

private:
    rg::Synth synth;
    std::atomic<float>* layerRaw[2][rg::NLAYER] {};
    std::atomic<float>* globalRaw[rg::NGLOBAL] {};
    juce::RangedAudioParameter* layerParams[2][rg::NLAYER] {};
    juce::RangedAudioParameter* globalParams[rg::NGLOBAL] {};
    bool sustain[16] {}; std::set<int> sustained[16];
    double sampleRate = 44100; double cpuSmooth = 0;
    void handleMidi(const juce::MidiMessage&);
    void syncParams();
    void setParamReal(juce::RangedAudioParameter* p, float real);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RaygunProcessor)
};
