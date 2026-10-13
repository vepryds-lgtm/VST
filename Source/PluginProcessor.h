#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "Params.h"
#include "SynthEngine.h"
#include "Effects.h"
#include "PresetManager.h"

class AuraProcessor : public juce::AudioProcessor
{
public:
    AuraProcessor();
    ~AuraProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AuraSynth"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    // The factory presets are exposed as host "programs" so FL Studio's own preset browser works too.
    int getNumPrograms() override { return presets.numFactory(); }
    int getCurrentProgram() override { return juce::jmax(0, presets.currentIndex()); }
    void setCurrentProgram(int index) override { presets.load(index); }
    const juce::String getProgramName(int index) override { return presets.preset(index).name; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    aura::PresetManager presets;
    aura::SynthEngine engine;
    aura::FxChain fx;
    juce::MidiKeyboardState keyboardState;   // on-screen keyboard -> synth

private:
    std::array<std::atomic<float>*, aura::kNumParams> raw {};
    float values[aura::kNumParams] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuraProcessor)
};
