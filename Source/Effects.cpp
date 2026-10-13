#include "Effects.h"
#include <cmath>
#include <algorithm>

namespace aura
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
inline float clampf(float x, float lo, float hi) noexcept { return x < lo ? lo : (x > hi ? hi : x); }

inline float softClip(float x) noexcept
{
    constexpr float t = 0.85f;
    const float a = std::abs(x);
    if (a <= t) return x;
    const float s = x < 0.f ? -1.f : 1.f;
    return s * (t + (1.f - t) * std::tanh((a - t) / (1.f - t)));
}

inline bool silentSm(const juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>& s) noexcept
{
    return s.getCurrentValue() < 1e-4f && s.getTargetValue() < 1e-4f;
}
} // namespace

// ------------------------------------------------------------------------------------------
void Biquad::set(double nb0, double nb1, double nb2, double na0, double na1, double na2)
{
    b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
    a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
}
void Biquad::lowShelf(double sr, double f, double gainDb)
{
    const double A = std::pow(10.0, gainDb / 40.0), w0 = 2 * kPi * f / sr, cw = std::cos(w0), sw = std::sin(w0);
    const double alpha = sw / 2.0 * std::sqrt(2.0), sq = 2.0 * std::sqrt(A) * alpha;
    set(A * ((A + 1) - (A - 1) * cw + sq), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - sq),
        (A + 1) + (A - 1) * cw + sq, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - sq);
}
void Biquad::highShelf(double sr, double f, double gainDb)
{
    const double A = std::pow(10.0, gainDb / 40.0), w0 = 2 * kPi * f / sr, cw = std::cos(w0), sw = std::sin(w0);
    const double alpha = sw / 2.0 * std::sqrt(2.0), sq = 2.0 * std::sqrt(A) * alpha;
    set(A * ((A + 1) + (A - 1) * cw + sq), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - sq),
        (A + 1) - (A - 1) * cw + sq, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - sq);
}
void Biquad::peak(double sr, double f, double q, double gainDb)
{
    const double A = std::pow(10.0, gainDb / 40.0), w0 = 2 * kPi * f / sr, cw = std::cos(w0), alpha = std::sin(w0) / (2 * q);
    set(1 + alpha * A, -2 * cw, 1 - alpha * A, 1 + alpha / A, -2 * cw, 1 - alpha / A);
}
void Biquad::lowpass(double sr, double f, double q)
{
    const double w0 = 2 * kPi * f / sr, cw = std::cos(w0), alpha = std::sin(w0) / (2 * q);
    set((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + alpha, -2 * cw, 1 - alpha);
}
void Biquad::highpass(double sr, double f, double q)
{
    const double w0 = 2 * kPi * f / sr, cw = std::cos(w0), alpha = std::sin(w0) / (2 * q);
    set((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + alpha, -2 * cw, 1 - alpha);
}

void DelayLine::init(int minSize)
{
    int s = 1;
    while (s < minSize) s <<= 1;
    buf.assign((size_t) s, 0.f);
    mask = s - 1; w = 0;
}
float DelayLine::read(float d) const noexcept
{
    const float pos = (float) w - d;
    float fl = std::floor(pos);
    const float fr = pos - fl;
    const int i0 = ((int) fl) & mask, i1 = (i0 + 1) & mask;
    return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * fr;
}

// ------------------------------------------------------------------------------------------
//  FDN reverb
// ------------------------------------------------------------------------------------------
void FdnReverb::prepare(double sampleRate)
{
    sr = sampleRate;
    // mutually "unrelated" base lengths in ms (prime-ish) -> dense, non-metallic tail
    static const float ms[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.3f, 71.9f };
    for (int i = 0; i < N; ++i)
    {
        baseLen[i] = ms[i] * 0.001f;
        line[i].init((int) (sr * 0.12) + 64);                 // max length 72 ms * 1.45 + modulation
        modPh[i] = (float) i / (float) N;
        modInc[i] = (0.25f + 0.09f * (float) i) / (float) sr;  // 0.25 .. 0.9 Hz
    }
    for (int c = 0; c < 2; ++c) pre[c].init((int) (sr * 0.12) + 64);
    modDepth = (float) (0.00035 * sr);
    inHpC = 1.f - std::exp(-2.f * (float) kPi * 160.f / (float) sr);
    hpC   = 1.f - std::exp(-2.f * (float) kPi * 110.f / (float) sr);
    ready = true;
    reset();
    setParams(0.5f, 0.5f, 1.f);
    scaleCur = scaleTarget;
}

void FdnReverb::reset()
{
    for (int i = 0; i < N; ++i) { line[i].clear(); lpS[i] = hpS[i] = 0.f; }
    for (int c = 0; c < 2; ++c) { pre[c].clear(); inHpS[c] = 0.f; }
}

void FdnReverb::setParams(float size, float damp, float w)
{
    size = clampf(size, 0.f, 1.f); damp = clampf(damp, 0.f, 1.f);
    scaleTarget = 0.55f + 0.9f * size;                                  // room size (delay length scale)
    const float rt60 = 0.35f + std::pow(size, 1.5f) * 6.5f;             // 0.35 s .. 6.85 s
    for (int i = 0; i < N; ++i)
        fb[i] = std::pow(10.f, -3.f * baseLen[i] * scaleTarget / rt60);
    const float dampHz = 13000.f * std::pow(1500.f / 13000.f, damp);    // 13 kHz .. 1.5 kHz in the loop
    lpC = 1.f - std::exp(-2.f * (float) kPi * dampHz / (float) sr);
    preSamples = (0.010f + 0.030f * size) * (float) sr;                 // 10 .. 40 ms pre-delay keeps the dry note clear
    width = clampf(w, 0.f, 1.f);
    outGain = 0.55f / std::sqrt(std::max(0.5f, rt60));
}

void FdnReverb::processWet(const float* inL, const float* inR, float* wetL, float* wetR, int n)
{
    static const float sgnL[N] = { 1, -1, 1, -1, 1, -1, 1, -1 };
    static const float sgnR[N] = { 1, 1, -1, -1, 1, 1, -1, -1 };
    scaleCur += (scaleTarget - scaleCur) * 0.15f;       // glide the room size (once per block)
    float y[N];
    for (int s = 0; s < n; ++s)
    {
        pre[0].push(inL[s]); pre[1].push(inR[s]);
        float xin[2] = { pre[0].read(preSamples), pre[1].read(preSamples) };
        for (int c = 0; c < 2; ++c) { inHpS[c] += (xin[c] - inHpS[c]) * inHpC; xin[c] -= inHpS[c]; }

        float sum = 0.f;
        for (int i = 0; i < N; ++i)
        {
            modPh[i] += modInc[i]; if (modPh[i] >= 1.f) modPh[i] -= 1.f;
            const float d = baseLen[i] * scaleCur * (float) sr + std::sin(2.f * (float) kPi * modPh[i]) * modDepth;
            y[i] = line[i].read(d);
            sum += y[i];
        }
        const float half = sum * (2.f / (float) N);
        float oL = 0.f, oR = 0.f;
        for (int i = 0; i < N; ++i)
        {
            oL += y[i] * sgnL[i]; oR += y[i] * sgnR[i];
            float f = y[i] - half;                                   // Householder feedback matrix
            lpS[i] += (f - lpS[i]) * lpC;                            // damping
            hpS[i] += (lpS[i] - hpS[i]) * hpC;                       // keep the lows from building up
            f = lpS[i] - hpS[i];
            line[i].push(f * fb[i] + xin[i & 1] * 0.5f);
        }
        oL *= outGain; oR *= outGain;
        const float m = 0.5f * (oL + oR), sd = 0.5f * (oL - oR) * width;
        wetL[s] = m + sd; wetR[s] = m - sd;
    }
}

// ------------------------------------------------------------------------------------------
void FxChain::prepare(double sampleRate, int maxBlock)
{
    sr = sampleRate;
    for (int c = 0; c < 2; ++c)
    {
        flLine[c].init((int) (sr * 0.02) + 8);
        chLine[c].init((int) (sr * 0.06) + 8);
        dlLine[c].init((int) (sr * 2.1) + 8);
    }
    scratchL.assign((size_t) std::max(maxBlock, 64), 0.f);
    scratchR.assign((size_t) std::max(maxBlock, 64), 0.f);
    wetL.assign((size_t) std::max(maxBlock, 64), 0.f);
    wetR.assign((size_t) std::max(maxBlock, 64), 0.f);
    fdn.prepare(sr);
    for (auto* s : { &smDsMix, &smDsDrive, &smSat, &smBcMix, &smPhMix, &smFlMix, &smChMix, &smDlMix, &smRvMix, &smMaster, &smWidth })
        s->reset(sr, 0.03);
    reset();
}

void FxChain::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        flLine[c].clear(); chLine[c].clear(); dlLine[c].clear();
        for (auto& a : phAp[c]) a = 0.f;
        phFbState[c] = flFbState[c] = dlToneState[c] = bcHold[c] = dsDc[c] = 0.f;
        dcX[c] = dcY[c] = 0.f;
        eqLowF[c].reset(); eqMidF[c].reset(); eqHighF[c].reset(); hpF[c].reset(); lpF[c].reset();
    }
    cpEnv = 0.f; cpGain = 1.f; bcCount = 0; dlTimeSm = 0.f;
    dlHpS[0] = dlHpS[1] = 0.f; sideLp[0] = sideLp[1] = 0.f; widLfo = 0.f; rvSilent = 0;
    fdn.reset();
}

