// AuraSynth offline QA tool.  Drives the real AuraProcessor (processBlock) with MIDI, no audio device needed.
//   AuraRenderTest                 run the whole test-suite
//   AuraRenderTest --calibrate f   (re)generate loudness-matched master gains into f (Source/PresetGains.inc)
//   AuraRenderTest --wav dir       render every factory preset to a WAV file
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <chrono>
#include <cmath>
#include <map>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using namespace aura;

namespace
{
int gFailures = 0, gChecks = 0;
void check(bool ok, const juce::String& what)
{
    ++gChecks;
    if (!ok) { ++gFailures; std::cout << "  [FAIL] " << what << std::endl; }
}
void info(const juce::String& s) { std::cout << s << std::endl; }

struct Evt { int sample; juce::MidiMessage msg; };
Evt noteOn(double t, int n, int vel, double sr)  { return { (int) (t * sr), juce::MidiMessage::noteOn(1, n, (juce::uint8) vel) }; }
Evt noteOff(double t, int n, double sr)          { return { (int) (t * sr), juce::MidiMessage::noteOff(1, n) }; }
Evt cc(double t, int c, int v, double sr)        { return { (int) (t * sr), juce::MidiMessage::controllerEvent(1, c, v) }; }
Evt bend(double t, int v, double sr)             { return { (int) (t * sr), juce::MidiMessage::pitchWheel(1, v) }; }

struct Stats { float peak = 0, rms = 0; bool bad = false; };
Stats stats(const juce::AudioBuffer<float>& b, int from = 0, int to = -1)
{
    if (to < 0) to = b.getNumSamples();
    Stats s; double acc = 0; long cnt = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = from; i < to; ++i)
        {
            const float x = b.getSample(c, i);
            if (!std::isfinite(x)) { s.bad = true; continue; }
            s.peak = std::max(s.peak, std::abs(x));
            acc += (double) x * x; ++cnt;
        }
    s.rms = cnt ? (float) std::sqrt(acc / (double) cnt) : 0.f;
    return s;
}

struct Rig
{
    AuraProcessor proc;
    double sr;
    explicit Rig(double sampleRate = 44100.0) : sr(sampleRate)
    {
        proc.setPlayConfigDetails(0, 2, sr, 512);
        proc.prepareToPlay(sr, 512);
    }
    void setParam(const juce::String& id, float realValue)
    {
        auto* p = proc.apvts.getParameter(id);
        jassert(p != nullptr);
        if (p) p->setValueNotifyingHost(p->convertTo0to1(realValue));
    }
    float getParam(const juce::String& id) { return proc.apvts.getRawParameterValue(id)->load(); }
    void loadDefaults()
    {
        for (int i = 0; i < kNumParams; ++i) setParam(paramDef(i).id, paramDefault(i));
    }
    void apply(const std::vector<float>& v)
    {
        for (int i = 0; i < kNumParams; ++i) setParam(paramDef(i).id, v[(size_t) i]);
    }
    juce::AudioBuffer<float> render(double seconds, std::vector<Evt> evts, double bpm = 120.0)
    {
        (void) bpm;
        proc.prepareToPlay(sr, 512);
        const int total = (int) std::lround(seconds * sr);
        juce::AudioBuffer<float> out(2, total);
        out.clear();
        for (int pos = 0; pos < total; pos += 512)
        {
            const int n = std::min(512, total - pos);
            juce::AudioBuffer<float> blk(2, n);
            blk.clear();
            juce::MidiBuffer mb;
            for (auto& e : evts)
                if (e.sample >= pos && e.sample < pos + n) mb.addEvent(e.msg, e.sample - pos);
            proc.processBlock(blk, mb);
            for (int c = 0; c < 2; ++c) out.copyFrom(c, pos, blk, c, 0, n);
        }
        return out;
    }
};

float relDiff(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    double num = 0, den = 0;
    const int n = std::min(a.getNumSamples(), b.getNumSamples());
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < n; ++i)
        {
            const double d = (double) a.getSample(c, i) - (double) b.getSample(c, i);
            num += d * d; den += (double) a.getSample(c, i) * a.getSample(c, i);
        }
    return den > 1e-12 ? (float) std::sqrt(num / den) : (num > 1e-12 ? 1.f : 0.f);
}

// dominant frequency by zero crossings of channel 0 over a window (clean periodic signals only)
double zeroCrossFreq(const juce::AudioBuffer<float>& b, int from, int to, double sr)
{
    int crossings = 0;
    for (int i = from + 1; i < to; ++i)
        if (b.getSample(0, i - 1) <= 0.f && b.getSample(0, i) > 0.f) ++crossings;
    return crossings / ((to - from) / sr);
}

std::vector<float> baseValues()
{
    Preset p;
    auto set = [&](const char* id, float v) { p.values.emplace_back(indexOfParam(id), v); };
    set("o1Wave", Saw); set("o1Level", 0.7f); set("o1Uni", 3); set("o1Det", 15); set("o1Spread", 0.6f);
    set("o2Wave", Square); set("o2Level", 0.4f); set("o2Uni", 2); set("o2Det", 12); set("o2Spread", 0.6f);
    set("subLevel", 0.25f); set("nLevel", 0.12f);
    set("fType", LP24); set("fCutoff", 2500); set("fRes", 0.3f); set("fEnv", 0.4f); set("fKey", 0.3f);
    set("ampA", 0.005f); set("ampD", 0.3f); set("ampS", 0.8f); set("ampR", 0.25f);
    set("fltA", 0.005f); set("fltD", 0.3f); set("fltS", 0.4f); set("fltR", 0.3f);
    return resolveValues(p);
}

using Ctx = std::vector<std::pair<const char*, float>>;

// preconditions that make a parameter audible in the test patch
Ctx contextFor(const juce::String& id)
{
    auto ends = [&](const char* s) { return id.endsWith(s); };
    if (id == "o1WT") return { { "o1Wave", Wavetable } };
    if (id == "o2WT") return { { "o2Wave", Wavetable } };
    if (id == "o1PW") return { { "o1Wave", Pulse } };
    if (id == "o2PW") return { { "o2Wave", Pulse } };
    if (id == "o1Det" || id == "o1Spread") return { { "o1Uni", 5 } };
    if (id == "o2Det" || id == "o2Spread") return { { "o2Uni", 5 }, { "o2Level", 0.6f } };
    if (id == "o1Sync") return { { "o2Semi", 7 } };
    if (id.startsWith("o2")) return { { "o2Level", 0.6f } };
    if (id == "fmRatio") return { { "fmAmt", 0.5f } };
    if (id == "subWave" || id == "subOct") return { { "subLevel", 0.6f } };
    if (id == "nTone") return { { "nLevel", 0.5f } };
    if (id == "fVowel") return { { "fType", Formant } };
    if (id == "ampD") return { { "ampS", 0.2f } };
    if (id.startsWith("flt")) return { { "fEnv", 0.9f }, { "fCutoff", 800 } };
    if (id == "l1Shape" || id == "l1Rate") return { { "m1Src", SrcLfo1 }, { "m1Dst", DstCutoff }, { "m1Amt", 0.6f } };
    if (id == "l2Shape" || id == "l2Rate") return { { "m1Src", SrcLfo2 }, { "m1Dst", DstCutoff }, { "m1Amt", 0.6f } };
    if (id == "l3Shape" || id == "l3Rate") return { { "m1Src", SrcLfo3 }, { "m1Dst", DstCutoff }, { "m1Amt", 0.6f } };
    if (id == "arpRate" || id == "arpPat" || id == "arpOct" || id == "arpGate") return { { "arpOn", 1 } };
    if (id.startsWith("lay") && id != "layOn") return { { "layOn", 1 }, { "o2Level", 0.6f } };
    if (id == "layOn") return { { "o2Level", 0.6f } };
    if (id == "bcBits" || id == "bcRate") return { { "bcMix", 0.8f } };
    if (id == "dsType" || id == "dsDrive") return { { "dsMix", 0.8f } };
    if (id == "chRate" || id == "chDepth") return { { "chMix", 0.8f } };
    if (id == "phRate" || id == "phDepth" || id == "phFb") return { { "phMix", 0.8f } };
    if (id == "flRate" || id == "flDepth" || id == "flFb") return { { "flMix", 0.8f } };
    if (id == "eqMidFreq") return { { "eqMid", 8.f } };
    if (id == "cpThresh") return { { "cpRatio", 8.f } };
    if (id == "cpRatio") return { { "cpThresh", -30.f } };
    if (id == "cpRel") return { { "cpThresh", -30.f }, { "cpRatio", 8.f } };
    if (id.startsWith("dl") && id != "dlMix") return { { "dlMix", 0.6f }, { "dlFb", 0.5f } };
    if (id.startsWith("rv") && id != "rvMix") return { { "rvMix", 0.6f } };
    (void) ends;
    return {};
}

std::vector<Evt> testNotes(const juce::String& id, double sr)
{
    if (id.startsWith("arp") || id == "vMode")
        return { noteOn(0.0, 60, 100, sr), noteOn(0.0, 64, 100, sr), noteOn(0.0, 67, 100, sr), noteOff(1.6, 60, sr), noteOff(1.6, 64, sr), noteOff(1.6, 67, sr) };
    std::vector<Evt> e { noteOn(0.0, 72, 100, sr), noteOff(0.5, 72, sr), noteOn(0.5, 79, 100, sr), noteOff(1.0, 79, sr) };
    if (id == "vBend") e.push_back(bend(0.2, 16383, sr));
    if (id == "o1Sync") e = { noteOn(0.0, 60, 100, sr), noteOff(1.0, 60, sr) };
    return e;
}

