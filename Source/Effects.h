#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include "Params.h"

namespace aura
{
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    float process(float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void lowShelf(double sr, double f, double gainDb);
    void highShelf(double sr, double f, double gainDb);
    void peak(double sr, double f, double q, double gainDb);
    void lowpass(double sr, double f, double q);
    void highpass(double sr, double f, double q);
    void set(double nb0, double nb1, double nb2, double na0, double na1, double na2);
};

struct DelayLine
{
    std::vector<float> buf;
    int w = 0, mask = 0;
    void init(int minSize);
    void clear() { std::fill(buf.begin(), buf.end(), 0.f); w = 0; }
    void push(float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float read(float delaySamples) const noexcept; // linear interpolation
};

// Stereo feedback-delay-network reverb: 8 modulated delay lines + Householder feedback matrix, high-pass in the
// input and in the loop (keeps the lows clean), damping low-pass in the loop (soft, non-metallic tail), pre-delay.
class FdnReverb
{
public:
    static constexpr int N = 8;
    void prepare(double sampleRate);
    void reset();
    // size/damp/width 0..1. Call once per block.
    void setParams(float size, float damp, float width);
    // Produces the WET signal only (it is never mixed with the dry input here).
    void processWet(const float* inL, const float* inR, float* wetL, float* wetR, int n);

private:
    double sr = 44100.0;
    DelayLine line[N], pre[2];
    float baseLen[N] {};          // seconds at size = 1.0 scale
    float scaleCur = 1.f, scaleTarget = 1.f;
    float fb[N] {}, lpS[N] {}, hpS[N] {};
    float lpC = 0.5f, hpC = 0.01f, inHpC = 0.02f, inHpS[2] {};
    float modPh[N] {}, modInc[N] {};
    float modDepth = 0.f, preSamples = 0.f, outGain = 0.3f, width = 1.f;
    bool ready = false;
};

// Global effect chain: Distortion > Saturation > Bitcrush > Phaser > Flanger > Chorus > EQ/HP/LP
//                      > Compressor > Delay > Reverb > Stereo width > Master (+ soft safety limiter)
class FxChain
{
public:
    void prepare(double sampleRate, int maxBlock);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, const float* values, double bpm);

private:
    using Sm = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;

    void distortion(float* L, float* R, int n, const float* v);
    void saturation(float* L, float* R, int n, const float* v);
    void bitcrush(float* L, float* R, int n, const float* v);
    void phaser(float* L, float* R, int n, const float* v);
    void flanger(float* L, float* R, int n, const float* v);
    void chorus(float* L, float* R, int n, const float* v);
    void eq(float* L, float* R, int n, const float* v);
    void compressor(float* L, float* R, int n, const float* v);
    void delay(float* L, float* R, int n, const float* v, double bpm);
    void reverb(float* L, float* R, int n, const float* v);

    double sr = 44100.0;
    Sm smDsMix, smDsDrive, smSat, smBcMix, smPhMix, smFlMix, smChMix, smDlMix, smRvMix, smMaster, smWidth;

    float dcX[2] {}, dcY[2] {};
    float dsDc[2] {};
    float bcHold[2] {}; int bcCount = 0;
    float phLfo = 0, flLfo = 0, chLfo = 0;
    float phAp[2][6] {}, phFbState[2] {};
    DelayLine flLine[2], chLine[2];
    float flFbState[2] {};
    Biquad eqLowF[2], eqMidF[2], eqHighF[2], hpF[2], lpF[2];
    float cpEnv = 0.f, cpGain = 1.f;
    DelayLine dlLine[2];
    float dlTimeSm = 0.f, dlToneState[2] {};
    FdnReverb fdn;
    std::vector<float> scratchL, scratchR, wetL, wetR;
    float dlHpS[2] {};
    float widLfo = 0.f, sideLp[2] {};
    int rvSilent = 0;
};
} // namespace aura