void FxChain::process(juce::AudioBuffer<float>& buffer, const float* v, double bpm)
{
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0) return;
    if ((int) scratchL.size() < n)
    {
        scratchL.assign((size_t) n, 0.f); scratchR.assign((size_t) n, 0.f);
        wetL.assign((size_t) n, 0.f); wetR.assign((size_t) n, 0.f);
    }
    float* L = buffer.getWritePointer(0);
    float* R = buffer.getWritePointer(1);

    smDsMix.setTargetValue(v[dsMix]);  smDsDrive.setTargetValue(v[dsDrive]);
    smSat.setTargetValue(clampf(v[saAmt] + v[warmth] * 0.45f, 0.f, 1.f));          // Warmth = gentle saturation + tilt
    smBcMix.setTargetValue(v[bcMix]);  smPhMix.setTargetValue(v[phMix]);     smFlMix.setTargetValue(v[flMix]);
    smChMix.setTargetValue(v[chMix]);  smDlMix.setTargetValue(v[dlMix]);     smRvMix.setTargetValue(v[rvMix]);
    // preset gain (calibrated) x makeup x user volume
    smMaster.setTargetValue(v[master] * 2.f * (v[volume] * 1.25f));
    smWidth.setTargetValue(v[stWidth]);

    distortion(L, R, n, v);
    saturation(L, R, n, v);
    bitcrush(L, R, n, v);
    phaser(L, R, n, v);
    flanger(L, R, n, v);
    chorus(L, R, n, v);
    eq(L, R, n, v);
    compressor(L, R, n, v);
    delay(L, R, n, v, bpm);
    reverb(L, R, n, v);

    // stereo width (bass below ~130 Hz is kept mono for mono compatibility) + master + DC blocker + safety soft clip
    const float moveAmt = v[movement];
    const float lfoInc = 0.07f / (float) sr;
    const float sideC = 1.f - std::exp(-2.f * (float) kPi * 130.f / (float) sr);
    for (int i = 0; i < n; ++i)
    {
        float w = smWidth.getNextValue();
        if (moveAmt > 0.001f)
        {
            widLfo += lfoInc; if (widLfo >= 1.f) widLfo -= 1.f;
            w *= 1.f + 0.12f * moveAmt * std::sin(2.f * (float) kPi * widLfo);
        }
        const float g = smMaster.getNextValue();
        const float m = 0.5f * (L[i] + R[i]);
        const float sRaw = 0.5f * (L[i] - R[i]);
        sideLp[0] += (sRaw - sideLp[0]) * sideC;
        const float s = (sRaw - sideLp[0] * 0.85f) * w;
        float l = (m + s) * g, r = (m - s) * g;
        const float yl = l - dcX[0] + 0.9993f * dcY[0]; dcX[0] = l; dcY[0] = yl;
        const float yr = r - dcX[1] + 0.9993f * dcY[1]; dcX[1] = r; dcY[1] = yr;
        L[i] = softClip(yl);
        R[i] = softClip(yr);
    }
}