// ------------------------------------------------------------------------------------------
void testParameters()
{
    info("\n== Parameter layout ==");
    Rig rig;
    check(rig.proc.getParameters().size() == kNumParams, "parameter count matches table (" + juce::String(kNumParams) + ")");
    juce::StringArray ids;
    bool unique = true;
    for (int i = 0; i < kNumParams; ++i) { if (ids.contains(paramDef(i).id)) unique = false; ids.add(paramDef(i).id); }
    check(unique, "all parameter ids unique");
    int automatable = 0;
    for (auto* p : rig.proc.getParameters()) if (p->isAutomatable()) ++automatable;
    check(automatable == kNumParams, "every parameter is automatable by the host");
    info("  parameters: " + juce::String(kNumParams));
}

void testMidi()
{
    info("\n== MIDI / voice behaviour ==");
    Rig rig;
    const double sr = rig.sr;

    // silence without notes
    rig.loadDefaults();
    auto silent = rig.render(0.5, {});
    check(stats(silent).peak == 0.f, "no notes -> exact silence");

    // note triggers sound, frequency is correct (pure sine through open filter)
    rig.loadDefaults();
    rig.setParam("o1Wave", Sine); rig.setParam("fType", LP12); rig.setParam("fCutoff", 20000); rig.setParam("fEnv", 0);
    rig.setParam("ampS", 1); rig.setParam("fKey", 0); rig.setParam("master", 0.7f);
    auto a4 = rig.render(1.0, { noteOn(0.0, 69, 127, sr) });
    check(stats(a4, 4410, 40000).rms > 0.05f, "note-on produces audio");
    const double f = zeroCrossFreq(a4, 8000, 40000, sr);
    check(std::abs(f - 440.0) < 3.0, "A4 plays at 440 Hz (measured " + juce::String(f, 2) + ")");

    auto c3 = rig.render(1.0, { noteOn(0.0, 48, 127, sr) });
    const double f2 = zeroCrossFreq(c3, 8000, 40000, sr);
    check(std::abs(f2 - 130.81) < 1.5, "C3 plays at 130.8 Hz (measured " + juce::String(f2, 2) + ")");

    // octave / semi / fine
    rig.setParam("o1Oct", 1);
    auto up = rig.render(1.0, { noteOn(0.0, 69, 127, sr) });
    check(std::abs(zeroCrossFreq(up, 8000, 40000, sr) - 880.0) < 6.0, "octave +1 doubles the frequency");
    rig.setParam("o1Oct", 0); rig.setParam("o1Semi", 7);
    auto sem = rig.render(1.0, { noteOn(0.0, 69, 127, sr) });
    check(std::abs(zeroCrossFreq(sem, 8000, 40000, sr) - 659.26) < 5.0, "semi +7 -> E5");
    rig.setParam("o1Semi", 0);

    // pitch bend: +2 semitones at full wheel
    auto pb = rig.render(1.5, { noteOn(0.0, 69, 127, sr), bend(0.5, 16383, sr) });
    check(std::abs(zeroCrossFreq(pb, (int) (0.8 * sr), (int) (1.4 * sr), sr) - 493.88) < 4.0, "pitch bend +2 st (493.9 Hz)");
    rig.setParam("vBend", 12);
    auto pb12 = rig.render(1.5, { noteOn(0.0, 69, 127, sr), bend(0.5, 0, sr) });
    check(std::abs(zeroCrossFreq(pb12, (int) (0.8 * sr), (int) (1.4 * sr), sr) - 220.0) < 3.0, "pitch bend range 12 st down -> 220 Hz");
    rig.setParam("vBend", 2);

    // note-off -> release -> silence
    rig.setParam("ampR", 0.2f);
    auto rel = rig.render(2.0, { noteOn(0.0, 69, 127, sr), noteOff(0.5, 69, sr) });
    check(stats(rel, (int) (0.4 * sr), (int) (0.5 * sr)).rms > 0.05f, "note sustains while held");
    check(stats(rel, (int) (1.5 * sr), (int) (2.0 * sr)).rms < 1e-4f, "note is silent after release");

    // sustain pedal holds the note
    auto ped = rig.render(2.5, { cc(0.0, 64, 127, sr), noteOn(0.1, 69, 127, sr), noteOff(0.5, 69, sr), cc(1.5, 64, 0, sr) });
    check(stats(ped, (int) (0.8 * sr), (int) (1.4 * sr)).rms > 0.05f, "sustain pedal keeps the note sounding");
    check(stats(ped, (int) (2.2 * sr), (int) (2.5 * sr)).rms < 1e-4f, "note releases when pedal is lifted");

    // velocity sensitivity
    rig.setParam("vVelSens", 1.0f);
    auto loud = rig.render(0.6, { noteOn(0.0, 69, 127, sr) });
    auto soft = rig.render(0.6, { noteOn(0.0, 69, 32, sr) });
    const float ratio = stats(soft, 8000, 20000).rms / std::max(1e-6f, stats(loud, 8000, 20000).rms);
    check(ratio > 0.2f && ratio < 0.3f, "velocity sensitivity scales level (ratio " + juce::String(ratio, 3) + ")");
    rig.setParam("vVelSens", 0.0f);
    auto soft0 = rig.render(0.6, { noteOn(0.0, 69, 32, sr) });
    const float ratio0 = stats(soft0, 8000, 20000).rms / std::max(1e-6f, stats(loud, 8000, 20000).rms);
    check(std::abs(ratio0 - 1.f) < 0.02f, "velocity sensitivity 0 -> constant level");
    rig.setParam("vVelSens", 0.6f);

    // polyphony: 3 notes louder than 1; 16+ notes do not blow up
    auto one = rig.render(0.6, { noteOn(0.0, 60, 100, sr) });
    auto three = rig.render(0.6, { noteOn(0.0, 60, 100, sr), noteOn(0.0, 64, 100, sr), noteOn(0.0, 67, 100, sr) });
    check(stats(three, 8000, 20000).rms > stats(one, 8000, 20000).rms * 1.3f, "polyphonic: chord is louder than a single note");
    std::vector<Evt> many;
    for (int i = 0; i < 24; ++i) many.push_back(noteOn(0.01 * i, 36 + i * 2, 120, sr));
    auto big = rig.render(1.5, many);
    check(!stats(big).bad && stats(big).peak <= 1.0f, "24 simultaneous notes (voice stealing) stay finite and <= 0 dBFS");

    // mono mode plays one note at a time; legato does not retrigger the envelope
    rig.setParam("vMode", Mono); rig.setParam("ampA", 0.001f);
    auto mono = rig.render(1.0, { noteOn(0.0, 60, 100, sr), noteOn(0.0, 64, 100, sr), noteOn(0.0, 67, 100, sr) });
    check(std::abs(zeroCrossFreq(mono, 8000, 40000, sr) - 392.0) < 4.0, "mono mode plays only the last note (G4 = 392 Hz)");
    rig.setParam("vMode", Poly);
    rig.setParam("o1Oct", -1);

    // glide: after the second note the pitch is in between for a while
    rig.setParam("o1Oct", 0); rig.setParam("vMode", Legato); rig.setParam("vGlide", 0.5f);
    auto gl = rig.render(2.0, { noteOn(0.0, 60, 100, sr), noteOn(0.6, 72, 100, sr) });
    const double fa = zeroCrossFreq(gl, (int) (0.4 * sr), (int) (0.55 * sr), sr);
    const double fb = zeroCrossFreq(gl, (int) (0.75 * sr), (int) (0.85 * sr), sr);
    const double fc = zeroCrossFreq(gl, (int) (1.8 * sr), (int) (1.95 * sr), sr);
    check(fa < 270.0 && fb > fa * 1.05 && fb < fc * 0.97 && std::abs(fc - 523.25) < 12.0,
          "legato glide sweeps C4 -> C5 (" + juce::String(fa, 0) + " / " + juce::String(fb, 0) + " / " + juce::String(fc, 0) + " Hz)");
    rig.setParam("vMode", Poly); rig.setParam("vGlide", 0.f);

    // all-notes-off message
    auto off = rig.render(1.5, { noteOn(0.0, 60, 100, sr), { (int) (0.5 * sr), juce::MidiMessage::allNotesOff(1) } });
    check(stats(off, (int) (1.2 * sr), (int) (1.5 * sr)).rms < 1e-4f, "all-notes-off silences held notes");

    // arpeggiator timing: 1/8 notes at 120 bpm = 4 steps per second -> count onsets via envelope
    rig.loadDefaults();
    rig.setParam("o1Wave", Sine); rig.setParam("fType", LP12); rig.setParam("fCutoff", 20000); rig.setParam("fEnv", 0); rig.setParam("fKey", 0);
    rig.setParam("ampA", 0.001f); rig.setParam("ampD", 0.05f); rig.setParam("ampS", 0.0f); rig.setParam("ampR", 0.02f);
    rig.setParam("arpOn", 1); rig.setParam("arpRate", 1); rig.setParam("arpGate", 0.5f);
    auto arp = rig.render(3.0, { noteOn(0.0, 60, 100, sr), noteOn(0.0, 64, 100, sr), noteOn(0.0, 67, 100, sr) });
    int onsets = 0; bool inNote = false;
    const int win = (int) (0.005 * sr);
    for (int i = 0; i + win < arp.getNumSamples(); i += win)
    {
        const float e = stats(arp, i, i + win).rms;
        if (!inNote && e > 0.03f) { ++onsets; inNote = true; }
        else if (inNote && e < 0.004f) inNote = false;
    }
    check(onsets >= 11 && onsets <= 13, "arpeggiator: 1/8 at 120 bpm gives ~12 notes in 3 s (counted " + juce::String(onsets) + ")");
}

