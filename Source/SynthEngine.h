#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <vector>
#include <memory>
#include <cstdint>
#include "Params.h"
#include "Tables.h"

namespace aura
{
constexpr int kMaxVoices = 16;
constexpr int kCtrlRate  = 8;   // samples per control tick (filter / modulation update)
constexpr int kMaxUni    = 7;

struct EnvCoefs
{
    float aCoef = 0, aBase = 0, dCoef = 0, dBase = 0, rCoef = 0, rBase = 0, sus = 0;
};
void computeEnvCoefs(EnvCoefs& c, float a, float d, float s, float r, double sr);

struct Env
{
    enum Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Idle;
    float v = 0.f;

    void gateOn() noexcept  { stage = Attack; }
    void gateOff() noexcept { if (stage != Idle) stage = Release; }
    bool active() const noexcept { return stage != Idle; }

    float tick(const EnvCoefs& c) noexcept
    {
        switch (stage)
        {
            case Attack:  v = c.aBase + v * c.aCoef; if (v >= 1.f) { v = 1.f; stage = Decay; } break;
            case Decay:   v = c.dBase + v * c.dCoef; if (v <= c.sus) { v = c.sus; stage = Sustain; } break;
            case Sustain: v = c.sus; break;
            case Release: v = c.rBase + v * c.rCoef; if (v <= 0.00005f) { v = 0.f; stage = Idle; } break;
            case Idle:    v = 0.f; break;
        }
        return v;
    }
};

struct SVF { float ic1 = 0.f, ic2 = 0.f; };

struct Voice
{
    bool active = false;
    bool gate = false;       // key physically held
    bool sustained = false;  // key released but held by the sustain pedal
    int note = 60;
    float vel = 0.f;
    std::uint64_t age = 0;
    float noteCur = 60.f;    // glided pitch (in semitones / MIDI note units)

    Env ampEnv, fltEnv, layEnv;                 // layEnv = envelope of the optional "Layer B"
    float ph1[kMaxUni] {}, ph2[kMaxUni] {}, mph[kMaxUni] {};
    float fmPh = 0.f, subPh = 0.f;
    float lfoPh[3] {}, lfoSH[3] {}, lfoVal[3] {};
    SVF f1[2], f2[2], fm[2][3], ly[2];
    float mvPh[3] {}, mvRate[3] {};            // slow per-voice "movement" modulators
    float drift = 0.f, driftT = 0.f; int driftCnt = 0;   // analog pitch drift
    float noiseLP = 0.f;
    std::uint32_t rng = 1u;
};

class SynthEngine
{
public:
    SynthEngine();

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    // `values` = kNumParams real (de-normalised) parameter values, read once per block.
    void process(juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const float* values, double bpm);

    void allNotesOff(bool immediate = false);

    // Visual feedback for the GUI (written by the audio thread, read by the message thread)
    std::atomic<float> vizLfo[3] { { 0.f }, { 0.f }, { 0.f } };
    std::atomic<float> vizAmpEnv { 0.f }, vizFiltEnv { 0.f };
    std::atomic<int>   vizVoices { 0 };

private:
    struct OscCtl
    {
        int n = 1, wave = 0, mip = 0;
        float level = 0.f, pw = 0.5f, pwGain = 1.f, fr = 0.f;
        const float* t0 = nullptr;
        const float* t1 = nullptr;
        float inc[kMaxUni] {}, minc[kMaxUni] {}, gL[kMaxUni] {}, gR[kMaxUni] {};
    };

    struct SvfCoef { float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0; };

    void handleMidi(const juce::MidiMessage& m);
    void noteOn(int note, float vel);
    void noteOff(int note);
    void voiceNoteOn(int note, float vel);
    void voiceNoteOff(int note);
    void releaseVoice(Voice& v);
    void startVoice(Voice& v, int note, float vel, float glideFrom, bool retrigger);
    Voice* allocateVoice(int note);
    void sustainPedal(bool down);

    void updateBlockParams(double bpm);
    void renderSegment(float* L, float* R, int start, int count);
    void renderVoice(Voice& v, float* L, float* R, int count);
    void setupOsc(OscCtl& c, int which, float noteSemis, const float* d, float pitchExtra, bool fmBoost);
    void fireArpStep();
    double arpStepSamples() const;

    float P(int idx) const noexcept { return p[idx]; }

    double sr = 44100.0;
    std::shared_ptr<const WaveTables> tables;
    float p[kNumParams] {};
    float prevArpOn = 0.f;
    EnvCoefs ampC, fltC, layC;
    double bpmNow = 120.0;

    std::array<Voice, kMaxVoices> voices;
    std::uint64_t ageCounter = 0;
    int lastNote = -1;

    // mono / legato note stack
    std::vector<std::pair<int, float>> monoStack;

    // controllers
    bool pedalDown = false;
    float bendTarget = 0.f, bendCur = 0.f;     // in semitones
    float wheelTarget = 0.f, wheelCur = 0.f;   // 0..1
    int bendRaw = 8192;

    // arpeggiator
    std::vector<int> arpNotes;          // sorted, unique
    std::vector<float> arpVel;          // parallel to arpNotes
    std::vector<int> arpPedalHeld;      // notes released while the pedal is down
    double arpNext = 0.0, arpGateT = 0.0;
    bool arpGateActive = false;
    int arpPlaying = -1, arpStep = 0, arpDir = 1;
    juce::Random arpRand { 1234 };

    std::vector<std::pair<int, juce::MidiMessage>> events;
    float idleLfoPh[3] {};
};
} // namespace aura