// ------------------------------------------------------------------------------------------
void FxChain::distortion(float* L, float* R, int n, const float* v)
{
    if (silentSm(smDsMix)) { smDsMix.skip(n); smDsDrive.skip(n); return; }
    const int type = (int) v[dsType];
    for (int i = 0; i < n; ++i)
    {
        const float mix = smDsMix.getNextValue();
        const float drive = 1.f + smDsDrive.getNextValue() * 24.f;
        const float comp = 1.f / std::sqrt(drive);
        float* ch[2] = { L + i, R + i };
        for (int c = 0; c < 2; ++c)
        {
            const float x = *ch[c];
            float y;
            switch (type)
            {
                case 0:  y = std::tanh(x * drive) * (0.6f + 0.4f * comp); break;
                case 1:  y = clampf(x * drive, -1.f, 1.f) * 0.7f * (0.6f + 0.4f * comp); break;
                default:
                {
                    float f = x * drive * 0.5f;
                    f = f - 4.f * std::floor((f + 1.f) / 4.f);          // triangle fold into [-1, 3)
                    f = f > 1.f ? 2.f - f : f;                          // fold back
                    y = clampf(f, -1.f, 1.f) * (0.6f + 0.4f * comp);
                    break;
                }
            }
            *ch[c] = x + (y - x) * mix;
        }
    }
}