// each parameter must change the sound when changed
void testParametersAffectAudio()
{
    info("\n== Every parameter changes the audio ==");
    Rig rig;
    const double sr = rig.sr;
    const auto base = baseValues();
    int ok = 0;
    for (int i = 0; i < kNumParams; ++i)
    {
        const juce::String id = paramDef(i).id;
        auto vA = base;
        // contexts
        auto applyCtx = [&](std::vector<float>& v) {
            for (auto& kv : contextFor(id)) v[(size_t) indexOfParam(kv.first)] = kv.second;
            if (id.length() == 5 && id[0] == 'm' && juce::CharacterFunctions::isDigit(id[1]) && id != "master")
            {
                const juce::String n = id.substring(1, 2);
                v[(size_t) indexOfParam("m" + n + "Src")] = SrcLfo1;
                v[(size_t) indexOfParam("m" + n + "Dst")] = DstCutoff;
                v[(size_t) indexOfParam("m" + n + "Amt")] = 0.6f;
            }
            if (id == "vBend" || id == "vMode") {}
        };
        applyCtx(vA);
        auto vB = vA;
        const auto& d = paramDef(i);
        float alt;
        switch (d.type)
        {
            case ParamType::Bool:   alt = vA[(size_t) i] > 0.5f ? 0.f : 1.f; break;
            case ParamType::Choice:
            {
                const int n = juce::StringArray::fromTokens(d.choices, "|", "").size();
                alt = (float) (((int) vA[(size_t) i] + (id.endsWith("Dst") ? 1 : (id.endsWith("Src") ? 5 : 1))) % n);
                if (id == "fType") alt = HP12;
                if (id == "arpPat") alt = 1;
                if (id == "dlSync") alt = 2;
                if (id == "vMode") alt = Mono;
                break;
            }
            case ParamType::Int:
                alt = vA[(size_t) i] + (vA[(size_t) i] + 1 <= d.maxV ? 1.f : -1.f);
                if (id == "o1Oct" || id == "o2Oct") alt = vA[(size_t) i] + 1;
                if (id == "o1Uni" || id == "o2Uni") alt = std::min(d.maxV, vA[(size_t) i] + 3);
                if (id == "vBend") alt = 12;
                if (id == "arpOct") alt = 3;
                break;
            default:
            {
                const float mid = 0.5f * (d.minV + d.maxV);
                alt = vA[(size_t) i] < mid ? d.minV + 0.85f * (d.maxV - d.minV) : d.minV + 0.15f * (d.maxV - d.minV);
                if (id == "stWidth") alt = 0.f;
                if (id == "master") alt = 0.3f;
                if (id == "bcBits") alt = 4;
                if (id == "bcRate") alt = 8;
                break;
            }
        }
        vB[(size_t) i] = alt;
        auto evts = testNotes(id, sr);
        rig.apply(vA); auto a = rig.render(1.6, evts);
        rig.apply(vB); auto b = rig.render(1.6, evts);
        const float diff = relDiff(a, b);
        const bool bad = stats(a).bad || stats(b).bad;
        const bool pass = !bad && diff > 0.01f;
        if (pass) ++ok; else check(false, "parameter '" + id + "' has no audible effect (diff " + juce::String(diff, 4) + (bad ? ", NaN!" : "") + ")");
        ++gChecks;
    }
    info("  " + juce::String(ok) + " / " + juce::String(kNumParams) + " parameters verified audible");
}

// ------------------------------------------------------------------------------------------
double measureHarmonicPurity(Rig& rig, int note, double sr, float* outNoiseDb)
{
    // renders a steady note and measures the energy that is NOT on harmonics of f0 (= aliasing + noise floor)
    const int N = 32768;
    auto buf = rig.render(1.2, { noteOn(0.0, note, 127, sr) });
    juce::dsp::FFT fft(15);
    std::vector<float> data((size_t) N * 2, 0.f);
    const int start = (int) (0.4 * sr);
    for (int i = 0; i < N; ++i)
    {
        const float w = 0.5f - 0.5f * std::cos(2.f * 3.14159265f * (float) i / (float) N);
        data[(size_t) i] = 0.5f * (buf.getSample(0, start + i) + buf.getSample(1, start + i)) * w;
    }
    fft.performFrequencyOnlyForwardTransform(data.data());
    const double f0 = 440.0 * std::pow(2.0, (note - 69) / 12.0);
    const double binHz = sr / N;
    double harm = 0, other = 0;
    for (int k = 2; k < N / 2; ++k)
    {
        const double fk = k * binHz;
        const double nearest = std::round(fk / f0) * f0;
        const bool onHarm = nearest >= f0 * 0.5 && std::abs(fk - nearest) < binHz * 4.0;
        const double p = (double) data[(size_t) k] * data[(size_t) k];
        (onHarm ? harm : other) += p;
    }
    *outNoiseDb = (float) (10.0 * std::log10(other / std::max(1e-20, harm)));
    return f0;
}

void testAliasing()
{
    info("\n== Band-limiting (aliasing) ==");
    Rig rig;
    const double sr = rig.sr;
    for (int wave : { (int) Saw, (int) Square, (int) Triangle, (int) Pulse, (int) Wavetable })
        for (int note : { 84, 96, 105 })
        {
            rig.loadDefaults();
            rig.setParam("o1Wave", (float) wave); rig.setParam("o1WT", 0.5f); rig.setParam("o1PW", 0.3f);
            rig.setParam("fType", LP12); rig.setParam("fCutoff", 20000); rig.setParam("fEnv", 0); rig.setParam("fKey", 0);
            rig.setParam("ampS", 1); rig.setParam("master", 0.5f);
            float db = 0;
            measureHarmonicPurity(rig, note, sr, &db);
            check(db < -45.f, "wave " + juce::String(wave) + " @ MIDI " + juce::String(note) + ": non-harmonic energy " + juce::String(db, 1) + " dB (< -45 dB)");
        }
}

void testPresetLibrary()
{
    info("\n== Preset library ==");
    const auto& all = factoryPresets();
    check(presetBuildErrors().isEmpty(), "all preset parameter ids are valid");
    for (auto& e : presetBuildErrors()) info("  " + e);
    check(all.size() >= 100, "at least 100 factory presets (" + juce::String((int) all.size()) + ")");

    juce::StringArray names;
    bool dupes = false;
    std::map<juce::String, int> perCat;
    for (auto& p : all) { if (names.contains(p.name)) { dupes = true; info("  duplicate name: " + p.name); } names.add(p.name); ++perCat[p.category]; }
    check(!dupes, "preset names are unique");
    {
        // front page: genre buttons / starter sounds / the ten Melodic Trap sounds the user asked for
        int perGenre[kNumGenres] {};
        for (auto& p : all) for (int g = 0; g < kNumGenres; ++g) if (p.genres & (1u << g)) ++perGenre[g];
        for (int g = 0; g < kNumGenres; ++g)
        {
            check(perGenre[g] >= 12, "genre " + genreNames()[g] + " has >= 12 sounds (" + juce::String(perGenre[g]) + ")");
            bool ok = false;
            for (auto& p : all) if (p.name == genreStarterPreset(g) && (p.genres & (1u << g))) ok = true;
            check(ok, "genre starter '" + genreStarterPreset(g) + "' exists and belongs to " + genreNames()[g]);
        }
        check(perGenre[1] < (int) all.size() * 0.4, "Melodic Trap is a curated subset, not nearly everything");
        for (auto nm : { "Plan A Pluck", "Dark Thoughts Lead", "Glass Bounce", "Rubber Keys", "Airy Trap Pad", "Dark Floating Pad",
                         "Soft Digital Bells", "Dreamy Lead", "Warm Background Keys", "Full Melodic Stack" })
        {
            bool found = false;
            for (auto& p : all) if (p.name == nm && (p.genres & GenreTrap)) found = true;
            check(found, juce::String("Melodic Trap preset '") + nm + "' exists and is tagged Melodic Trap");
        }
        for (int k : { GroupPluck, GroupLead, GroupPad, GroupKeys, GroupBell, GroupTexture })
        {
            int n = 0;
            for (auto& p : all) if (p.group == k) ++n;
            check(n >= 8, "sound type '" + groupNames()[k] + "' has >= 8 sounds (" + juce::String(n) + ")");
        }
    }

    check((int) perCat.size() == kNumCategories, "all 20 categories are populated");
    for (auto& c : categoryNames()) check(perCat[c] >= 5, "category '" + c + "' has >= 5 presets (" + juce::String(perCat[c]) + ")");

    // distinctness: no two presets may be (nearly) identical in parameter space
    int closePairs = 0;
    std::vector<std::vector<float>> vals;
    for (auto& p : all) vals.push_back(resolveValues(p));
    for (size_t a = 0; a < vals.size(); ++a)
        for (size_t b = a + 1; b < vals.size(); ++b)
        {
            int diff = 0;
            for (int i = 0; i < kNumParams; ++i)
                if (i != master && std::abs(vals[a][(size_t) i] - vals[b][(size_t) i]) > 1e-4f) ++diff;
            if (diff < 12) { ++closePairs; info("  too similar (" + juce::String(diff) + " params differ): " + all[a].name + " / " + all[b].name); }
        }
    check(closePairs == 0, "presets are genuinely distinct (>= 12 differing parameters between any two)");
    int minSet = 999; for (auto& p : all) minSet = std::min(minSet, (int) p.values.size());
    info("  smallest preset sets " + juce::String(minSet) + " parameters");
}

