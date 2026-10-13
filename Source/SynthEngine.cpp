#include "SynthEngine.h"
#include <cmath>
#include <algorithm>

namespace aura
{
namespace
{
constexpr float kPi  = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float wrap01(float x) noexcept { const float r = x - std::floor(x); return r >= 1.f ? 0.f : r; }
inline float clampf(float x, float lo, float hi) noexcept { return x < lo ? lo : (x > hi ? hi : x); }

struct Vowel { float f[3], db[3], bw[3]; };
// a e i o u (classic formant table)
constexpr Vowel kVowels[5] = {
    { { 650, 1080, 2650 }, { 0, -6, -7 },   { 80, 90, 120 } },
    { { 400, 1700, 2600 }, { 0, -14, -12 }, { 70, 80, 100 } },
    { { 290, 1870, 2800 }, { 0, -15, -18 }, { 40, 90, 100 } },
    { { 400, 800, 2600 },  { 0, -10, -12 }, { 40, 80, 100 } },
    { { 350, 600, 2700 },  { 0, -20, -17 }, { 40, 80, 100 } },
};

// parameter index tables for the two oscillators
struct OscParamIdx { int level, wave, oct, semi, fine, pw, wt, uni, det, spread; };
constexpr OscParamIdx kOscIdx[2] = {
    { o1Level, o1Wave, o1Oct, o1Semi, o1Fine, o1PW, o1WT, o1Uni, o1Det, o1Spread },
    { o2Level, o2Wave, o2Oct, o2Semi, o2Fine, o2PW, o2WT, o2Uni, o2Det, o2Spread },
};
constexpr int kSlotIdx[6][3] = {
    { m1Src, m1Dst, m1Amt }, { m2Src, m2Dst, m2Amt }, { m3Src, m3Dst, m3Amt },
    { m4Src, m4Dst, m4Amt }, { m5Src, m5Dst, m5Amt }, { m6Src, m6Dst, m6Amt },
};

inline void makeSvf(float fc, float k, double sr, float& g, float& a1, float& a2, float& a3) noexcept
{
    g = std::tan(kPi * fc / (float) sr);
    a1 = 1.f / (1.f + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
}
} // namespace

void computeEnvCoefs(EnvCoefs& c, float a, float d, float s, float r, double sr)
{
    const double trA = 0.3, trD = 0.0001;
    auto coef = [&](double t, double ratio) {
        const double n = std::max(1.0, t * sr);
        return std::exp(-std::log((1.0 + ratio) / ratio) / n);
    };
    c.aCoef = (float) coef(a, trA);  c.aBase = (float) ((1.0 + trA) * (1.0 - c.aCoef));
    c.dCoef = (float) coef(d, trD);  c.dBase = (float) ((s - trD) * (1.0 - c.dCoef));
    c.rCoef = (float) coef(r, trD);  c.rBase = (float) (-trD * (1.0 - c.rCoef));
    c.sus = s;
}

SynthEngine::SynthEngine()
{
    for (int i = 0; i < kNumParams; ++i) p[i] = paramDefault(i);
    events.reserve(2048);
    arpNotes.reserve(32); arpVel.reserve(32); arpPedalHeld.reserve(32);
    monoStack.reserve(32);
}

void SynthEngine::prepare(double sampleRate, int)
{
    sr = sampleRate;
    tables = WaveTables::get(sr);
    reset();
}

void SynthEngine::reset()
{
    for (auto& v : voices) v = Voice();
    monoStack.clear(); arpNotes.clear(); arpVel.clear(); arpPedalHeld.clear();
    pedalDown = false; arpGateActive = false; arpPlaying = -1; arpNext = 0.0;
    bendTarget = bendCur = 0.f; wheelTarget = wheelCur = 0.f; bendRaw = 8192;
    lastNote = -1;
}

void SynthEngine::allNotesOff(bool immediate)
{
    monoStack.clear(); arpNotes.clear(); arpVel.clear(); arpPedalHeld.clear();
    arpGateActive = false; arpPlaying = -1;
    for (auto& v : voices)
    {
        if (!v.active) continue;
        v.gate = false; v.sustained = false;
        if (immediate) { v = Voice(); } else releaseVoice(v);
    }
}

// ------------------------------------------------------------------------------------------
//  MIDI / voice management
// ------------------------------------------------------------------------------------------
void SynthEngine::handleMidi(const juce::MidiMessage& m)
{
    if (m.isNoteOn())            noteOn(m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())      noteOff(m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) allNotesOff(m.isAllSoundOff());
    else if (m.isPitchWheel())
    {
        bendRaw = m.getPitchWheelValue();
        bendTarget = ((float) (bendRaw - 8192) / 8192.f) * P(vBend);
    }
    else if (m.isController())
    {
        const int cc = m.getControllerNumber(), val = m.getControllerValue();
        if (cc == 1)       wheelTarget = (float) val / 127.f;
        else if (cc == 64) sustainPedal(val >= 64);
    }
}

void SynthEngine::noteOn(int note, float vel)
{
    if (P(arpOn) > 0.5f)
    {
        auto it = std::lower_bound(arpNotes.begin(), arpNotes.end(), note);
        const bool wasEmpty = arpNotes.empty();
        if (it == arpNotes.end() || *it != note)
        {
            const auto idx = it - arpNotes.begin();
            arpNotes.insert(it, note);
            arpVel.insert(arpVel.begin() + idx, vel);
        }
        arpPedalHeld.erase(std::remove(arpPedalHeld.begin(), arpPedalHeld.end(), note), arpPedalHeld.end());
        if (wasEmpty) { arpNext = 0.0; arpStep = 0; arpDir = 1; }
        return;
    }
    voiceNoteOn(note, vel);
}

void SynthEngine::noteOff(int note)
{
    if (P(arpOn) > 0.5f)
    {
        if (pedalDown) { arpPedalHeld.push_back(note); return; }
        auto it = std::find(arpNotes.begin(), arpNotes.end(), note);
        if (it != arpNotes.end())
        {
            arpVel.erase(arpVel.begin() + (it - arpNotes.begin()));
            arpNotes.erase(it);
        }
        return;
    }
    voiceNoteOff(note);
}

void SynthEngine::sustainPedal(bool down)
{
    pedalDown = down;
    if (down) return;

    // pedal released: let go of everything that was only being held by the pedal
    for (int n : arpPedalHeld)
    {
        auto it = std::find(arpNotes.begin(), arpNotes.end(), n);
        if (it != arpNotes.end()) { arpVel.erase(arpVel.begin() + (it - arpNotes.begin())); arpNotes.erase(it); }
    }
    arpPedalHeld.clear();
    for (auto& v : voices)
        if (v.active && v.sustained) { v.sustained = false; if (!v.gate) releaseVoice(v); }
}

void SynthEngine::releaseVoice(Voice& v)
{
    v.ampEnv.gateOff();
    v.fltEnv.gateOff();
    v.layEnv.gateOff();
}

Voice* SynthEngine::allocateVoice(int note)
{
    // 1. retrigger a voice already playing this note
    for (auto& v : voices) if (v.active && v.note == note && v.gate) return &v;
    // 2. free voice
    for (auto& v : voices) if (!v.active) return &v;
    // 3. steal: quietest releasing voice, otherwise the oldest
    Voice* best = nullptr;
    for (auto& v : voices)
        if (v.ampEnv.stage == Env::Release && (!best || v.ampEnv.v < best->ampEnv.v)) best = &v;
    if (best) return best;
    best = &voices[0];
    for (auto& v : voices) if (v.age < best->age) best = &v;
    return best;
}

void SynthEngine::startVoice(Voice& v, int note, float vel, float glideFrom, bool retrigger)
{
    const bool wasActive = v.active;
    v.note = note; v.vel = vel; v.age = ++ageCounter; v.gate = true; v.sustained = false;

    if (!wasActive)
    {
        v.ampEnv = Env(); v.fltEnv = Env(); v.layEnv = Env();
        v.f1[0] = v.f1[1] = v.f2[0] = v.f2[1] = SVF();
        v.ly[0] = v.ly[1] = SVF();
        for (int i = 0; i < 3; ++i)
        {
            v.mvPh[i] = arpRand.nextFloat();
            v.mvRate[i] = (0.08f + 0.05f * (float) i) * (0.75f + 0.5f * arpRand.nextFloat());
        }
        v.drift = v.driftT = 0.f; v.driftCnt = 0;
        for (auto& ch : v.fm) for (auto& s : ch) s = SVF();
        v.noiseLP = 0.f;
        v.rng = (std::uint32_t) (arpRand.nextInt() | 1);
        v.fmPh = 0.f; v.subPh = 0.f;
        const bool rnd1 = P(o1Uni) > 1.5f, rnd2 = P(o2Uni) > 1.5f;
        for (int i = 0; i < kMaxUni; ++i)
        {
            v.ph1[i] = rnd1 ? arpRand.nextFloat() : 0.f;
            v.ph2[i] = rnd2 ? arpRand.nextFloat() : 0.f;
            v.mph[i] = 0.f;
        }
        for (int i = 0; i < 3; ++i)
        {
            v.lfoPh[i] = 0.f;
            v.lfoSH[i] = arpRand.nextFloat() * 2.f - 1.f;
            v.lfoVal[i] = 0.f;
        }
    }
    v.noteCur = (glideFrom >= 0.f && P(vGlide) > 0.0005f) ? glideFrom : (float) note;
    if (retrigger || !wasActive)
    {
        v.ampEnv.gateOn(); v.fltEnv.gateOn(); v.layEnv.gateOn();
    }
    v.active = true;
}

void SynthEngine::voiceNoteOn(int note, float vel)
{
    const int mode = (int) P(vMode);
    if (mode == Poly)
    {
        Voice* v = allocateVoice(note);
        startVoice(*v, note, vel, lastNote >= 0 ? (float) lastNote : -1.f, true);
        lastNote = note;
        return;
    }

    // Mono / Legato share voice 0 and keep a last-note-priority stack
    monoStack.erase(std::remove_if(monoStack.begin(), monoStack.end(),
                                   [note](const auto& e) { return e.first == note; }), monoStack.end());
    const bool anyHeld = !monoStack.empty() && voices[0].active && (voices[0].gate || voices[0].sustained);
    monoStack.emplace_back(note, vel);

    Voice& v = voices[0];
    const bool legato = (mode == Legato) && anyHeld && v.active;
    const float from = v.active ? v.noteCur : (lastNote >= 0 ? (float) lastNote : -1.f);
    if (legato)
    {
        v.note = note; v.vel = vel; v.gate = true; v.sustained = false; v.age = ++ageCounter;
        if (v.ampEnv.stage == Env::Release || v.ampEnv.stage == Env::Idle) { v.ampEnv.gateOn(); v.fltEnv.gateOn(); v.layEnv.gateOn(); }
        if (P(vGlide) <= 0.0005f) v.noteCur = (float) note;
    }
    else
    {
        startVoice(v, note, vel, from, true);
    }
    for (int i = 1; i < kMaxVoices; ++i) // mono voice steals everything else
        if (voices[(size_t) i].active) releaseVoice(voices[(size_t) i]);
    lastNote = note;
}

void SynthEngine::voiceNoteOff(int note)
{
    const int mode = (int) P(vMode);
    if (mode == Poly)
    {
        for (auto& v : voices)
            if (v.active && v.note == note && v.gate)
            {
                v.gate = false;
                if (pedalDown) v.sustained = true; else releaseVoice(v);
            }
        return;
    }

    monoStack.erase(std::remove_if(monoStack.begin(), monoStack.end(),
                                   [note](const auto& e) { return e.first == note; }), monoStack.end());
    Voice& v = voices[0];
    if (!v.active) return;
    if (!monoStack.empty())
    {
        const auto last = monoStack.back();
        if (v.note != last.first)
        {
            v.note = last.first; v.vel = last.second;
            if (P(vGlide) <= 0.0005f) v.noteCur = (float) v.note;
            if (mode == Mono) { v.ampEnv.gateOn(); v.fltEnv.gateOn(); v.layEnv.gateOn(); }
            lastNote = v.note;
        }
        return;
    }
    v.gate = false;
    if (pedalDown) v.sustained = true; else releaseVoice(v);
}

// ------------------------------------------------------------------------------------------
//  Arpeggiator
// ------------------------------------------------------------------------------------------
double SynthEngine::arpStepSamples() const
{
    static const double beats[6] = { 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
    const int r = juce::jlimit(0, 5, (int) P(arpRate));
    return sr * 60.0 / std::max(20.0, bpmNow) * beats[r];
}

void SynthEngine::fireArpStep()
{
    if (arpPlaying >= 0) { voiceNoteOff(arpPlaying); arpPlaying = -1; }
    arpGateActive = false;
    if (arpNotes.empty()) return;

    const int octs = juce::jlimit(1, 3, (int) P(arpOct));
    std::vector<std::pair<int, float>> seq;
    seq.reserve(arpNotes.size() * 3);
    for (int o = 0; o < octs; ++o)
        for (size_t i = 0; i < arpNotes.size(); ++i)
            seq.emplace_back(arpNotes[i] + 12 * o, arpVel[i]);

    const int n = (int) seq.size();
    int idx = 0;
    switch ((int) P(arpPat))
    {
        case 0: idx = arpStep % n; arpStep = (arpStep + 1) % n; break;
        case 1: idx = n - 1 - (arpStep % n); arpStep = (arpStep + 1) % n; break;
        case 2:
        {
            if (n == 1) { idx = 0; break; }
            const int period = 2 * n - 2;
            const int s = arpStep % period;
            idx = s < n ? s : period - s;
            arpStep = (arpStep + 1) % period;
            break;
        }
        default: idx = arpRand.nextInt(n); break;
    }
    const int note = juce::jlimit(0, 127, seq[(size_t) idx].first);
    voiceNoteOn(note, seq[(size_t) idx].second);
    arpPlaying = note;
    arpGateActive = true;
    arpGateT = std::max(1.0, arpStepSamples() * (double) juce::jlimit(0.05f, 1.f, P(arpGate)));
}

// ------------------------------------------------------------------------------------------
//  Block processing
// ------------------------------------------------------------------------------------------
void SynthEngine::updateBlockParams(double bpm)
{
    bpmNow = bpm > 1.0 ? bpm : 120.0;
    computeEnvCoefs(ampC, P(ampA), P(ampD), P(ampS), P(ampR), sr);
    computeEnvCoefs(fltC, P(fltA), P(fltD), P(fltS), P(fltR), sr);
    computeEnvCoefs(layC, P(layA), P(layD), P(layS), P(layR), sr);
    bendTarget = ((float) (bendRaw - 8192) / 8192.f) * P(vBend);
}

void SynthEngine::process(juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const float* values, double bpm)
{
    for (int i = 0; i < kNumParams; ++i) p[i] = values[i];
    const int numSamples = out.getNumSamples();
    out.clear();
    if (numSamples == 0) return;
    if (!tables) tables = WaveTables::get(sr);

    // arp switched off while running -> flush
    if (prevArpOn > 0.5f && P(arpOn) < 0.5f)
    {
        if (arpPlaying >= 0) { voiceNoteOff(arpPlaying); arpPlaying = -1; }
        arpNotes.clear(); arpVel.clear(); arpPedalHeld.clear(); arpGateActive = false;
    }
    prevArpOn = P(arpOn);
    updateBlockParams(bpm);

    events.clear();
    for (const auto meta : midi)
        events.emplace_back(meta.samplePosition, meta.getMessage());

    float* L = out.getWritePointer(0);
    float* R = out.getNumChannels() > 1 ? out.getWritePointer(1) : nullptr;

    size_t ev = 0;
    int pos = 0;
    const bool arpActive = P(arpOn) > 0.5f;
    while (pos < numSamples)
    {
        while (ev < events.size() && events[ev].first <= pos) handleMidi(events[ev++].second);

        int next = numSamples;
        if (ev < events.size()) next = std::min(next, std::max(pos + 1, events[ev].first));
        if (arpActive)
        {
            double toNext = std::max(0.0, arpNext);
            if (arpGateActive) toNext = std::min(toNext, std::max(0.0, arpGateT));
            next = std::min(next, pos + std::max(1, (int) std::ceil(toNext)));
        }
        const int seg = next - pos;
        renderSegment(L, R, pos, seg);

        if (arpActive)
        {
            arpNext -= seg;
            if (arpGateActive)
            {
                arpGateT -= seg;
                if (arpGateT <= 0.0) { if (arpPlaying >= 0) { voiceNoteOff(arpPlaying); arpPlaying = -1; } arpGateActive = false; }
            }
            while (arpNext <= 0.0) { fireArpStep(); arpNext += arpStepSamples(); }
        }
        pos = next;
    }

    // ---- visual feedback ----
    const Voice* newest = nullptr;
    int count = 0;
    for (const auto& v : voices)
        if (v.active) { ++count; if (!newest || v.age > newest->age) newest = &v; }
    vizVoices.store(count, std::memory_order_relaxed);
    if (newest)
    {
        vizAmpEnv.store(newest->ampEnv.v, std::memory_order_relaxed);
        vizFiltEnv.store(newest->fltEnv.v, std::memory_order_relaxed);
        for (int i = 0; i < 3; ++i) vizLfo[i].store(newest->lfoVal[i], std::memory_order_relaxed);
    }
    else
    {
        vizAmpEnv.store(0.f, std::memory_order_relaxed);
        vizFiltEnv.store(0.f, std::memory_order_relaxed);
        const int rateIdx[3] = { l1Rate, l2Rate, l3Rate };
        const int shapeIdx[3] = { l1Shape, l2Shape, l3Shape };
        for (int i = 0; i < 3; ++i)
        {
            idleLfoPh[i] = wrap01(idleLfoPh[i] + P(rateIdx[i]) * (float) numSamples / (float) sr);
            const float ph = idleLfoPh[i];
            float val = 0.f;
            switch ((int) P(shapeIdx[i]))
            {
                case LfoSine:   val = std::sin(kTwoPi * ph); break;
                case LfoTri:    val = ph < 0.25f ? 4.f * ph : (ph < 0.75f ? 2.f - 4.f * ph : 4.f * ph - 4.f); break;
                case LfoSaw:    val = 2.f * ph - 1.f; break;
                case LfoSquare: val = ph < 0.5f ? 1.f : -1.f; break;
                default:        val = std::sin(kTwoPi * ph * 3.7f) * 0.8f; break;
            }
            vizLfo[i].store(val, std::memory_order_relaxed);
        }
    }
}

void SynthEngine::renderSegment(float* L, float* R, int start, int count)
{
    // smooth controllers (once per segment)
    const float k = 1.f - std::exp(-(float) count / (0.004f * (float) sr));
    bendCur  += (bendTarget - bendCur) * k;
    wheelCur += (wheelTarget - wheelCur) * k;

    for (auto& v : voices)
        if (v.active) renderVoice(v, L + start, R ? R + start : nullptr, count);
}

// ------------------------------------------------------------------------------------------
//  Oscillator setup (once per control tick)
// ------------------------------------------------------------------------------------------
void SynthEngine::setupOsc(OscCtl& c, int which, float noteSemis, const float* d, float pitchExtra, bool fmBoost)
{
    const auto& ix = kOscIdx[which];
    const int dstWT    = which == 0 ? DstWT1 : DstWT2;
    const int dstPW    = which == 0 ? DstPW1 : DstPW2;
    const int dstLevel = which == 0 ? DstLevel1 : DstLevel2;
    const int dstPitch = which == 0 ? DstPitch1 : DstPitch2;

    c.wave  = juce::jlimit(0, 5, (int) P(ix.wave));
    c.level = clampf(P(ix.level) + d[dstLevel], 0.f, 1.5f);
    c.n     = juce::jlimit(1, kMaxUni, (int) P(ix.uni));

    const float semis = noteSemis + 12.f * P(ix.oct) + P(ix.semi) + P(ix.fine) * 0.01f
                      + pitchExtra + d[DstPitch] * 12.f + d[dstPitch] * 12.f;
    const float freq = clampf(440.f * std::exp2((semis - 69.f) / 12.f), 1.f, 0.45f * (float) sr);

    const float det = clampf(P(ix.det) + d[DstDetune] * 50.f, 0.f, 100.f);
    const float spread = P(ix.spread);
    const float uniGain = 1.f / std::sqrt((float) c.n);
    for (int i = 0; i < c.n; ++i)
    {
        const float off = c.n > 1 ? (2.f * (float) i / (float) (c.n - 1) - 1.f) : 0.f;
        const float ratio = std::exp2(off * det / 1200.f);
        c.inc[i] = freq * ratio / (float) sr;
        const float ang = (clampf(off * spread, -1.f, 1.f) + 1.f) * kPi * 0.25f;
        c.gL[i] = std::cos(ang) * 1.41421356f * uniGain;
        c.gR[i] = std::sin(ang) * 1.41421356f * uniGain;
    }
    const float maxRatio = std::exp2(det / 1200.f);
    int mip = mipForFrequency(freq * maxRatio);
    if (fmBoost) mip = std::min(mip + 1, kMipLevels - 1);
    c.mip = mip;

    c.pw = clampf(P(ix.pw) + d[dstPW] * 0.45f, 0.05f, 0.95f);
    c.pwGain = 1.f / (1.f + std::abs(1.f - 2.f * c.pw));
    c.t1 = nullptr; c.fr = 0.f;
    switch (c.wave)
    {
        case Saw:      c.t0 = tables->saw.table(0, mip); break;
        case Square:   c.t0 = tables->square.table(0, mip); break;
        case Triangle: c.t0 = tables->tri.table(0, mip); break;
        case Sine:     c.t0 = tables->sine.table(0, mip); break;
        case Pulse:    c.t0 = tables->saw.table(0, mip); break;
        default:
        {
            const float pos = clampf(P(ix.wt) + d[dstWT], 0.f, 1.f) * (float) (kWTFrames - 1);
            const int f0 = std::min((int) pos, kWTFrames - 1);
            const int f1 = std::min(f0 + 1, kWTFrames - 1);
            c.fr = pos - (float) f0;
            c.t0 = tables->wt.table(f0, mip);
            c.t1 = tables->wt.table(f1, mip);
            break;
        }
    }
}

static inline float oscRead(const int wave, const float* t0, const float* t1, float fr, float pw, float pwGain, float ph) noexcept
{
    if (wave == Pulse)
    {
        float p2 = ph + pw; if (p2 >= 1.f) p2 -= 1.f;
        return (readTable(t0, ph) - readTable(t0, p2)) * pwGain;
    }
    if (wave == Wavetable)
    {
        const float a = readTable(t0, ph);
        return fr > 0.0001f ? a + (readTable(t1, ph) - a) * fr : a;
    }
    return readTable(t0, ph);
}

// ------------------------------------------------------------------------------------------
//  Voice rendering
// ------------------------------------------------------------------------------------------
void SynthEngine::renderVoice(Voice& v, float* outL, float* outR, int count)
{
    const float srf = (float) sr;
    const int filterMode = (int) P(fType);
    const float velGain = 1.f - P(vVelSens) + P(vVelSens) * v.vel;
    const float fmRatioV = P(fmRatio);
    const float subLvl = P(subLevel), noiseLvl = P(nLevel);
    const int subWaveIdx = juce::jlimit(0, 2, (int) P(subWave));
    const float noiseCoef = 0.01f + 0.99f * P(nTone) * P(nTone);
    const float noiseGain = std::min(6.f, std::sqrt((2.f - noiseCoef) / noiseCoef)) * 0.6f;
    const bool syncOn = P(o1Sync) > 0.5f;
    const bool layerOn = P(layOn) > 0.5f;
    const float moveAmt = P(movement), driftAmt = P(vDrift);
    const float brightOct = (P(bright) - 0.5f) * 4.f;          // +-2 octaves of filter cutoff
    const bool brightAffectsCutoff = (filterMode != HP12 && filterMode != Notch);

    int done = 0;
    while (done < count)
    {
        const int n = std::min(kCtrlRate, count - done);

        // ---- glide ----
        const float glide = P(vGlide);
        if (glide > 0.0005f)
        {
            const float c = 1.f - std::exp(-(float) n / (srf * glide * 0.33f));
            v.noteCur += ((float) v.note - v.noteCur) * c;
            if (std::abs((float) v.note - v.noteCur) < 0.001f) v.noteCur = (float) v.note;
        }
        else v.noteCur = (float) v.note;

        // ---- LFOs ----
        static const int rateIdx[3]  = { l1Rate, l2Rate, l3Rate };
        static const int shapeIdx[3] = { l1Shape, l2Shape, l3Shape };
        for (int i = 0; i < 3; ++i)
        {
            float ph = v.lfoPh[i] + P(rateIdx[i]) * (float) n / srf;
            if (ph >= 1.f) { ph -= std::floor(ph); v.lfoSH[i] = arpRand.nextFloat() * 2.f - 1.f; }
            v.lfoPh[i] = ph;
            float val;
            switch ((int) P(shapeIdx[i]))
            {
                case LfoSine:   val = std::sin(kTwoPi * ph); break;
                case LfoTri:    val = ph < 0.25f ? 4.f * ph : (ph < 0.75f ? 2.f - 4.f * ph : 4.f * ph - 4.f); break;
                case LfoSaw:    val = 2.f * ph - 1.f; break;
                case LfoSquare: val = ph < 0.5f ? 1.f : -1.f; break;
                default:        val = v.lfoSH[i]; break;
            }
            v.lfoVal[i] = val;
        }

        // ---- "Movement" (slow, per-voice random-phase drift of filter / wavetable / pulse width) and analog drift ----
        float mv[3] = {};
        if (moveAmt > 0.001f)
            for (int i = 0; i < 3; ++i)
            {
                v.mvPh[i] = wrap01(v.mvPh[i] + v.mvRate[i] * (float) n / srf);
                mv[i] = std::sin(kTwoPi * v.mvPh[i]) * moveAmt;
            }
        float driftSemis = 0.f;
        if (driftAmt > 0.001f)
        {
            v.driftCnt -= n;
            if (v.driftCnt <= 0) { v.driftT = arpRand.nextFloat() * 2.f - 1.f; v.driftCnt = (int) (srf * (0.35f + 0.5f * arpRand.nextFloat())); }
            v.drift += (v.driftT - v.drift) * std::min(1.f, (float) n / (srf * 0.4f));
            driftSemis = v.drift * driftAmt * 0.09f;              // up to +-9 cents
        }

        // ---- modulation matrix ----
        float src[kNumModSrc] = {};
        src[SrcLfo1] = v.lfoVal[0]; src[SrcLfo2] = v.lfoVal[1]; src[SrcLfo3] = v.lfoVal[2];
        src[SrcFiltEnv] = v.fltEnv.v; src[SrcAmpEnv] = v.ampEnv.v;
        src[SrcVel] = v.vel; src[SrcWheel] = wheelCur;
        src[SrcKey] = clampf((v.noteCur - 60.f) / 36.f, -1.f, 1.f);
        float d[kNumModDst] = {};
        for (int s = 0; s < 6; ++s)
        {
            const int si = (int) P(kSlotIdx[s][0]), di = (int) P(kSlotIdx[s][1]);
            if (si > 0 && di > 0 && si < kNumModSrc && di < kNumModDst) d[di] += src[si] * P(kSlotIdx[s][2]);
        }
        if (moveAmt > 0.001f)
        {
            d[DstWT1] += mv[1] * 0.18f;  d[DstWT2] += mv[2] * 0.18f;
            d[DstPW1] += mv[1] * 0.25f;  d[DstPW2] += mv[2] * 0.25f;
            d[DstDetune] += mv[0] * 0.12f;
        }

        // ---- oscillators ----
        const float fmEff = clampf(P(fmAmt) + d[DstFM], 0.f, 1.f);
        OscCtl c1, c2;
        const float pitchExtraV = bendCur + driftSemis + mv[2] * 0.03f;
        setupOsc(c1, 0, v.noteCur, d, pitchExtraV, fmEff > 0.2f);
        setupOsc(c2, 1, v.noteCur, d, pitchExtraV, false);

        // sync master runs at osc2's pitch (without osc1-only pitch modulation)
        if (syncOn)
        {
            const float det = clampf(P(o1Det) + d[DstDetune] * 50.f, 0.f, 100.f);
            const float semis = v.noteCur + bendCur + 12.f * P(o2Oct) + P(o2Semi) + P(o2Fine) * 0.01f
                              + d[DstPitch] * 12.f + d[DstPitch2] * 12.f;
            const float mf = clampf(440.f * std::exp2((semis - 69.f) / 12.f), 1.f, 0.45f * srf);
            for (int i = 0; i < c1.n; ++i)
            {
                const float off = c1.n > 1 ? (2.f * (float) i / (float) (c1.n - 1) - 1.f) : 0.f;
                c2.minc[i] = mf * std::exp2(off * det / 1200.f) / srf;
            }
        }

        // FM modulator (sine at osc1 pitch * ratio)
        const float fmInc = std::min(0.49f, c1.inc[c1.n / 2] * fmRatioV);
        const float fmIdx = fmEff * fmEff * 1.5f;

        // sub oscillator
        const float subSemis = v.noteCur + bendCur + 12.f * P(subOct) + d[DstPitch] * 12.f;
        const float subFreq = clampf(440.f * std::exp2((subSemis - 69.f) / 12.f), 1.f, 0.45f * srf);
        const float subInc = subFreq / srf;
        const int subMip = mipForFrequency(subFreq);
        const float* subT = subWaveIdx == 0 ? tables->sine.table(0, subMip)
                          : subWaveIdx == 1 ? tables->tri.table(0, subMip)
                                            : tables->square.table(0, subMip);

        // ---- filter coefficients ----
        const float keyTrack = P(fKey) * (v.noteCur - 60.f) / 12.f;
        const float cutOct = keyTrack + P(fEnv) * v.fltEnv.v * 5.f + d[DstCutoff] * 4.f
                           + (brightAffectsCutoff ? brightOct : 0.f) + mv[0] * 0.9f;
        float cutoff = clampf(P(fCutoff) * std::exp2(cutOct), 20.f, std::min(20000.f, 0.45f * srf));
        const float res = clampf(P(fRes) + d[DstRes] * 0.5f, 0.f, 1.f);
        const float drive = P(fDrive);
        const float preGain = 1.f + drive * 8.f;
        const float postGain = 1.f / (1.f + drive * 1.8f);

        SvfCoef s1, s2;
        SvfCoef fb[3]; float fbAmp[3] = {};
        if (filterMode == Formant)
        {
            const float vow = clampf(P(fVowel) + d[DstVowel], 0.f, 1.f) * 4.f;
            const int v0 = std::min((int) vow, 3);
            const float fr = vow - (float) v0;
            const float shift = clampf(std::pow(cutoff / 2000.f, 0.4f), 0.5f, 2.5f);
            const float bwScale = 1.f - 0.6f * res;
            for (int b = 0; b < 3; ++b)
            {
                const float f  = (kVowels[v0].f[b]  + (kVowels[v0 + 1].f[b]  - kVowels[v0].f[b])  * fr) * shift;
                const float bw = (kVowels[v0].bw[b] + (kVowels[v0 + 1].bw[b] - kVowels[v0].bw[b]) * fr) * bwScale;
                const float db = kVowels[v0].db[b]  + (kVowels[v0 + 1].db[b] - kVowels[v0].db[b]) * fr;
                const float fc = clampf(f, 60.f, 0.45f * srf);
                fb[b].k = clampf(bw / fc, 0.04f, 2.f);
                makeSvf(fc, fb[b].k, sr, fb[b].g, fb[b].a1, fb[b].a2, fb[b].a3);
                fbAmp[b] = std::pow(10.f, db / 20.f) * fb[b].k * 2.2f;
            }
        }
        else
        {
            s1.k = 2.f * (1.f - 0.97f * res);
            makeSvf(cutoff, s1.k, sr, s1.g, s1.a1, s1.a2, s1.a3);
            if (filterMode == LP24)
            {
                s2.k = 1.2f;
                makeSvf(cutoff, s2.k, sr, s2.g, s2.a1, s2.a2, s2.a3);
            }
        }

        const float ampMod = std::max(0.f, 1.f + d[DstAmp]);

        // ---- Layer B: Osc 2 goes through its own 12 dB low-pass and its own envelope ----
        SvfCoef sl;
        if (layerOn)
        {
            const float lc = clampf(P(layCut) * std::exp2(0.4f * (v.noteCur - 60.f) / 12.f + brightOct + mv[0] * 0.6f),
                                    40.f, std::min(18000.f, 0.45f * srf));
            sl.k = 1.5f;
            makeSvf(lc, sl.k, sr, sl.g, sl.a1, sl.a2, sl.a3);
        }

        // ---- sample loop ----
        for (int s = 0; s < n; ++s)
        {
            const float ampE = v.ampEnv.tick(ampC);
            v.fltEnv.tick(fltC);

            float fmv = 0.f;
            if (fmIdx > 0.0001f)
            {
                v.fmPh += fmInc; if (v.fmPh >= 1.f) v.fmPh -= 1.f;
                fmv = readTable(tables->sineLut.data(), v.fmPh) * fmIdx;
            }

            float l = 0.f, r = 0.f, ll = 0.f, lr = 0.f;
            if (c1.level > 0.0001f)
            {
                float al = 0.f, ar = 0.f;
                for (int k = 0; k < c1.n; ++k)
                {
                    if (syncOn)
                    {
                        v.mph[k] += c2.minc[k];
                        if (v.mph[k] >= 1.f) { v.mph[k] -= 1.f; v.ph1[k] = wrap01(v.mph[k] * (c1.inc[k] / std::max(1e-9f, c2.minc[k]))); }
                    }
                    float rp = v.ph1[k];
                    if (fmv != 0.f) rp = wrap01(rp + fmv);
                    const float o = oscRead(c1.wave, c1.t0, c1.t1, c1.fr, c1.pw, c1.pwGain, rp);
                    al += o * c1.gL[k]; ar += o * c1.gR[k];
                    v.ph1[k] += c1.inc[k]; if (v.ph1[k] >= 1.f) v.ph1[k] -= 1.f;
                }
                l += al * c1.level; r += ar * c1.level;
            }
            if (c2.level > 0.0001f)
            {
                float al = 0.f, ar = 0.f;
                for (int k = 0; k < c2.n; ++k)
                {
                    const float o = oscRead(c2.wave, c2.t0, c2.t1, c2.fr, c2.pw, c2.pwGain, v.ph2[k]);
                    al += o * c2.gL[k]; ar += o * c2.gR[k];
                    v.ph2[k] += c2.inc[k]; if (v.ph2[k] >= 1.f) v.ph2[k] -= 1.f;
                }
                if (layerOn) { ll += al * c2.level; lr += ar * c2.level; }
                else         { l += al * c2.level;  r += ar * c2.level; }
            }
            if (subLvl > 0.0001f)
            {
                const float sv = readTable(subT, v.subPh) * subLvl;
                v.subPh += subInc; if (v.subPh >= 1.f) v.subPh -= 1.f;
                l += sv; r += sv;
            }
            if (noiseLvl > 0.0001f)
            {
                v.rng = v.rng * 1664525u + 1013904223u;
                const float white = (float) (v.rng >> 8) * (2.f / 16777216.f) - 1.f;
                v.noiseLP += (white - v.noiseLP) * noiseCoef;
                const float nv = v.noiseLP * noiseGain * noiseLvl;
                l += nv; r += nv;
            }
            l *= 0.5f; r *= 0.5f;

            // ---- filter (L and R) ----
            float yl = l, yr = r;
            if (drive > 0.0005f)
            {
                yl = std::tanh(yl * preGain) * postGain;
                yr = std::tanh(yr * preGain) * postGain;
            }
            for (int ch = 0; ch < 2; ++ch)
            {
                float x = ch == 0 ? yl : yr, y = 0.f;
                if (filterMode == Formant)
                {
                    for (int b = 0; b < 3; ++b)
                    {
                        SVF& st = v.fm[ch][b];
                        const SvfCoef& sc = fb[b];
                        const float v3 = x - st.ic2;
                        const float v1 = sc.a1 * st.ic1 + sc.a2 * v3;
                        const float v2 = st.ic2 + sc.a2 * st.ic1 + sc.a3 * v3;
                        st.ic1 = 2.f * v1 - st.ic1; st.ic2 = 2.f * v2 - st.ic2;
                        y += v1 * fbAmp[b];
                    }
                }
                else
                {
                    SVF& st = v.f1[ch];
                    const float v3 = x - st.ic2;
                    const float v1 = s1.a1 * st.ic1 + s1.a2 * v3;
                    const float v2 = st.ic2 + s1.a2 * st.ic1 + s1.a3 * v3;
                    st.ic1 = 2.f * v1 - st.ic1; st.ic2 = 2.f * v2 - st.ic2;
                    switch (filterMode)
                    {
                        case LP12: y = v2; break;
                        case LP24:
                        {
                            SVF& st2 = v.f2[ch];
                            const float w3 = v2 - st2.ic2;
                            const float w1 = s2.a1 * st2.ic1 + s2.a2 * w3;
                            const float w2 = st2.ic2 + s2.a2 * st2.ic1 + s2.a3 * w3;
                            st2.ic1 = 2.f * w1 - st2.ic1; st2.ic2 = 2.f * w2 - st2.ic2;
                            y = w2;
                            break;
                        }
                        case HP12:  y = x - s1.k * v1 - v2; break;
                        case BP12:  y = v1 * s1.k; break;
                        default:    y = x - s1.k * v1; break; // Notch
                    }
                }
                if (ch == 0) yl = y; else yr = y;
            }

            const float g = ampE * velGain * ampMod;
            float oL = yl * g, oR = yr * g;
            if (layerOn)
            {
                const float layE = v.layEnv.tick(layC) * velGain * 0.5f;
                for (int ch = 0; ch < 2; ++ch)
                {
                    SVF& st = v.ly[ch];
                    const float x = ch == 0 ? ll : lr;
                    const float v3 = x - st.ic2;
                    const float v1 = sl.a1 * st.ic1 + sl.a2 * v3;
                    const float v2 = st.ic2 + sl.a2 * st.ic1 + sl.a3 * v3;
                    st.ic1 = 2.f * v1 - st.ic1; st.ic2 = 2.f * v2 - st.ic2;
                    (ch == 0 ? oL : oR) += v2 * layE;
                }
            }
            outL[done + s] += oL;
            if (outR) outR[done + s] += oR;
        }

        done += n;
        if (!v.ampEnv.active() && !(layerOn && v.layEnv.active())) { v.active = false; v.gate = false; v.sustained = false; break; }
    }
}
} // namespace aura
