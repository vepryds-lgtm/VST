#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include "DrumEngine.h"

namespace drums
{
namespace pid
{
inline const char* sound   = "sound";
inline const char* mode    = "mode";       // 0 = single sound, 1 = kit
inline const char* kit     = "kit";
inline const char* keyMode = "keymode";
inline const char* volume  = "volume";
inline const char* tune    = "tune";
inline const char* length  = "length";
inline const char* tone    = "tone";
inline const char* space   = "space";
} // namespace pid

class DrumsProcessor : public juce::AudioProcessor
{
public:
    DrumsProcessor();
    ~DrumsProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AuraDrums"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    // every sound is also a host "program", so FL Studio's own preset arrows step through the library
    int getNumPrograms() override { return numSounds(); }
    int getCurrentProgram() override { return selectedSound(); }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ---- used by the editor
    int  selectedSound() const;
    void selectSound(int index);              // also switches to Single mode
    void setKitMode(bool kit);
    bool isKitMode() const;
    int  selectedKit() const;
    void selectKit(int k);
    void auditionSound(int index);            // plays a sound at its own pitch (thread safe)
    const Bank& bank() const { return *bankPtr; }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;   // on-screen keyboard -> engine
    std::atomic<int> voicesInUse { 0 };
    std::atomic<float> peakLevel { 0.f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateSettings();
    void updateTone();
    void handleMidi(const juce::MidiMessage& m);

    std::shared_ptr<const Bank> bankPtr;
    Engine engine;
    double sampleRateHz = 44100.0;

    std::atomic<float>* pSound = nullptr; std::atomic<float>* pMode = nullptr; std::atomic<float>* pKit = nullptr;
    std::atomic<float>* pKeyMode = nullptr; std::atomic<float>* pVolume = nullptr; std::atomic<float>* pTune = nullptr;
    std::atomic<float>* pLength = nullptr; std::atomic<float>* pTone = nullptr; std::atomic<float>* pSpace = nullptr;

    // editor -> audio thread audition requests (lock free, single producer / single consumer is enough: only the UI thread writes)
    std::atomic<int> pendingAudition { -1 };

    // tone (tilt EQ) and space (reverb send)
    juce::dsp::IIR::Filter<float> lowShelf[2], highShelf[2];
    float lastTone = 9.f;
    juce::Reverb reverb;
    juce::dsp::IIR::Filter<float> sendHp[2];
    juce::AudioBuffer<float> scratch;
    float smoothVol = -1.f;
    bool reverbWasOn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumsProcessor)
};
} // namespace drums