struct PresetRender { float peak, rms, tailRms; bool bad; float loud; };

PresetRender renderPreset(Rig& rig, const Preset& p, double sr, float masterOverride = -1.f)
{
    auto v = resolveValues(p);
    if (masterOverride >= 0.f) v[(size_t) master] = masterOverride;
    rig.apply(v);
    std::vector<Evt> e;
    if (p.testChord) { for (int n : { p.testNote, p.testNote + 4, p.testNote + 7 }) { e.push_back(noteOn(0.0, n, 100, sr)); e.push_back(noteOff(2.5, n, sr)); } }
    else { e.push_back(noteOn(0.0, p.testNote, 100, sr)); e.push_back(noteOff(2.5, p.testNote, sr)); }
    auto buf = rig.render(8.0, e);
    PresetRender r {};
    const auto all = stats(buf);
    r.peak = all.peak; r.bad = all.bad;
    r.tailRms = stats(buf, (int) (7.5 * sr), (int) (8.0 * sr)).rms;
    // loudest 150 ms RMS window
    const int win = (int) (0.15 * sr);
    float best = 0.f;
    for (int s = 0; s + win < (int) (3.0 * sr); s += win / 4) best = std::max(best, stats(buf, s, s + win).rms);
    r.loud = best; r.rms = stats(buf, 0, (int) (3.0 * sr)).rms;
    return r;
}

void testPresetsRender()
{
    info("\n== Rendering every factory preset ==");
    Rig rig;
    const double sr = rig.sr;
    int silent = 0, loud = 0, unstable = 0, hang = 0;
    float lo = 1e9f, hi = 0.f;
    for (auto& p : factoryPresets())
    {
        const auto r = renderPreset(rig, p, sr);
        if (r.bad) { ++unstable; info("  NaN/Inf: " + p.name); }
        if (r.loud < 0.02f) { ++silent; info("  too quiet (" + juce::String(r.loud, 4) + "): " + p.name); }
        if (r.peak > 1.0001f) { ++loud; info("  clips (" + juce::String(r.peak, 3) + "): " + p.name); }
        if (r.tailRms > 0.004f) { ++hang; info("  tail still audible after 5 s release (" + juce::String(r.tailRms, 4) + "): " + p.name); }
        lo = std::min(lo, r.loud); hi = std::max(hi, r.loud);
    }
    check(unstable == 0, "no preset produces NaN/Inf");
    check(silent == 0, "no preset is silent / near-silent");
    check(loud == 0, "no preset exceeds 0 dBFS");
    check(hang == 0, "no preset has runaway / stuck tails");
    info("  loudness spread (loudest 150 ms RMS): " + juce::String(20 * std::log10(lo), 1) + " dB .. " + juce::String(20 * std::log10(hi), 1) + " dB");
}


// ------------------------------------------------------------------------------------------
//  Sound-design checks: filter types, reverb, macros, layer
// ------------------------------------------------------------------------------------------
std::vector<float> spectrum(const juce::AudioBuffer<float>& b, int start, int order = 15)
{
    const int N = 1 << order;
    juce::dsp::FFT fft(order);
    std::vector<float> data((size_t) N * 2, 0.f);
    for (int i = 0; i < N; ++i)
    {
        const float w = 0.5f - 0.5f * std::cos(2.f * 3.14159265f * (float) i / (float) N);
        data[(size_t) i] = 0.5f * (b.getSample(0, start + i) + b.getSample(1, start + i)) * w;
    }
    fft.performFrequencyOnlyForwardTransform(data.data());
    data.resize((size_t) N / 2);
    return data;
}
double bandDb(const std::vector<float>& sp, double sr, double f0, double f1)
{
    const double binHz = sr / (2.0 * (double) sp.size());
    double e = 0;
    for (size_t k = 1; k < sp.size(); ++k)
    {
        const double f = (double) k * binHz;
        if (f >= f0 && f < f1) e += (double) sp[k] * sp[k];
    }
    return 10.0 * std::log10(std::max(1e-20, e));
}
double centroid(const juce::AudioBuffer<float>& b, int start, double sr)
{
    const auto sp = spectrum(b, start, 13);
    const double binHz = sr / (2.0 * (double) sp.size());
    double num = 0, den = 0;
    for (size_t k = 1; k < sp.size(); ++k) { const double f = (double) k * binHz; if (f < 100.0) continue; const double p = (double) sp[k] * sp[k]; num += p * f; den += p; }
    return den > 0 ? num / den : 0;
}