void FxChain::saturation(float* L, float* R, int n, const float* )
{
    if (silentSm(smSat)) { smSat.skip(n); return; }
    for (int i = 0; i < n; ++i)
    {
        const float a = smSat.getNextValue();
        const float g = 1.f + 3.f * a, bias = 0.18f * a;
        const float off = std::tanh(bias);
        for (int c = 0; c < 2; ++c)
        {
            float& s = c == 0 ? L[i] : R[i];
            const float y = (std::tanh(s * g + bias) - off) / std::max(0.6f, std::tanh(g * 0.6f)) * 0.9f;
            s = s + (y - s) * a;
        }
    }
}

void FxChain::bitcrush(float* L, float* R, int n, const float* v)
{
    if (silentSm(smBcMix)) { smBcMix.skip(n); return; }
    const float levels = std::pow(2.f, std::round(clampf(v[bcBits], 2.f, 16.f)) - 1.f);
    const int rate = std::max(1, (int) std::round(v[bcRate]));
    for (int i = 0; i < n; ++i)
    {
        const float mix = smBcMix.getNextValue();
        if (bcCount == 0)
        {
            bcHold[0] = std::round(L[i] * levels) / levels;
            bcHold[1] = std::round(R[i] * levels) / levels;
        }
        bcCount = (bcCount + 1) % rate;
        L[i] += (bcHold[0] - L[i]) * mix;
        R[i] += (bcHold[1] - R[i]) * mix;
    }
}

void FxChain::phaser(float* L, float* R, int n, const float* v)
{
    if (silentSm(smPhMix)) { smPhMix.skip(n); return; }
    const float inc = v[phRate] / (float) sr, depth = v[phDepth], fb = v[phFb];
    for (int i = 0; i < n; ++i)
    {
        const float mix = smPhMix.getNextValue();
        phLfo += inc; if (phLfo >= 1.f) phLfo -= 1.f;
        for (int c = 0; c < 2; ++c)
        {
            float ph = phLfo + (c ? 0.25f : 0.f); if (ph >= 1.f) ph -= 1.f;
            const float lfo = 0.5f + 0.5f * std::sin(2.f * (float) kPi * ph);
            const float f = 250.f * std::exp2(lfo * depth * 4.f);                // 250 Hz .. 4 kHz
            const float t = std::tan((float) kPi * std::min(f, 0.45f * (float) sr) / (float) sr);
            const float a = (t - 1.f) / (t + 1.f);
            float& s = c == 0 ? L[i] : R[i];
            float x = s + phFbState[c] * fb;
            for (int st = 0; st < 6; ++st)
            {
                const float y = a * x + phAp[c][st];
                phAp[c][st] = x - a * y;
                x = y;
            }
            phFbState[c] = std::tanh(x);
            s = s * (1.f - 0.5f * mix) + x * 0.5f * mix;
        }
    }
}