void testSoundDesign()
{
    info("\n== Sound design: filters, reverb, macros, layer ==");
    Rig rig;
    const double sr = rig.sr;

    // ---- filter types really filter (regression test for the notch-only bug) ----
    auto filt = [&](int type, float cutoff) {
        rig.loadDefaults();
        rig.setParam("o1Wave", Saw); rig.setParam("o1Level", 1.f); rig.setParam("fType", (float) type); rig.setParam("fCutoff", cutoff);
        rig.setParam("fRes", 0.f); rig.setParam("fEnv", 0.f); rig.setParam("fKey", 0.f); rig.setParam("ampS", 1.f); rig.setParam("master", 1.f);
        auto buf = rig.render(1.4, { noteOn(0.0, 57, 127, sr) });          // A3 = 220 Hz saw
        return spectrum(buf, (int) (0.6 * sr));
    };
    const auto open = filt(LP12, 20000.f);
    const auto lp12 = filt(LP12, 1000.f), lp24 = filt(LP24, 1000.f), hp12 = filt(HP12, 1000.f), bp = filt(BP12, 1000.f);
    const double hiOpen = bandDb(open, sr, 6000, 9000), loOpen = bandDb(open, sr, 150, 500);
    const double hiLP12 = bandDb(lp12, sr, 6000, 9000), hiLP24 = bandDb(lp24, sr, 6000, 9000), loLP12 = bandDb(lp12, sr, 150, 500);
    info("  LP12 @1 kHz: highs " + juce::String(hiLP12 - hiOpen, 1) + " dB, lows " + juce::String(loLP12 - loOpen, 1) + " dB | LP24 highs " + juce::String(hiLP24 - hiOpen, 1) + " dB");
    check(hiLP12 - hiOpen < -20.0, "LP12 removes the highs");
    check(std::abs(loLP12 - loOpen) < 3.0, "LP12 leaves the lows alone");
    check(hiLP24 < hiLP12 - 8.0, "LP24 is steeper than LP12");
    check(bandDb(hp12, sr, 150, 500) - loOpen < -15.0, "HP12 removes the lows");
    check(std::abs(bandDb(hp12, sr, 6000, 9000) - hiOpen) < 3.0, "HP12 leaves the highs alone");
    check(bandDb(bp, sr, 150, 500) - loOpen < -6.0 && bandDb(bp, sr, 6000, 9000) - hiOpen < -6.0, "BP12 removes both lows and highs");
    const auto nt = filt(Notch, 880.f);
    check(bandDb(nt, sr, 840, 920) < bandDb(open, sr, 840, 920) - 10.0 && std::abs(bandDb(nt, sr, 6000, 9000) - hiOpen) < 3.0, "Notch only removes the band around the cutoff");

    // ---- reverb: level, tail, stability ----
    auto chordRender = [&](float mix, float size, float damp, double secs, double hold) {
        rig.loadDefaults();
        rig.setParam("o1Wave", Saw); rig.setParam("o1Uni", 1); rig.setParam("fType", LP12); rig.setParam("fCutoff", 3000.f); rig.setParam("fEnv", 0.f);
        rig.setParam("ampS", 1.f); rig.setParam("ampR", 0.05f); rig.setParam("master", 0.7f);
        rig.setParam("rvMix", mix); rig.setParam("rvSize", size); rig.setParam("rvDamp", damp);
        return rig.render(secs, { noteOn(0.0, 60, 100, sr), noteOn(0.0, 64, 100, sr), noteOn(0.0, 67, 100, sr),
                                  noteOff(hold, 60, sr), noteOff(hold, 64, sr), noteOff(hold, 67, sr) });
    };
    for (float size : { 0.2f, 0.5f, 0.8f })
    {
        auto dry = chordRender(0.f, size, 0.5f, 3.0, 5.0), wet = chordRender(1.f, size, 0.5f, 3.0, 5.0);
        double num = 0, den = 0;
        for (int c = 0; c < 2; ++c)
            for (int i = (int) (1.0 * sr); i < (int) (3.0 * sr); ++i)
            { const double d = wet.getSample(c, i) - dry.getSample(c, i); num += d * d; den += (double) dry.getSample(c, i) * dry.getSample(c, i); }
        const double ratio = std::sqrt(num / std::max(1e-12, den));
        info("  reverb size " + juce::String(size, 1) + ": wet/dry level at mix 100% = " + juce::String(ratio, 2));
        check(ratio > 0.25 && ratio < 1.6, "reverb wet level is in a musical range at size " + juce::String(size, 1));
    }
    {
        auto r = chordRender(1.f, 1.f, 0.f, 20.0, 0.3);   // longest, brightest setting
        check(!stats(r).bad && stats(r).peak <= 1.f, "reverb at maximum size stays finite");
        check(stats(r, (int) (16.0 * sr), (int) (20.0 * sr)).rms < 1e-3f, "reverb tail dies out (no runaway) at maximum size");
        auto sh = chordRender(0.5f, 0.15f, 0.5f, 3.0, 0.3);
        check(stats(sh, (int) (1.6 * sr), (int) (2.0 * sr)).rms < stats(sh, (int) (0.2 * sr), (int) (0.3 * sr)).rms * 0.1f, "short reverb is short");
        // low-end control: reverb of a bass note must not add much energy below 100 Hz relative to the dry note
        rig.loadDefaults();
        rig.setParam("o1Wave", Saw); rig.setParam("fType", LP12); rig.setParam("fCutoff", 1500.f); rig.setParam("fEnv", 0.f);
        rig.setParam("ampS", 1.f); rig.setParam("ampR", 0.05f); rig.setParam("rvMix", 1.f); rig.setParam("rvSize", 0.6f);
        auto b = rig.render(5.0, { noteOn(0.0, 36, 100, sr), noteOff(0.5, 36, sr) });
        const auto tail = spectrum(b, (int) (1.5 * sr));
        info("  reverb tail of a C1 bass note: <120 Hz " + juce::String(bandDb(tail, sr, 20, 120), 1) + " dB vs 120-1000 Hz " + juce::String(bandDb(tail, sr, 120, 1000), 1) + " dB");
        check(bandDb(tail, sr, 20, 120) < bandDb(tail, sr, 120, 1000) + 3.0, "reverb tail has controlled low end");
    }

    // ---- macros ----
    auto macroRender = [&](const char* id, float v) {
        rig.loadDefaults();
        rig.setParam("o1Wave", Saw); rig.setParam("o1Uni", 1); rig.setParam("fType", LP24); rig.setParam("fCutoff", 2500.f);
        rig.setParam("fEnv", 0.f); rig.setParam("ampS", 1.f); rig.setParam("master", 0.7f);
        rig.setParam(id, v);
        return rig.render(2.0, { noteOn(0.0, 57, 100, sr) });
    };
    auto highBand = [&](float v) { return bandDb(spectrum(macroRender("bright", v), (int) (1.0 * sr)), sr, 1500, 10000); };
    const double c1 = highBand(0.15f), c2 = highBand(0.5f), c3 = highBand(0.85f);
    info("  Brightness 15/50/85 % -> energy above 1.5 kHz " + juce::String(c1, 1) + " / " + juce::String(c2, 1) + " / " + juce::String(c3, 1) + " dB");
    check(c1 < c2 - 3.0 && c2 < c3 - 3.0, "Brightness macro darkens / brightens the sound");
    {
        auto w0 = macroRender("warmth", 0.f), w1 = macroRender("warmth", 1.f);
        check(relDiff(w0, w1) > 0.05f && !stats(w1).bad && stats(w1).peak <= 1.f, "Warmth macro changes the tone and stays clean");
        check(bandDb(spectrum(w1, (int) (1.0 * sr)), sr, 40, 160) > bandDb(spectrum(w0, (int) (1.0 * sr)), sr, 40, 160) + 0.5, "Warmth adds low-end body");
    }
    {
        // Movement: spectral centroid drifts over time with movement, stays steady without
        auto spread = [&](float amt) {
            double lo = 1e9, hi = 0;
            rig.loadDefaults();
            rig.setParam("o1Wave", Saw); rig.setParam("o1Uni", 1); rig.setParam("fType", LP24); rig.setParam("fCutoff", 1200.f);
            rig.setParam("fEnv", 0.f); rig.setParam("ampS", 1.f); rig.setParam("movement", amt);
            auto b = rig.render(12.0, { noteOn(0.0, 57, 100, sr) });
            for (double t = 1.0; t < 11.5; t += 0.5) { const double c = centroid(b, (int) (t * sr), sr); lo = std::min(lo, c); hi = std::max(hi, c); }
            info("    movement " + juce::String(amt, 1) + ": centroid " + juce::String(lo, 0) + " .. " + juce::String(hi, 0) + " Hz");
            return hi / lo;
        };
        const double still = spread(0.f), moving = spread(1.f);
        info("  Movement 0 % / 100 % -> centroid max/min ratio " + juce::String(still, 3) + " / " + juce::String(moving, 3));
        check(still < 1.03, "Movement = 0 keeps a sustained note stable");
        check(moving > 1.15, "Movement = 100 % makes a sustained note evolve");
    }

    // ---- stereo: width control, real stereo from unison, mono bass ----
    {
        auto sideMid = [&](float width, int note, int wave) {
            rig.loadDefaults();
            rig.setParam("o1Wave", (float) wave); rig.setParam("o1Uni", 5); rig.setParam("o1Spread", 1.f); rig.setParam("o1Detune", 25.f);
            rig.setParam("fType", LP12); rig.setParam("fCutoff", 4000.f); rig.setParam("fEnv", 0.f);
            rig.setParam("ampS", 1.f); rig.setParam("stWidth", width);
            auto b = rig.render(2.0, { noteOn(0.0, note, 100, sr) });
            double m = 0, s = 0;
            for (int i = (int) (0.8 * sr); i < (int) (1.8 * sr); ++i)
            { const double l = b.getSample(0, i), r = b.getSample(1, i); m += (l + r) * (l + r); s += (l - r) * (l - r); }
            return 10.0 * std::log10(std::max(1e-12, s) / std::max(1e-12, m));
        };
        const double w0 = sideMid(0.f, 60, Saw), w1 = sideMid(1.f, 60, Saw), w2 = sideMid(1.8f, 60, Saw);
        info("  side/mid at width 0 / 1.0 / 1.8: " + juce::String(w0, 1) + " / " + juce::String(w1, 1) + " / " + juce::String(w2, 1) + " dB");
        check(w0 < -60.0, "Stereo Width 0 gives exact mono");
        check(w1 > -30.0, "unison voices produce a real stereo image (side/mid > -30 dB)");
        check(w2 > w1 + 3.0, "Stereo Width 1.8 is wider than 1.0");
        const double bass = sideMid(1.8f, 31, Sine), mid60 = sideMid(1.8f, 72, Sine);   // G0 ~ 49 Hz sine vs a C5 sine
        info("  side/mid of a very low note at width 1.8: " + juce::String(bass, 1) + " dB (C5 sine: " + juce::String(mid60, 1) + " dB)");
        check(bass < mid60 - 10.0, "bass stays (nearly) mono even with the width wide open");
    }

    // ---- layer: pluck on the main path + slow pad on layer B ----
    {
        rig.loadDefaults();
        rig.setParam("o1Wave", Saw); rig.setParam("fType", LP12); rig.setParam("fCutoff", 3000.f); rig.setParam("fEnv", 0.f);
        rig.setParam("ampA", 0.001f); rig.setParam("ampD", 0.15f); rig.setParam("ampS", 0.f); rig.setParam("ampR", 0.1f);
        rig.setParam("o2Wave", Triangle); rig.setParam("o2Level", 0.8f);
        rig.setParam("layOn", 1); rig.setParam("layA", 0.4f); rig.setParam("layS", 0.9f); rig.setParam("layR", 0.8f);
        auto on = rig.render(4.0, { noteOn(0.0, 60, 100, sr), noteOff(2.0, 60, sr) });
        rig.setParam("layOn", 0);
        auto off = rig.render(4.0, { noteOn(0.0, 60, 100, sr), noteOff(2.0, 60, sr) });
        check(stats(on, (int) (1.2 * sr), (int) (1.8 * sr)).rms > 0.02f, "Layer B keeps sounding while the main pluck has died away");
        check(stats(off, (int) (1.2 * sr), (int) (1.8 * sr)).rms < 1e-3f, "(without the layer the pluck is gone)");
        info("  layer release: " + juce::String(stats(on, (int) (2.05 * sr), (int) (2.2 * sr)).rms, 4) + " just after note-off, " + juce::String(stats(on, (int) (3.6 * sr), (int) (4.0 * sr)).rms, 6) + " at the end");
        check(stats(on, (int) (2.05 * sr), (int) (2.2 * sr)).rms > 0.003f && stats(on, (int) (3.6 * sr), (int) (4.0 * sr)).rms < 1e-3f, "Layer B releases with its own release time and the voice ends");
        check(stats(on, 0, (int) (0.05 * sr)).rms > stats(on, (int) (0.05 * sr), (int) (0.1 * sr)).rms * 0.3f, "(pluck attack is still there)");
    }
}

void testFuzz()
{
    info("\n== Fuzz: random patches + random MIDI ==");
    Rig rig;
    const double sr = rig.sr;
    juce::Random rnd(2026);
    int bad = 0, over = 0;
    for (int iter = 0; iter < 150; ++iter)
    {
        std::vector<float> v((size_t) kNumParams);
        for (int i = 0; i < kNumParams; ++i)
        {
            const auto& d = paramDef(i);
            const float r = rnd.nextFloat();
            switch (d.type)
            {
                case ParamType::Choice: v[(size_t) i] = (float) rnd.nextInt(juce::StringArray::fromTokens(d.choices, "|", "").size()); break;
                case ParamType::Bool:   v[(size_t) i] = rnd.nextBool() ? 1.f : 0.f; break;
                case ParamType::Int:    v[(size_t) i] = (float) (d.minV + rnd.nextInt((int) (d.maxV - d.minV) + 1)); break;
                default:                v[(size_t) i] = d.minV + r * (d.maxV - d.minV); break;
            }
        }
        rig.apply(v);
        std::vector<Evt> e;
        for (int k = 0; k < 12; ++k)
        {
            const int n = 24 + rnd.nextInt(80);
            const double t0 = rnd.nextDouble() * 2.0;
            e.push_back(noteOn(t0, n, 1 + rnd.nextInt(127), sr));
            e.push_back(noteOff(t0 + 0.05 + rnd.nextDouble() * 1.5, n, sr));
        }
        if (rnd.nextInt(3) == 0) e.push_back(bend(0.5, rnd.nextInt(16384), sr));
        if (rnd.nextInt(3) == 0) e.push_back(cc(0.3, 1, rnd.nextInt(128), sr));
        std::stable_sort(e.begin(), e.end(), [](const Evt& a, const Evt& b) { return a.sample < b.sample; });
        auto buf = rig.render(4.0, e);
        const auto s = stats(buf);
        if (s.bad) { ++bad; info("  NaN/Inf in random patch #" + juce::String(iter)); }
        if (s.peak > 1.0001f) ++over;
    }
    check(bad == 0, "150 random patches: no NaN/Inf");
    check(over == 0, "150 random patches: output never exceeds 0 dBFS (safety limiter)");
}

void testState()
{
    info("\n== State save / restore, preset save / load ==");
    Rig a, b;
    a.loadDefaults();
    a.setParam("fCutoff", 1234.f); a.setParam("o1Wave", Wavetable); a.setParam("dlMix", 0.4f); a.setParam("m3Dst", DstFM); a.setParam("m3Amt", -0.7f);
    a.proc.presets.load(10);
    a.setParam("fCutoff", 1234.f);
    juce::MemoryBlock mb;
    a.proc.getStateInformation(mb);
    b.proc.setStateInformation(mb.getData(), (int) mb.getSize());
    check(std::abs(b.getParam("fCutoff") - 1234.f) < 0.5f, "state restore: cutoff");
    check((int) b.getParam("o1Wave") == (int) a.getParam("o1Wave"), "state restore: waveform");
    check(b.proc.presets.currentName() == a.proc.presets.currentName() && b.proc.presets.currentName().isNotEmpty(), "state restore: preset name");
    bool same = true;
    for (int i = 0; i < kNumParams; ++i) if (std::abs(a.getParam(paramDef(i).id) - b.getParam(paramDef(i).id)) > 1e-3f) { same = false; info("  differs: " + juce::String(paramDef(i).id)); }
    check(same, "state restore: all " + juce::String(kNumParams) + " parameters identical");

    // factory presets load
    Rig c;
    const auto& pr = c.proc.presets;
    check(pr.numFactory() == (int) factoryPresets().size(), "preset manager exposes the whole factory bank");
    c.proc.presets.load(5);
    const auto expected = resolveValues(factoryPresets()[5]);
    bool match = true;
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& d = paramDef(i);
        float e = expected[(size_t) i];
        const float got = c.getParam(d.id);
        float tol = d.type == ParamType::Float ? (d.maxV - d.minV) * 2e-3f : 0.01f;
        if (std::abs(got - e) > tol) { match = false; info(juce::String("  mismatch ") + d.id + " expected " + juce::String(e) + " got " + juce::String(got)); }
    }
    check(match, "loading a factory preset sets every parameter");

    // search / filter / favourites
    check(!pr.filter("glass", "").empty(), "search finds 'glass'");
    check(pr.filter("zzzzqq", "").empty(), "search with no hit returns nothing");
    check((int) pr.filter("", categoryNames()[17]).size() == 6, "category filter returns the category");
    c.proc.presets.toggleFavorite(3);
    check(pr.isFavorite(3) && pr.filter("", "Favorites").size() >= 1, "favourites toggle + filter");
    c.proc.presets.toggleFavorite(3);
    check(!pr.isFavorite(3), "favourite can be removed again");

    // user preset save / load round trip
    c.proc.presets.load(20);
    c.setParam("fCutoff", 777.f);
    const juce::String name = "ZZ Test Preset " + juce::String(juce::Random::getSystemRandom().nextInt(100000));
    check(c.proc.presets.saveUserPreset(name, "User"), "user preset saves");
    auto file = c.proc.presets.userPresetDirectory().getChildFile(juce::File::createLegalFileName(name) + ".aurapreset");
    check(file.existsAsFile(), "user preset file exists on disk");
    c.proc.presets.load(1);
    check(std::abs(c.getParam("fCutoff") - 777.f) > 1.f, "(sanity) other preset changed the cutoff");
    check(c.proc.presets.loadPresetFile(file), "user preset loads from file");
    check(std::abs(c.getParam("fCutoff") - 777.f) < 1.f, "user preset restores its parameters");
    check(!c.proc.presets.filter(name, "User").empty(), "user preset appears in the browser");
    file.deleteFile();

    // host program interface
    check(c.proc.getNumPrograms() == (int) factoryPresets().size(), "host program list = factory presets");
    c.proc.setCurrentProgram(7);
    check(c.proc.getProgramName(7) == factoryPresets()[7].name && c.proc.presets.currentIndex() == 7, "setCurrentProgram loads the preset");
}

struct FakeHost : public juce::AudioPlayHead
{
    double bpm = 120.0;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo i; i.setBpm(bpm); i.setIsPlaying(true); return i; }
};

void testHostTempo()
{
    info("\n== Host tempo ==");
    Rig rig;
    FakeHost host;
    rig.proc.setPlayHead(&host);
    const double sr = rig.sr;
    auto echoTime = [&](double bpm, int sync) {
        host.bpm = bpm;
        rig.loadDefaults();
        rig.setParam("o1Wave", Sine); rig.setParam("o1Uni", 1); rig.setParam("fType", LP12); rig.setParam("fCutoff", 12000.f); rig.setParam("fEnv", 0.f);
        rig.setParam("ampA", 0.001f); rig.setParam("ampD", 0.04f); rig.setParam("ampS", 0.f); rig.setParam("ampR", 0.01f);
        rig.setParam("dlMix", 1.f); rig.setParam("dlSync", (float) sync); rig.setParam("dlFb", 0.f); rig.setParam("dlTone", 12000.f);
        rig.setParam("rvMix", 0.f);
        auto b = rig.render(1.6, { noteOn(0.0, 81, 120, sr), noteOff(0.05, 81, sr) });   // A5 blip
        int from = (int) (0.12 * sr); float best = 0.f; int at = from;
        for (int i = from; i < b.getNumSamples(); ++i) { const float a = std::abs(b.getSample(0, i)); if (a > best) { best = a; at = i; } }
        return (double) at / sr;
    };
    const double e120 = echoTime(120.0, 2), e60 = echoTime(60.0, 2), e90q = echoTime(90.0, 1);
    info("  1/8 echo at 120 bpm: " + juce::String(e120, 3) + " s, at 60 bpm: " + juce::String(e60, 3) + " s; 1/4 at 90 bpm: " + juce::String(e90q, 3) + " s");
    check(std::abs(e120 - 0.25) < 0.03, "tempo-synced delay follows host tempo (1/8 @ 120 bpm = 250 ms)");
    check(std::abs(e60 - 0.50) < 0.03, "tempo-synced delay follows a tempo change (1/8 @ 60 bpm = 500 ms)");
    check(std::abs(e90q - 0.6667) < 0.03, "1/4 note @ 90 bpm = 667 ms");
    rig.proc.setPlayHead(nullptr);
}