void FxChain::flanger(float* L, float* R, int n, const float* v)
{
    if (silentSm(smFlMix)) { smFlMix.skip(n); return; }
    const float inc = v[flRate] / (float) sr, depth = v[flDepth], fb = v[flFb];
    for (int i = 0; i < n; ++i)
    {
        const float mix = smFlMix.getNextValue();
        flLfo += inc; if (flLfo >= 1.f) flLfo -= 1.f;
        for (int c = 0; c < 2; ++c)
        {
            float ph = flLfo + (c ? 0.25f : 0.f); if (ph >= 1.f) ph -= 1.f;
            const float lfo = 0.5f + 0.5f * std::sin(2.f * (float) kPi * ph);
            const float dly = (0.0004f + lfo * depth * 0.0035f) * (float) sr;
            float& s = c == 0 ? L[i] : R[i];
            flLine[c].push(s + flFbState[c] * fb);
            const float wet = flLine[c].read(dly);
            flFbState[c] = wet;
            s = (s + wet * mix) / (1.f + 0.5f * mix);
        }
    }
}

void FxChain::chorus(float* L, float* R, int n, const float* v)
{
    if (silentSm(smChMix)) { smChMix.skip(n); return; }
    const float inc = v[chRate] / (float) sr, depth = v[chDepth];
    for (int i = 0; i < n; ++i)
    {
        const float mix = smChMix.getNextValue();
        chLfo += inc; if (chLfo >= 1.f) chLfo -= 1.f;
        for (int c = 0; c < 2; ++c)
        {
            float& s = c == 0 ? L[i] : R[i];
            chLine[c].push(s);
            float wet = 0.f;
            for (int t = 0; t < 2; ++t)
            {
                float ph = chLfo + (c ? 0.25f : 0.f) + (t ? 0.5f : 0.f); ph -= std::floor(ph);
                const float lfo = std::sin(2.f * (float) kPi * ph);
                wet += chLine[c].read((0.014f + lfo * depth * 0.006f + (t ? 0.004f : 0.f)) * (float) sr);
            }
            wet *= 0.5f;
            s = s * (1.f - 0.35f * mix) + wet * 0.85f * mix;
        }
    }
}

void FxChain::eq(float* L, float* R, int n, const float* v)
{
    // Brightness (+-5 dB high shelf) and Warmth (low shelf up, highs slightly down) are folded into the EQ.
    const float lowG  = v[eqLow]  + v[warmth] * 2.5f;
    const float highG = v[eqHigh] + (v[bright] - 0.5f) * 10.f - v[warmth] * 2.f;
    const bool hpOn = v[fxHP] > 21.f, lpOn = v[fxLP] < 19900.f;
    const bool bands = std::abs(lowG) > 0.05f || std::abs(v[eqMid]) > 0.05f || std::abs(highG) > 0.05f;
    if (!hpOn && !lpOn && !bands) return;
    for (int c = 0; c < 2; ++c)
    {
        eqLowF[c].lowShelf(sr, 130.0, lowG);
        eqMidF[c].peak(sr, v[eqMidFreq], 0.8, v[eqMid]);
        eqHighF[c].highShelf(sr, 5500.0, highG);
        hpF[c].highpass(sr, v[fxHP], 0.7071);
        lpF[c].lowpass(sr, std::min((double) v[fxLP], 0.45 * sr), 0.7071);
    }
    for (int c = 0; c < 2; ++c)
    {
        float* d = c == 0 ? L : R;
        for (int i = 0; i < n; ++i)
        {
            float x = d[i];
            if (bands) x = eqHighF[c].process(eqMidF[c].process(eqLowF[c].process(x)));
            if (hpOn) x = hpF[c].process(x);
            if (lpOn) x = lpF[c].process(x);
            d[i] = x;
        }
    }
}