void testFrontPage()
{
    info("\n== Front page: genre mode, filters, reset, volume ==");
    AuraProcessor proc;
    proc.setPlayConfigDetails(0, 2, 44100.0, 512);
    proc.prepareToPlay(44100.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> base(proc.createEditor());
    auto* ed = dynamic_cast<aura::AuraEditor*>(base.get());
    check(ed != nullptr, "editor opens");
    if (ed == nullptr) return;
    ed->setSize(1280, 860);
    auto& pm = proc.presets;
    auto getp = [&](const char* id) { return proc.apvts.getRawParameterValue(id)->load(); };
    auto setp = [&](const char* id, float v) { auto* p = proc.apvts.getParameter(id); p->setValueNotifyingHost(p->convertTo0to1(v)); };

    check(pm.currentName() == "Plan A Pluck", "the plugin opens on Plan A Pluck (instantly playable)");
    check(!ed->isAdvancedPage(), "the editor opens on the simple page");
    ed->clickGenre(3);                                                     // JAZZ
    check(pm.currentName() == genreStarterPreset(3), "genre button loads that genre's starter sound (" + pm.currentName() + ")");
    check(ed->numShown() == (int) pm.filter("", "", 3, -1).size() && ed->numShown() > 5, "genre button filters the browser");
    ed->clickGenre(1);                                                     // MELODIC TRAP
    check(pm.currentName() == "Plan A Pluck", "Melodic Trap loads Plan A Pluck");
    ed->clickType(3);                                                      // Pads
    check(ed->numShown() == (int) pm.filter("", "", 1, GroupPad).size() && ed->numShown() >= 4, "Pads inside Melodic Trap lists the trap pads");
    for (int i : pm.filter("", "", 1, GroupPad)) check(pm.preset(i).group == GroupPad && (pm.preset(i).genres & GenreTrap), "  (" + pm.preset(i).name + " really is a Melodic Trap pad)");
    ed->clickType(0);

    // reset to original
    pm.load(pm.indexOfName("Airy Trap Pad"));
    check(!pm.isModified(), "freshly loaded preset is 'original'");
    const float rv0 = getp("rvMix"), br0 = getp("bright");
    setp("rvMix", 0.9f); setp("bright", 0.1f);
    check(pm.isModified(), "turning a knob marks the sound as changed");
    check(pm.resetToPreset() && std::abs(getp("rvMix") - rv0) < 0.01f && std::abs(getp("bright") - br0) < 0.01f && !pm.isModified(),
          "'Reset to original' restores the preset's own settings");

    // Volume belongs to the user, not to the preset
    setp("volume", 0.35f);
    pm.load(pm.indexOfName("Glass Bounce"));
    check(std::abs(getp("volume") - 0.35f) < 0.01f, "loading presets leaves the Volume knob alone");
    pm.resetToPreset();
    check(std::abs(getp("volume") - 0.35f) < 0.01f && !pm.isModified(), "reset leaves Volume alone and does not count it as a change");
    setp("volume", 0.8f);

    // saving keeps genre + group so user sounds show up under the right buttons
    pm.load(pm.indexOfName("Dreamy Lead"));
    const juce::String nm = "ZZ UI Test " + juce::String(juce::Random::getSystemRandom().nextInt(100000));
    check(pm.saveUserPreset(nm, "User"), "save my sound");
    const int ui = pm.indexOfName(nm);
    check(ui >= 0 && pm.preset(ui).group == GroupLead && (pm.preset(ui).genres & pm.preset(pm.indexOfName("Dreamy Lead")).genres) != 0, "saved sound keeps its sound type and genres");
    check(!pm.filter("", "User", -1, -1).empty() && !pm.filter(nm, "", -1, GroupLead).empty(), "saved sound is found under 'My sounds' and under Leads");
    pm.userPresetDirectory().getChildFile(juce::File::createLegalFileName(nm) + ".aurapreset").deleteFile();
    pm.rescanUserPresets();

    // advanced page still builds and toggles
    ed->showPage(true);
    check(ed->isAdvancedPage(), "advanced page opens");
    for (int t = 0; t < 4; ++t) ed->showTab(t);
    ed->showPage(false);
}

void testCpu()
{
    info("\n== CPU ==");
    Rig rig;
    const double sr = rig.sr;
    // worst case: unison 7 x 2 oscillators, wavetable, FM, sync, formant filter, all FX on, 16 voices
    rig.loadDefaults();
    for (auto kv : Ctx { { "o1Wave", Wavetable }, { "o1Uni", 7 }, { "o2Wave", Wavetable }, { "o2Uni", 7 }, { "o2Level", 0.8f }, { "fmAmt", 0.4f },
                         { "o1Sync", 1 }, { "fType", LP24 }, { "dsMix", 0.4f }, { "saAmt", 0.4f }, { "bcMix", 0.3f }, { "phMix", 0.4f }, { "flMix", 0.4f },
                         { "chMix", 0.4f }, { "eqLow", 3 }, { "cpRatio", 4 }, { "cpThresh", -20 }, { "dlMix", 0.3f }, { "rvMix", 0.4f }, { "ampR", 3.f } })
        rig.setParam(kv.first, kv.second);
    std::vector<Evt> e;
    for (int i = 0; i < 16; ++i) e.push_back(noteOn(0.0, 48 + i * 2, 100, sr));
    const double secs = 4.0;
    const auto t0 = std::chrono::steady_clock::now();
    auto buf = rig.render(secs, e);
    const double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const double load = el / secs * 100.0;
    info("  worst case (16 voices x 14 oscillators + full FX): " + juce::String(load, 1) + " % of one core (this machine, -O?)");
    check(!stats(buf).bad, "worst-case patch is stable");
    // typical: a preset with 4 voices
    const auto& p = factoryPresets()[0];
    rig.apply(resolveValues(p));
    const auto t1 = std::chrono::steady_clock::now();
    rig.render(secs, { noteOn(0.0, 60, 100, sr), noteOn(0.0, 64, 100, sr), noteOn(0.0, 67, 100, sr), noteOn(0.0, 72, 100, sr) });
    const double el2 = std::chrono::duration<double>(std::chrono::steady_clock::now() - t1).count();
    info("  typical preset, 4 voices: " + juce::String(el2 / secs * 100.0, 2) + " % of one core");
}

// ------------------------------------------------------------------------------------------
void calibrate(const juce::File& out)
{
    Rig rig;
    const double sr = rig.sr;
    juce::String text = "// Auto-generated by AuraRenderTest --calibrate. One loudness-matched master value per factory preset.\n";
    float lo = 10, hi = 0;
    for (auto& p : factoryPresets())
    {
        const bool quietCat = (p.group == GroupPad || p.group == GroupTexture);    // pads / textures sit under vocals
        const bool lowCat = (p.group == GroupBass);
        const float target = quietCat ? 0.085f : (lowCat ? 0.16f : 0.13f);
        const float refMaster = 0.4f;
        const auto r = renderPreset(rig, p, sr, refMaster);
        float m = refMaster * target / std::max(1e-5f, r.loud);
        if (r.peak * (m / refMaster) > 0.92f) m = refMaster * 0.92f / r.peak;      // keep headroom below the safety limiter
        m = juce::jlimit(0.05f, 2.0f, m);
        lo = std::min(lo, m); hi = std::max(hi, m);
        text << "    {\"" << p.name << "\", " << juce::String(m, 3) << "f},\n";
    }
    out.replaceWithText(text);
    info("calibrated " + juce::String((int) factoryPresets().size()) + " presets -> " + out.getFullPathName() + "  (master range " + juce::String(lo, 2) + " .. " + juce::String(hi, 2) + ")");
}


// --analyze [filter]: prints an objective "fingerprint" of presets (envelope shape in dB, brightness, stereo width, tail)
void analyze(const juce::String& filter)
{
    Rig rig;
    const double sr = rig.sr;
    std::cout << "name | group | peak dBFS | env dB rel.max @ 0-.1 .1-.3 .3-.6 .6-1.2 1.2-2.0 2.0-2.5(held) | after release .5/1/2 s | centroid early/late Hz | side/mid dB\n";
    for (auto& p : factoryPresets())
    {
        if (filter.isNotEmpty() && !p.name.containsIgnoreCase(filter) && !(filter == "new" && &p - &factoryPresets()[0] < 50)) continue;
        rig.apply(resolveValues(p));
        std::vector<Evt> e;
        if (p.testChord) { for (int n : { p.testNote, p.testNote + 4, p.testNote + 7 }) { e.push_back(noteOn(0.0, n, 100, sr)); e.push_back(noteOff(2.5, n, sr)); } }
        else { e.push_back(noteOn(0.0, p.testNote, 100, sr)); e.push_back(noteOff(2.5, p.testNote, sr)); }
        auto b = rig.render(6.0, e);
        auto w = [&](double a, double c) { return stats(b, (int) (a * sr), (int) (c * sr)).rms; };
        const float pk = stats(b).peak;
        float mx = 1e-9f;
        for (double t0 = 0; t0 < 2.5; t0 += 0.05) mx = std::max(mx, w(t0, t0 + 0.1));
        auto db = [&](float x) { return 20.f * std::log10(std::max(1e-6f, x / mx)); };
        double mid = 0, side = 0;
        for (int i = (int) (0.2 * sr); i < (int) (2.4 * sr); ++i)
        { const double l = b.getSample(0, i), r = b.getSample(1, i); mid += 0.25 * (l + r) * (l + r); side += 0.25 * (l - r) * (l - r); }
        char line[512];
        snprintf(line, sizeof(line), "%-24s %-8s %5.1f | %5.1f %5.1f %5.1f %5.1f %5.1f %5.1f | %5.1f %5.1f %5.1f | %5.0f / %5.0f | %5.1f",
                 p.name.toRawUTF8(), groupNames()[p.group].toRawUTF8(), 20 * std::log10(std::max(1e-6f, pk)),
                 db(w(0, .1)), db(w(.1, .3)), db(w(.3, .6)), db(w(.6, 1.2)), db(w(1.2, 2.0)), db(w(2.0, 2.5)),
                 db(w(3.0, 3.1)), db(w(3.5, 3.6)), db(w(4.5, 4.6)),
                 centroid(b, (int) (0.05 * sr), sr), centroid(b, (int) (1.5 * sr), sr), 10 * std::log10(std::max(1e-12, side / std::max(1e-12, mid))));
        std::cout << line << std::endl;
    }
}


void debugStereo()
{
    Rig rig; const double sr = rig.sr;
    for (const char* nm : { "Airy Trap Pad", "Plan A Pluck", "Wide Dream Pluck" })
    {
        for (auto& p : factoryPresets()) if (p.name == nm)
        {
            rig.apply(resolveValues(p));
            auto b = rig.render(2.0, { noteOn(0.0, 60, 100, sr) });
            double d = 0, s = 0;
            for (int i = (int) (0.5 * sr); i < (int) (1.5 * sr); ++i) { d = std::max(d, (double) std::abs(b.getSample(0, i) - b.getSample(1, i))); s = std::max(s, (double) std::abs(b.getSample(0, i))); }
            std::cout << nm << ": max|L-R| = " << d << "  max|L| = " << s << " stWidth=" << rig.getParam("stWidth") << " uni=" << rig.getParam("o1Uni") << std::endl;
        }
    }
}

// --rendermidi <file.mid> <preset name> <out.wav> [bpm]: plays a MIDI file through the real plugin (offline) and reports level
void renderMidi(const juce::File& midi, const juce::String& presetName, const juce::File& out, double bpm)
{
    Rig rig;
    FakeHost host; host.bpm = bpm;
    rig.proc.setPlayHead(&host);
    const double sr = rig.sr;
    const int idx = rig.proc.presets.indexOfName(presetName);
    if (idx < 0) { std::cout << "unknown preset: " << presetName << std::endl; return; }
    rig.proc.presets.load(idx);
    juce::FileInputStream in(midi);
    juce::MidiFile mf;
    if (!in.openedOk() || !mf.readFrom(in)) { std::cout << "cannot read MIDI file" << std::endl; return; }
    mf.convertTimestampTicksToSeconds();
    std::vector<Evt> evts;
    double endT = 0; int notes = 0;
    for (int t = 0; t < mf.getNumTracks(); ++t)
        for (int i = 0; i < mf.getTrack(t)->getNumEvents(); ++i)
        {
            const auto& m = mf.getTrack(t)->getEventPointer(i)->message;
            if (m.isNoteOn()) { ++notes; evts.push_back(noteOn(m.getTimeStamp(), m.getNoteNumber(), m.getVelocity(), sr)); }
            else if (m.isNoteOff()) { evts.push_back(noteOff(m.getTimeStamp(), m.getNoteNumber(), sr)); endT = std::max(endT, m.getTimeStamp()); }
        }
    const double total = endT + 4.0;
    auto buf = rig.render(total, evts);
    double peak = 0, sum = 0; int n = buf.getNumSamples();
    for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) { const double v = buf.getSample(c, i); peak = std::max(peak, std::abs(v)); sum += v * v; }
    const double rms = std::sqrt(sum / (2.0 * n));
    std::cout << midi.getFileName() << " -> " << presetName << ": " << notes << " notes, " << juce::String(total, 1) << " s, peak "
              << juce::String(20.0 * std::log10(std::max(1e-9, peak)), 1) << " dBFS, RMS " << juce::String(20.0 * std::log10(std::max(1e-9, rms)), 1) << " dBFS" << std::endl;
    juce::WavAudioFormat wav;
    out.deleteFile();
    std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(new juce::FileOutputStream(out), sr, 2, 24, {}, 0));
    if (w) w->writeFromAudioSampleBuffer(buf, 0, n);
    rig.proc.setPlayHead(nullptr);
}

void listGenres()
{
    int cnt[kNumGenres][kNumGroups] {};
    for (auto& p : factoryPresets()) for (int g = 0; g < kNumGenres; ++g) if (p.genres & (1u << g)) ++cnt[g][p.group];
    for (int g = 0; g < kNumGenres; ++g)
    {
        std::cout << genreNames()[g] << ": ";
        int tot = 0;
        for (int k = 0; k < kNumGroups; ++k) { std::cout << groupNames()[k] << " " << cnt[g][k] << "  "; tot += cnt[g][k]; }
        std::cout << " = " << tot << std::endl;
    }
}

void screenshots(const juce::File& dir, int presetIndex)
{
    dir.createDirectory();
    AuraProcessor proc;
    proc.setPlayConfigDetails(0, 2, 44100.0, 512);
    proc.prepareToPlay(44100.0, 512);
    proc.presets.load(presetIndex);
    std::unique_ptr<juce::AudioProcessorEditor> base(proc.createEditor());
    auto* ed = dynamic_cast<aura::AuraEditor*>(base.get());
    ed->setSize(1280, 860);
    // let some audio flow so the modulation scopes show live data
    juce::AudioBuffer<float> blk(2, 512);
    juce::MidiBuffer mb;
    mb.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8) 100), 0);
    juce::PNGImageFormat png;
    auto shot = [&](const juce::String& name) {
        for (int i = 0; i < 40; ++i) { proc.processBlock(blk, mb); mb.clear(); ed->pumpForTesting(); }
        auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        auto file = dir.getChildFile("editor_" + name + ".png");
        file.deleteFile();
        juce::FileOutputStream fos(file);
        if (fos.openedOk()) png.writeImageToStream(img, fos);
    };
    // front page: fresh, then Melodic Trap selected, then the Pads filter
    ed->showPage(false);
    shot("simple_start");
    ed->clickGenre(1);
    shot("simple_trap");
    ed->clickType(3);
    shot("simple_trap_pads");
    ed->showPage(true);
    const char* names[4] = { "mod", "fx", "voice", "layer" };
    for (int t = 0; t < 4; ++t) { ed->showTab(t); shot(names[t]); }
    info("wrote screenshots to " + dir.getFullPathName());
}

void renderWavs(const juce::File& dir)
{
    dir.createDirectory();
    Rig rig(44100.0);
    const double sr = rig.sr;
    juce::WavAudioFormat wav;
    for (auto& p : factoryPresets())
    {
        const auto r = renderPreset(rig, p, sr);
        (void) r;
        auto v = resolveValues(p);
        rig.apply(v);
        std::vector<Evt> e;
        const int base = p.testNote;
        if (p.testChord) { for (int n : { base, base + 4, base + 7 }) { e.push_back(noteOn(0.0, n, 100, sr)); e.push_back(noteOff(3.0, n, sr)); } }
        else
        {
            const int seq[4] = { 0, 3, 7, 5 };
            for (int k = 0; k < 4; ++k) { e.push_back(noteOn(0.5 * k, base + seq[k], 100, sr)); e.push_back(noteOff(0.5 * k + 0.4, base + seq[k], sr)); }
        }
        auto buf = rig.render(5.0, e);
        auto file = dir.getChildFile(juce::File::createLegalFileName(p.category + " - " + p.name) + ".wav");
        file.deleteFile();
        std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(new juce::FileOutputStream(file), sr, 2, 16, {}, 0));
        if (w) w->writeFromAudioSampleBuffer(buf, 0, buf.getNumSamples());
    }
    info("wrote WAVs to " + dir.getFullPathName());
}
} // namespace

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::StringArray args;
    for (int i = 1; i < argc; ++i) args.add(argv[i]);

    if (args.size() >= 2 && args[0] == "--calibrate") { calibrate(juce::File::getCurrentWorkingDirectory().getChildFile(args[1])); return 0; }
    if (args.size() >= 4 && args[0] == "--rendermidi")
    {
        renderMidi(juce::File::getCurrentWorkingDirectory().getChildFile(args[1]), args[2],
                   juce::File::getCurrentWorkingDirectory().getChildFile(args[3]), args.size() > 4 ? args[4].getDoubleValue() : 102.0);
        return 0;
    }
    if (args.size() >= 1 && args[0] == "--genres") { listGenres(); return 0; }
    if (args.size() >= 2 && args[0] == "--screenshot") { screenshots(juce::File::getCurrentWorkingDirectory().getChildFile(args[1]), args.size() > 2 ? args[2].getIntValue() : 0); return 0; }
    if (args.size() >= 1 && args[0] == "--debugstereo") { debugStereo(); return 0; }
    if (args.size() >= 1 && args[0] == "--analyze") { analyze(args.size() > 1 ? args[1] : juce::String()); return 0; }
    if (args.size() >= 2 && args[0] == "--wav") { renderWavs(juce::File::getCurrentWorkingDirectory().getChildFile(args[1])); return 0; }

    const bool quick = args.contains("--quick");
    testParameters();
    testPresetLibrary();
    testMidi();
    testAliasing();
    testParametersAffectAudio();
    testPresetsRender();
    testState();
    testFrontPage();
    testHostTempo();
    testSoundDesign();
    if (!quick) testFuzz();
    testCpu();

    std::cout << "\n================================================\n"
              << (gFailures == 0 ? "ALL CHECKS PASSED" : "FAILURES: " + juce::String(gFailures)) << "  (" << gChecks << " checks)" << std::endl;
    return gFailures == 0 ? 0 : 1;
}