void FxChain::compressor(float* L, float* R, int n, const float* v)
{
    const float ratio = v[cpRatio];
    if (ratio < 1.02f || v[cpThresh] > -0.5f) { cpGain = 1.f; return; }
    const float thr = v[cpThresh];
    const float atk = std::exp(-1.f / (0.008f * (float) sr));
    const float rel = std::exp(-1.f / (v[cpRel] * 0.001f * (float) sr));
    const float makeup = std::pow(10.f, (-thr * (1.f - 1.f / ratio) * 0.4f) / 20.f);
    for (int i = 0; i < n; ++i)
    {
        const float lvl = std::max(std::abs(L[i]), std::abs(R[i]));
        cpEnv = lvl > cpEnv ? atk * cpEnv + (1.f - atk) * lvl : rel * cpEnv + (1.f - rel) * lvl;
        const float db = 20.f * std::log10(std::max(cpEnv, 1e-6f));
        const float over = db - thr;
        const float gr = over > 0.f ? -over * (1.f - 1.f / ratio) : 0.f;
        const float g = std::pow(10.f, gr / 20.f) * makeup;
        L[i] *= g; R[i] *= g;
    }
}

void FxChain::delay(float* L, float* R, int n, const float* v, double bpm)
{
    if (silentSm(smDlMix)) { smDlMix.skip(n); for (int c = 0; c < 2; ++c) dlLine[c].push(0.f); /* keep line moving cheaply */ 
        // feed silence for the block so stale echoes do not pop in later
        for (int i = 1; i < n; ++i) for (int c = 0; c < 2; ++c) dlLine[c].push(0.f);
        return; }

    static const float beats[6] = { 0.f, 1.f, 0.5f, 0.75f, 0.25f, 1.5f };
    float targetMs = v[dlTime];
    const int sync = (int) v[dlSync];
    if (sync > 0) targetMs = (float) (60000.0 / std::max(20.0, bpm)) * beats[sync];
    const float targetSamples = clampf(targetMs * 0.001f * (float) sr, 16.f, (float) (sr * 2.0));
    if (dlTimeSm < 1.f) dlTimeSm = targetSamples;
    const float fb = v[dlFb];
    const bool ping = v[dlPing] > 0.5f;
    const float toneCoef = 1.f - std::exp(-2.f * (float) kPi * std::min(v[dlTone], 0.45f * (float) sr) / (float) sr);
    const float slew = 1.f - std::exp(-1.f / (0.05f * (float) sr));
    const float hpCoef = 1.f - std::exp(-2.f * (float) kPi * 180.f / (float) sr);

    for (int i = 0; i < n; ++i)
    {
        const float mix = smDlMix.getNextValue();
        dlTimeSm += (targetSamples - dlTimeSm) * slew;
        float wet[2];
        for (int c = 0; c < 2; ++c)
        {
            const float raw = dlLine[c].read(dlTimeSm);
            dlToneState[c] += (raw - dlToneState[c]) * toneCoef;            // low-pass: echoes get darker
            dlHpS[c] += (dlToneState[c] - dlHpS[c]) * hpCoef;               // high-pass: no low-end build-up
            wet[c] = dlToneState[c] - dlHpS[c];
        }
        const float inL = L[i], inR = R[i];
        // feedback is soft-limited so it can never run away or get harsh
        const float f0 = std::tanh(wet[0] * fb), f1 = std::tanh(wet[1] * fb);
        if (ping)
        {
            dlLine[0].push(0.5f * (inL + inR) + f1);
            dlLine[1].push(f0);
        }
        else
        {
            dlLine[0].push(inL + f0);
            dlLine[1].push(inR + f1);
        }
        L[i] = inL + wet[0] * mix;
        R[i] = inR + wet[1] * mix;
    }
}

void FxChain::reverb(float* L, float* R, int n, const float* v)
{
    if (silentSm(smRvMix))
    {
        smRvMix.skip(n);
        if (rvSilent++ == 0) fdn.reset();          // tail is inaudible now; clear it so it cannot pop back in later
        return;
    }
    rvSilent = 0;
    fdn.setParams(v[rvSize], v[rvDamp], v[rvWidth]);
    fdn.processWet(L, R, wetL.data(), wetR.data(), n);
    for (int i = 0; i < n; ++i)
    {
        const float mix = smRvMix.getNextValue();
        L[i] += wetL[(size_t) i] * mix;
        R[i] += wetR[(size_t) i] * mix;
    }
}
} // namespace aura
