#include "Presets.h"
#include "Params.h"
#include <map>
#include <string>

namespace aura
{
namespace
{
using KV = std::initializer_list<std::pair<const char*, float>>;

const std::map<std::string, float>& calibratedGains()
{
    static const std::map<std::string, float> g = {
#include "PresetGains.inc"
    };
    return g;
}

juce::StringArray& errorLog() { static juce::StringArray e; return e; }

// --- genre / group tagging for the original 120 presets (category based, with a few per-name overrides) ---
struct CatTag { int group; unsigned genres; };
CatTag legacyTag(int cat, const juce::String& name)
{
    static const unsigned HH = GenreHipHop, TR = GenreTrap, RB = GenreRnB, JZ = GenreJazz, AM = GenreAmbient, PO = GenrePop;
    // Legacy (v1) sounds get ONE home genre (two where it is a genuinely common fit) so the genre buttons stay selective.
    // The newer library in PresetsNew.inc tags its sounds explicitly.
    static const CatTag tags[20] = {
        { GroupPluck, PO },            // 0 bright digital plucks
        { GroupLead,  TR },            // 1 nasal buzzy leads
        { GroupLead,  PO },            // 2 kazoo / trumpet leads
        { GroupKeys,  HH | RB },       // 3 soft melodic keys
        { GroupBell,  AM },            // 4 bells
        { GroupPad,   JZ | RB },       // 5 warm analog
        { GroupPad,   AM },            // 6 airy pads
        { GroupPad,   HH },            // 7 dark pads
        { GroupPad,   AM },            // 8 long sustained
        { GroupLead,  HH },            // 9 counter melody
        { GroupLead,  TR },            // 10 glassy leads
        { GroupLead,  PO },            // 11 retro 80s
        { GroupPluck, PO },            // 12 arpeggiated
        { GroupTexture, AM },          // 13 ambient textures
        { GroupLead,  RB },            // 14 dreamy
        { GroupLead,  HH },            // 15 distorted
        { GroupBass,  HH },            // 16 bass
        { GroupBass,  HH | TR },       // 17 808
        { GroupLead,  RB },            // 18 vocal formant
        { GroupTexture, HH },          // 19 experimental
    };
    CatTag t = tags[juce::jlimit(0, 19, cat)];
    static const std::map<std::string, int> groupOverride = {
        {"Analog Horns", GroupLead}, {"Chorus Pulse", GroupKeys}, {"Smooth Chords", GroupKeys}, {"Arcade Bass-Lead", GroupBass},
        {"Miami Pad", GroupPad}, {"Sunset Strings", GroupPad}, {"Teardrop Keys", GroupKeys}, {"Moonlight Pluck", GroupPluck},
        {"Aah Choir", GroupPad}, {"Ooh Pad", GroupPad}, {"Fuzz Pad", GroupPad}, {"Pluck Reply", GroupPluck}, {"Mosquito Pluck", GroupPluck},
        {"Overdrive Stab", GroupPluck}, {"Neon Stab", GroupPluck}, {"Arp Bells", GroupBell}, {"Glass Drone", GroupPad}, {"Prism", GroupBell},
        {"Low Sustain", GroupPad}, {"Distant Thunder", GroupTexture}, {"Underworld", GroupTexture}, {"Vibraphone Dream", GroupBell},
        {"Echo Reply", GroupPluck}, {"Soft Spotlight", GroupKeys}, {"Heart Glow", GroupPad} };
    const auto it = groupOverride.find(name.toStdString());
    if (it != groupOverride.end()) t.group = it->second;
    return t;
}

void finish(Preset& p, int cat)
{
    // Test material for calibration: pads / keys / textures are judged with a chord, the rest with one note.
    p.testChord = (p.group == GroupPad || p.group == GroupKeys || p.group == GroupTexture);
    if (cat == 12) p.testChord = true;                       // arpeggiated presets are judged with a chord too
    p.testNote = (p.group == GroupBass) ? ((cat == 16) ? 40 : 36) : 60;
    const auto it = calibratedGains().find(p.name.toStdString());
    if (it != calibratedGains().end()) p.values.emplace_back(master, it->second);
}

void fill(Preset& p, const char* name, KV kv)
{
    p.name = name;
    for (const auto& e : kv)
    {
        const int idx = indexOfParam(e.first);
        if (idx >= 0) p.values.emplace_back(idx, e.second);
        else errorLog().add(juce::String(name) + ": unknown parameter '" + e.first + "'");
    }
}

// original library: group / genres derive from the category
void add(std::vector<Preset>& out, int cat, const char* name, KV kv)
{
    Preset p;
    p.category = categoryNames()[cat];
    fill(p, name, kv);
    const auto t = legacyTag(cat, p.name);
    p.group = t.group; p.genres = t.genres;
    finish(p, cat);
    out.push_back(std::move(p));
}

// new library: explicit group + genres
void addN(std::vector<Preset>& out, int cat, int group, unsigned genres, const char* name, KV kv)
{
    Preset p;
    p.category = categoryNames()[cat];
    p.group = group; p.genres = genres;
    fill(p, name, kv);
    finish(p, cat);
    out.push_back(std::move(p));
}

enum Cat { PLUCK, NASAL, KAZOO, KEYS, BELLS, ANALOG, AIRY, DARK, LONG, COUNTER, GLASSY, RETRO, ARP, AMBIENT, DREAMY,
           DIST, BASS, B808, VOCAL, EXPERIMENT };

void buildPart1(std::vector<Preset>& P)
{
    // ===================================== 1. Bright digital plucks =====================================
    add(P, PLUCK, "Digital Bounce", {
        {"o1Wave", Wavetable}, {"o1WT", 0.52f}, {"o1Uni", 2}, {"o1Det", 9}, {"o2Wave", Square}, {"o2Oct", 1}, {"o2Level", 0.3f},
        {"fType", LP24}, {"fCutoff", 2400}, {"fRes", 0.25f}, {"fEnv", 0.8f}, {"fltA", 0.001f}, {"fltD", 0.18f}, {"fltS", 0},
        {"ampA", 0.002f}, {"ampD", 0.24f}, {"ampS", 0}, {"ampR", 0.15f},
        {"dlMix", 0.16f}, {"dlSync", 2}, {"dlFb", 0.3f}, {"rvMix", 0.12f}, {"rvSize", 0.4f} });
    add(P, PLUCK, "Metallic Pluck", {
        {"o1Wave", Sine}, {"fmAmt", 0.45f}, {"fmRatio", 3.5f}, {"o2Wave", Triangle}, {"o2Oct", 1}, {"o2Level", 0.3f},
        {"fType", LP12}, {"fCutoff", 6000}, {"fEnv", 0.4f}, {"fltD", 0.15f}, {"fltS", 0},
        {"ampD", 0.32f}, {"ampS", 0}, {"ampR", 0.2f}, {"chMix", 0.25f}, {"rvMix", 0.15f} });
    add(P, PLUCK, "Pixel Pluck", {
        {"o1Wave", Pulse}, {"o1PW", 0.25f}, {"o2Wave", Square}, {"o2Oct", -1}, {"o2Level", 0.35f},
        {"fType", LP24}, {"fCutoff", 3000}, {"fRes", 0.2f}, {"fEnv", 0.7f}, {"fltD", 0.14f}, {"fltS", 0},
        {"ampD", 0.2f}, {"ampS", 0}, {"ampR", 0.1f}, {"l1Rate", 4.f},
        {"m1Src", SrcLfo1}, {"m1Dst", DstPW1}, {"m1Amt", 0.5f},
        {"bcBits", 10}, {"bcRate", 2}, {"bcMix", 0.25f}, {"dlMix", 0.12f}, {"dlSync", 4} });
    add(P, PLUCK, "Spark Pluck", {
        {"o1Wave", Saw}, {"o1Uni", 3}, {"o1Det", 12}, {"o1Spread", 0.8f},
        {"fType", LP24}, {"fCutoff", 3500}, {"fRes", 0.3f}, {"fEnv", 0.9f}, {"fltD", 0.12f}, {"fltS", 0},
        {"ampD", 0.18f}, {"ampS", 0}, {"ampR", 0.12f}, {"vVelSens", 0.9f},
        {"m1Src", SrcVel}, {"m1Dst", DstCutoff}, {"m1Amt", 0.35f},
        {"dlMix", 0.2f}, {"dlSync", 4}, {"dlFb", 0.35f}, {"rvMix", 0.1f} });
    add(P, PLUCK, "Glass Tick", {
        {"o1Wave", Wavetable}, {"o1WT", 0.78f}, {"o2Wave", Sine}, {"o2Oct", 1}, {"o2Semi", 7}, {"o2Level", 0.25f},
        {"fType", LP12}, {"fCutoff", 9000}, {"fEnv", 0.3f}, {"fltD", 0.1f}, {"fltS", 0.2f},
        {"ampD", 0.14f}, {"ampS", 0}, {"ampR", 0.2f}, {"rvMix", 0.22f}, {"rvSize", 0.65f}, {"rvDamp", 0.35f}, {"stWidth", 1.2f} });
    add(P, PLUCK, "Rubber Pluck", {
        {"o1Wave", Square}, {"o1Level", 0.9f},
        {"fType", LP24}, {"fCutoff", 900}, {"fRes", 0.5f}, {"fEnv", 0.9f}, {"fltD", 0.13f}, {"fltS", 0},
        {"ampD", 0.22f}, {"ampS", 0}, {"ampR", 0.12f}, {"vMode", Mono}, {"vGlide", 0.05f},
        {"m1Src", SrcFiltEnv}, {"m1Dst", DstPitch}, {"m1Amt", 0.08f}, {"saAmt", 0.3f} });

    // ===================================== 2. Nasal, buzzy synth leads =====================================
    add(P, NASAL, "Nasal Lead", {
        {"o1Wave", Wavetable}, {"o1WT", 0.46f}, {"o2Wave", Saw}, {"o2Fine", 8}, {"o2Level", 0.5f},
        {"fType", LP24}, {"fCutoff", 3500}, {"fRes", 0.4f}, {"fEnv", 0.2f}, {"fltD", 0.3f}, {"fltS", 0.5f},
        {"ampA", 0.005f}, {"ampD", 0.3f}, {"ampS", 0.8f}, {"ampR", 0.2f},
        {"l1Rate", 5.5f}, {"m1Src", SrcLfo1}, {"m1Dst", DstPitch}, {"m1Amt", 0.02f},
        {"m2Src", SrcWheel}, {"m2Dst", DstPitch}, {"m2Amt", 0.03f},
        {"eqMid", 5}, {"eqMidFreq", 1500}, {"dsType", 0}, {"dsDrive", 0.25f}, {"dsMix", 0.15f}, {"chMix", 0.2f} });
    add(P, NASAL, "Buzz Saw Lead", {
        {"o1Wave", Saw}, {"o1Uni", 5}, {"o1Det", 22}, {"o1Spread", 0.7f},
        {"fType", LP24}, {"fCutoff", 3000}, {"fRes", 0.25f}, {"fEnv", 0.4f}, {"fltD", 0.3f}, {"fltS", 0.5f},
        {"ampD", 0.4f}, {"ampS", 0.75f}, {"ampR", 0.2f}, {"saAmt", 0.35f}, {"rvMix", 0.1f} });
    add(P, NASAL, "Reed Buzz", {
        {"o1Wave", Pulse}, {"o1PW", 0.2f}, {"o2Wave", Saw}, {"o2Level", 0.4f}, {"o2Fine", -6},
        {"l2Rate", 0.7f}, {"m1Src", SrcLfo2}, {"m1Dst", DstPW1}, {"m1Amt", 0.4f},
        {"fType", LP12}, {"fCutoff", 2200}, {"fRes", 0.35f}, {"fEnv", 0.3f}, {"fltD", 0.25f},
        {"ampS", 0.8f}, {"ampR", 0.18f}, {"chMix", 0.25f}, {"dlMix", 0.1f} });
    add(P, NASAL, "Honk Lead", {
        {"o1Wave", Wavetable}, {"o1WT", 0.6f}, {"o2Wave", Square}, {"o2Oct", -1}, {"o2Level", 0.3f},
        {"fType", Formant}, {"fCutoff", 2000}, {"fRes", 0.3f}, {"fVowel", 0.25f}, {"fEnv", 0},
        {"ampA", 0.01f}, {"ampS", 0.85f}, {"ampR", 0.2f},
        {"l1Rate", 5.2f}, {"m1Src", SrcLfo1}, {"m1Dst", DstPitch}, {"m1Amt", 0.018f},
        {"dsMix", 0.12f}, {"rvMix", 0.12f} });
    add(P, NASAL, "Hornet", {
        {"o1Wave", Saw}, {"o1Uni", 7}, {"o1Det", 40}, {"o1Spread", 0.85f}, {"fmAmt", 0.15f}, {"fmRatio", 2},
        {"fType", LP12}, {"fCutoff", 4500}, {"fRes", 0.3f}, {"fEnv", 0.2f}, {"fltD", 0.2f},
        {"ampS", 0.8f}, {"ampR", 0.15f}, {"eqHigh", -2}, {"eqMid", 3}, {"eqMidFreq", 2200}, {"chMix", 0.15f} });
    add(P, NASAL, "Mosquito Pluck", {
        {"o1Wave", Pulse}, {"o1PW", 0.15f}, {"o2Wave", Square}, {"o2Oct", 1}, {"o2Level", 0.2f},
        {"fType", BP12}, {"fCutoff", 2500}, {"fRes", 0.5f}, {"fEnv", 0.5f}, {"fltD", 0.2f}, {"fltS", 0.1f},
        {"ampD", 0.35f}, {"ampS", 0.2f}, {"ampR", 0.15f}, {"dlMix", 0.18f}, {"dlSync", 4}, {"phMix", 0.2f}, {"phRate", 0.5f} });

    // ===================================== 3. Kazoo & trumpet leads =====================================
    add(P, KAZOO, "Rubber Trumpet", {
        {"o1Wave", Saw}, {"o2Wave", Saw}, {"o2Level", 0.6f}, {"o2Fine", 5},
        {"fType", LP24}, {"fCutoff", 700}, {"fRes", 0.2f}, {"fEnv", 0.9f}, {"fltA", 0.09f}, {"fltD", 0.3f}, {"fltS", 0.6f}, {"fltR", 0.2f},
        {"ampA", 0.04f}, {"ampD", 0.2f}, {"ampS", 0.8f}, {"ampR", 0.15f},
        {"l1Rate", 5.2f}, {"m1Src", SrcLfo1}, {"m1Dst", DstPitch}, {"m1Amt", 0.015f},
        {"vMode", Legato}, {"vGlide", 0.04f}, {"rvMix", 0.12f}, {"saAmt", 0.15f} });
    add(P, KAZOO, "Kazoo Lead", {
        {"o1Wave", Pulse}, {"o1PW", 0.3f}, {"o2Wave", Wavetable}, {"o2WT", 0.5f}, {"o2Level", 0.5f},
        {"fType", Formant}, {"fCutoff", 2200}, {"fRes", 0.4f}, {"fVowel", 0.25f}, {"fEnv", 0},
        {"ampA", 0.01f}, {"ampS", 0.85f}, {"ampR", 0.12f}, {"l3Shape", LfoSine}, {"l3Rate", 6.5f},
        {"m1Src", SrcLfo3}, {"m1Dst", DstPitch}, {"m1Amt", 0.03f}, {"dsMix", 0.2f}, {"dsDrive", 0.2f}, {"rvMix", 0.1f} });
    add(P, KAZOO, "Synth Brass Lead", {
        {"o1Wave", Saw}, {"o1Uni", 3}, {"o1Det", 10}, {"o1Spread", 0.4f},
        {"fType", LP24}, {"fCutoff", 1100}, {"fRes", 0.15f}, {"fEnv", 0.8f}, {"fltA", 0.06f}, {"fltD", 0.4f}, {"fltS", 0.5f},
        {"ampA", 0.03f}, {"ampS", 0.85f}, {"ampR", 0.2f}, {"chMix", 0.3f}, {"rvMix", 0.18f},
        {"o2Wave", Square}, {"o2Level", 0.25f}, {"o2Oct", -1}, {"vMode", Legato}, {"vGlide", 0.03f}, {"l1Rate", 5.5f}, {"m1Src", SrcLfo1}, {"m1Dst", DstPitch}, {"m1Amt", 0.012f}, {"eqHigh", 2} });
    add(P, KAZOO, "Muted Horn", {
        {"o1Wave", Wavetable}, {"o1WT", 0.62f},
        {"fType", LP12}, {"fCutoff", 900}, {"fRes", 0.1f}, {"fEnv", 0.5f}, {"fltA", 0.04f}, {"fltD", 0.3f}, {"fltS", 0.5f},
        {"ampA", 0.03f}, {"ampD", 0.3f}, {"ampS", 0.7f}, {"ampR", 0.2f},
        {"m1Src", SrcWheel}, {"m1Dst", DstCutoff}, {"m1Amt", 0.5f}, {"rvMix", 0.2f}, {"eqLow", -3} });
    add(P, KAZOO, "Toy Trumpet", {
        {"o1Wave", Square}, {"fmAmt", 0.2f}, {"fmRatio", 1}, {"o2Wave", Pulse}, {"o2PW", 0.35f}, {"o2Level", 0.3f}, {"o2Oct", 1},
        {"fType", LP12}, {"fCutoff", 3200}, {"fEnv", 0.2f}, {"fltD", 0.2f},
        {"ampA", 0.015f}, {"ampS", 0.8f}, {"ampR", 0.1f}, {"l1Rate", 6.f},
        {"m1Src", SrcLfo1}, {"m1Dst", DstPitch}, {"m1Amt", 0.025f}, {"bcBits", 11}, {"bcMix", 0.12f}, {"rvMix", 0.15f} });
    add(P, KAZOO, "Comb Horn", {
        {"o1Wave", Saw}, {"o1Fine", 12}, {"o2Wave", Saw}, {"o2Fine", -12}, {"o2Level", 0.7f},
        {"fType", LP24}, {"fCutoff", 1500}, {"fRes", 0.2f}, {"fEnv", 0.6f}, {"fltA", 0.03f}, {"fltD", 0.35f}, {"fltS", 0.6f},
        {"ampA", 0.02f}, {"ampS", 0.85f}, {"ampR", 0.18f}, {"flMix", 0.25f}, {"flRate", 0.15f}, {"flFb", 0.5f}, {"rvMix", 0.1f} });

    // ===================================== 4. Soft melodic keys =====================================
    add(P, KEYS, "Floating Keys", {
        {"o1Wave", Triangle}, {"o2Wave", Sine}, {"o2Oct", 1}, {"o2Level", 0.3f},
        {"fType", LP12}, {"fCutoff", 3200}, {"fEnv", 0.25f}, {"fltD", 0.6f}, {"fltS", 0.3f},
        {"ampA", 0.003f}, {"ampD", 0.9f}, {"ampS", 0.35f}, {"ampR", 0.6f},
        {"chMix", 0.3f}, {"chRate", 0.5f}, {"rvMix", 0.3f}, {"rvSize", 0.6f} });
    add(P, KEYS, "Soft Glass", {
        {"o1Wave", Wavetable}, {"o1WT", 0.35f}, {"o2Wave", Sine}, {"o2Oct", 1}, {"o2Level", 0.2f},
        {"fType", LP24}, {"fCutoff", 4500}, {"fRes", 0.1f}, {"fEnv", 0.2f}, {"fltD", 0.8f}, {"fltS", 0.3f},
        {"ampD", 1.2f}, {"ampS", 0.2f}, {"ampR", 0.7f}, {"chMix", 0.25f}, {"rvMix", 0.35f}, {"rvSize", 0.7f} });
    add(P, KEYS, "Felt Keys", {
        {"o1Wave", Sine}, {"o2Wave", Triangle}, {"o2Level", 0.4f},
        {"fType", LP12}, {"fCutoff", 1800}, {"fEnv", 0.3f}, {"fltD", 0.4f}, {"fltS", 0.2f},
        {"ampD", 0.8f}, {"ampS", 0.25f}, {"ampR", 0.5f}, {"saAmt", 0.2f}, {"rvMix", 0.2f}, {"vVelSens", 0.8f} });
    add(P, KEYS, "Velvet Rhodes", {
        {"o1Wave", Sine}, {"fmAmt", 0.22f}, {"fmRatio", 1}, {"o2Wave", Sine}, {"o2Level", 0.3f}, {"o2Oct", 1},
        {"fType", LP12}, {"fCutoff", 5000}, {"fEnv", 0},
        {"m1Src", SrcFiltEnv}, {"m1Dst", DstFM}, {"m1Amt", 0.25f}, {"fltD", 0.5f}, {"fltS", 0.1f},
        {"ampD", 1.5f}, {"ampS", 0.25f}, {"ampR", 0.5f}, {"chMix", 0.4f}, {"chRate", 0.35f}, {"rvMix", 0.2f} });
    add(P, KEYS, "Night Piano", {
        {"o1Wave", Triangle}, {"o1Uni", 2}, {"o1Det", 6}, {"o2Wave", Saw}, {"o2Oct", -1}, {"o2Level", 0.15f},
        {"fType", LP12}, {"fCutoff", 2200}, {"fEnv", 0.5f}, {"fltD", 0.6f}, {"fltS", 0.15f},
        {"ampD", 1.3f}, {"ampS", 0.1f}, {"ampR", 0.5f}, {"rvMix", 0.3f}, {"dlMix", 0.12f}, {"dlSync", 3} });
    add(P, KEYS, "Lofi Keys", {
        {"o1Wave", Saw}, {"o2Wave", Square}, {"o2Fine", 9}, {"o2Level", 0.4f},
        {"fType", LP12}, {"fCutoff", 1400}, {"fEnv", 0.2f}, {"fltD", 0.5f},
        {"ampD", 0.9f}, {"ampS", 0.3f}, {"ampR", 0.4f},
        {"l1Rate", 0.4f}, {"m1Src", SrcLfo1}, {"m1Dst", DstPitch}, {"m1Amt", 0.012f},
        {"bcBits", 12}, {"bcRate", 3}, {"bcMix", 0.2f}, {"saAmt", 0.35f}, {"chMix", 0.3f}, {"chRate", 0.4f} });

    // ===================================== 5. Bells & mallets =====================================
    add(P, BELLS, "Future Bell", {
        {"o1Wave", Sine}, {"fmAmt", 0.3f}, {"fmRatio", 3.5f}, {"o2Wave", Sine}, {"o2Oct", 1}, {"o2Semi", 7}, {"o2Level", 0.2f},
        {"fType", LP12}, {"fCutoff", 12000}, {"fEnv", 0},
        {"ampD", 2.2f}, {"ampS", 0}, {"ampR", 1.5f}, {"rvMix", 0.35f}, {"rvSize", 0.8f}, {"dlMix", 0.2f}, {"dlSync", 3} });
    add(P, BELLS, "Glass Marimba", {
        {"o1Wave", Sine}, {"fmAmt", 0.25f}, {"fmRatio", 4}, {"o2Wave", Triangle}, {"o2Oct", 1}, {"o2Level", 0.2f},
        {"fType", LP12}, {"fCutoff", 8000}, {"fEnv", 0.3f}, {"fltD", 0.1f}, {"fltS", 0},
        {"m1Src", SrcFiltEnv}, {"m1Dst", DstFM}, {"m1Amt", 0.3f},
        {"ampD", 0.45f}, {"ampS", 0}, {"ampR", 0.3f}, {"rvMix", 0.2f} });
    add(P, BELLS, "Crystal Bell", {
        {"o1Wave", Wavetable}, {"o1WT", 0.9f}, {"o1Oct", 1}, {"fmAmt", 0.2f}, {"fmRatio", 5.2f},
        {"fType", LP12}, {"fCutoff", 14000}, {"fEnv", 0},
        {"ampD", 3.f}, {"ampS", 0}, {"ampR", 2.f}, {"rvMix", 0.45f}, {"rvSize", 0.85f}, {"stWidth", 1.3f} });
    add(P, BELLS, "Music Box", {
        {"o1Wave", Triangle}, {"o1Oct", 1}, {"o2Wave", Sine}, {"o2Oct", 2}, {"o2Level", 0.3f},
        {"fType", LP12}, {"fCutoff", 7000}, {"fEnv", 0},
        {"ampD", 0.8f}, {"ampS", 0}, {"ampR", 0.6f}, {"chMix", 0.2f}, {"chRate", 0.2f},
        {"dlMix", 0.15f}, {"dlSync", 2}, {"rvMix", 0.25f} });
    add(P, BELLS, "Metal Chime", {
        {"o1Wave", Sine}, {"fmAmt", 0.5f}, {"fmRatio", 2.76f}, {"o1Oct", 1},
        {"fType", LP12}, {"fCutoff", 16000}, {"fEnv", 0}, {"fxHP", 300},
        {"ampD", 4.f}, {"ampS", 0}, {"ampR", 2.5f}, {"rvMix", 0.4f}, {"rvSize", 0.8f}, {"rvDamp", 0.3f} });
    add(P, BELLS, "Vibraphone Dream", {
        {"o1Wave", Sine}, {"o2Wave", Sine}, {"o2Oct", 2}, {"o2Level", 0.15f}, {"fmAmt", 0.08f}, {"fmRatio", 4},
        {"fType", LP12}, {"fCutoff", 6000}, {"fEnv", 0},
        {"l1Rate", 5.f}, {"m1Src", SrcLfo1}, {"m1Dst", DstAmp}, {"m1Amt", 0.3f},
        {"ampD", 2.f}, {"ampS", 0.1f}, {"ampR", 1.2f}, {"rvMix", 0.3f}, {"chMix", 0.2f} });

    // ===================================== 6. Warm analog synths =====================================
    add(P, ANALOG, "Warm Analog", {
        {"o1Wave", Saw}, {"o1Uni", 3}, {"o1Det", 14}, {"o2Wave", Square}, {"o2Oct", -1}, {"o2Level", 0.4f},
        {"fType", LP24}, {"fCutoff", 1400}, {"fRes", 0.2f}, {"fEnv", 0.35f}, {"fltD", 0.8f}, {"fltS", 0.4f},
        {"ampA", 0.01f}, {"ampD", 0.5f}, {"ampS", 0.7f}, {"ampR", 0.4f}, {"chMix", 0.25f}, {"saAmt", 0.2f}, {"rvMix", 0.15f} });
    add(P, ANALOG, "Analog Stack", {
        {"o1Wave", Saw}, {"o2Wave", Saw}, {"o2Fine", 10}, {"o2Level", 0.7f}, {"subLevel", 0.3f}, {"subWave", SubSquare},
        {"fType", LP24}, {"fCutoff", 1800}, {"fRes", 0.25f}, {"fEnv", 0.3f}, {"fltD", 0.6f}, {"fltS", 0.5f},
        {"ampA", 0.02f}, {"ampS", 0.8f}, {"ampR", 0.35f}, {"saAmt", 0.25f} });
    add(P, ANALOG, "Chorus Pulse", {
        {"o1Wave", Pulse}, {"o2Wave", Saw}, {"o2Level", 0.5f}, {"l2Rate", 0.6f},
        {"m1Src", SrcLfo2}, {"m1Dst", DstPW1}, {"m1Amt", 0.35f},
        {"fType", LP24}, {"fCutoff", 2200}, {"fRes", 0.15f}, {"fEnv", 0.2f},
        {"ampA", 0.02f}, {"ampS", 0.8f}, {"ampR", 0.5f}, {"chMix", 0.55f}, {"chRate", 0.7f}, {"chDepth", 0.7f} });
    add(P, ANALOG, "Tape Pad", {
        {"o1Wave", Saw}, {"o1Uni", 2}, {"o1Det", 10}, {"o2Wave", Triangle}, {"o2Level", 0.5f},
        {"fType", LP12}, {"fCutoff", 1100}, {"fEnv", 0.2f}, {"fltA", 0.3f},
        {"ampA", 0.35f}, {"ampS", 0.85f}, {"ampR", 0.9f}, {"saAmt", 0.45f}, {"chMix", 0.3f}, {"chRate", 0.3f}, {"rvMix", 0.2f} });
    add(P, ANALOG, "Analog Horns", {
        {"o1Wave", Saw}, {"o1Uni", 4}, {"o1Det", 18},
        {"fType", LP24}, {"fCutoff", 900}, {"fRes", 0.2f}, {"fEnv", 0.7f}, {"fltA", 0.08f}, {"fltD", 0.5f}, {"fltS", 0.55f},
        {"ampA", 0.05f}, {"ampS", 0.85f}, {"ampR", 0.3f}, {"chMix", 0.25f}, {"rvMix", 0.2f},
        {"o2Wave", Triangle}, {"o2Level", 0.35f}, {"o2Oct", -1}, {"subLevel", 0.2f}, {"vVelSens", 0.9f}, {"m1Src", SrcVel}, {"m1Dst", DstCutoff}, {"m1Amt", 0.3f}, {"saAmt", 0.3f} });
    add(P, ANALOG, "Smooth Chords", {
        {"o1Wave", Square}, {"o2Wave", Saw}, {"o2Fine", 7}, {"o2Level", 0.5f},
        {"fType", LP24}, {"fCutoff", 1600}, {"fRes", 0.1f}, {"fEnv", 0.25f}, {"fltD", 0.9f}, {"fltS", 0.5f},
        {"ampA", 0.04f}, {"ampS", 0.8f}, {"ampR", 0.6f}, {"rvMix", 0.25f}, {"chMix", 0.2f} });

    // ===================================== 7. Airy background pads =====================================
    add(P, AIRY, "Airy Motion", {
        {"o1Wave", Saw}, {"o1Uni", 5}, {"o1Det", 25}, {"o1Spread", 1}, {"o2Wave", Sine}, {"o2Oct", 1}, {"o2Level", 0.2f},
        {"nLevel", 0.06f}, {"nTone", 0.9f},
        {"fType", LP12}, {"fCutoff", 3500}, {"fRes", 0.1f}, {"fEnv", 0.1f}, {"fltA", 1.f},
        {"l1Rate", 0.2f}, {"m1Src", SrcLfo1}, {"m1Dst", DstCutoff}, {"m1Amt", 0.25f},
        {"ampA", 1.2f}, {"ampD", 1.f}, {"ampS", 0.8f}, {"ampR", 2.5f}, {"chMix", 0.4f}, {"rvMix", 0.45f}, {"rvSize", 0.8f} });
    add(P, AIRY, "Wide Horizon", {
        {"o1Wave", Wavetable}, {"o1WT", 0.25f}, {"o1Uni", 7}, {"o1Det", 30}, {"o1Spread", 1}, {"o2Wave", Triangle}, {"o2Oct", -1}, {"o2Level", 0.3f},
        {"fType", LP24}, {"fCutoff", 2800}, {"fRes", 0.1f}, {"fEnv", 0.15f}, {"fltA", 1.2f},
        {"ampA", 1.5f}, {"ampS", 0.85f}, {"ampR", 3.f}, {"rvMix", 0.5f}, {"rvSize", 0.88f}, {"stWidth", 1.4f}, {"chMix", 0.3f} });
    add(P, AIRY, "Cloud Breath", {
        {"o1Wave", Saw}, {"o1Uni", 3}, {"o1Det", 18}, {"nLevel", 0.15f}, {"nTone", 0.7f},
        {"fType", LP12}, {"fCutoff", 5000}, {"fRes", 0.15f}, {"fEnv", 0},
        {"l1Rate", 0.15f}, {"m1Src", SrcLfo1}, {"m1Dst", DstCutoff}, {"m1Amt", 0.3f},
        {"ampA", 2.f}, {"ampS", 0.9f}, {"ampR", 3.f}, {"rvMix", 0.5f}, {"rvSize", 0.85f}, {"fxHP", 120} });
    add(P, AIRY, "Silk Air", {
        {"o1Wave", Triangle}, {"o1Uni", 5}, {"o1Det", 18}, {"o2Wave", Sine}, {"o2Oct", 1}, {"o2Level", 0.3f},
        {"fType", LP12}, {"fCutoff", 4500}, {"fEnv", 0},
        {"ampA", 0.9f}, {"ampS", 0.9f}, {"ampR", 2.f}, {"phMix", 0.3f}, {"phRate", 0.15f}, {"phFb", 0.3f}, {"rvMix", 0.4f} });
    add(P, AIRY, "Morning Haze", {
        {"o1Wave", Saw}, {"o2Wave", Pulse}, {"o2Level", 0.5f}, {"l2Rate", 0.3f},
        {"m1Src", SrcLfo2}, {"m1Dst", DstPW2}, {"m1Amt", 0.4f},
        {"fType", LP24}, {"fCutoff", 2500}, {"fRes", 0.12f}, {"fEnv", 0.15f}, {"fltA", 0.8f},
        {"ampA", 1.f}, {"ampS", 0.85f}, {"ampR", 2.f}, {"chMix", 0.5f}, {"chRate", 0.3f},
        {"dlMix", 0.2f}, {"dlSync", 3}, {"rvMix", 0.4f} });
    add(P, AIRY, "Soft Halo", {
        {"o1Wave", Wavetable}, {"o1WT", 0.1f}, {"o1Uni", 5}, {"o1Det", 20}, {"o2Wave", Wavetable}, {"o2WT", 0.85f}, {"o2Oct", 1}, {"o2Level", 0.12f},
        {"fType", LP12}, {"fCutoff", 7000}, {"fEnv", 0},
        {"ampA", 1.8f}, {"ampS", 0.9f}, {"ampR", 4.f}, {"rvMix", 0.55f}, {"rvSize", 0.9f}, {"fxHP", 150}, {"stWidth", 1.3f} });
}
} // namespace

namespace
{
void buildPart2(std::vector<Preset>& P)
{
    // ===================================== 8. Dark atmospheric pads =====================================
    add(P, DARK, "Dark Cloud", {
        {"o1Wave", Saw}, {"o1Uni", 5}, {"o1Det", 20}, {"o1Spread", 0.9f}, {"o2Wave", Square}, {"o2Oct", -1}, {"o2Level", 0.4f},
        {"fType", LP24}, {"fCutoff", 600}, {"fRes", 0.25f}, {"fEnv", 0.2f}, {"fltA", 1.5f},
        {"l1Rate", 0.15f}, {"m1Src", SrcLfo1}, {"m1Dst", DstCutoff}, {"m1Amt", 0.2f},
        {"ampA", 2.f}, {"ampS", 0.9f}, {"ampR", 3.f}, {"rvMix", 0.5f}, {"rvSize", 0.9f}, {"rvDamp", 0.7f}, {"chMix", 0.2f} });
    add(P, DARK, "Midnight Drone", {
        {"o1Wave", Saw}, {"o1Uni", 3}, {"o1Det", 14}, {"subLevel", 0.5f}, {"subWave", SubSine},
        {"fType", LP24}, {"fCutoff", 400}, {"fRes", 0.2f}, {"fEnv", 0.3f}, {"fltA", 3.f}, {"fltS", 0.6f},
        {"ampA", 2.5f}, {"ampS", 1}, {"ampR", 3.5f}, {"rvMix", 0.45f}, {"rvSize", 0.85f}, {"saAmt", 0.3f} });
    add(P, DARK, "Shadow Layer", {
        {"o1Wave", Wavetable}, {"o1WT", 0.45f}, {"o1Uni", 4}, {"o1Det", 18},
        {"fType", LP12}, {"fCutoff", 900}, {"fRes", 0.35f}, {"fEnv", 0},
        {"l2Rate", 0.12f}, {"m1Src", SrcLfo2}, {"m1Dst", DstWT1}, {"m1Amt", 0.3f},
        {"ampA", 1.6f}, {"ampS", 0.9f}, {"ampR", 3.f}, {"dlMix", 0.25f}, {"dlSync", 3}, {"dlFb", 0.45f}, {"rvMix", 0.5f}, {"rvSize", 0.85f} });
    add(P, DARK, "Black Glass", {
        {"o1Wave", Saw}, {"o2Wave", Wavetable}, {"o2WT", 0.7f}, {"o2Level", 0.6f}, {"o2Fine", 6},
        {"fType", LP24}, {"fCutoff", 700}, {"fRes", 0.3f}, {"fEnv", 0.25f}, {"fltA", 1.2f},
        {"ampA", 1.2f}, {"ampS", 0.85f}, {"ampR", 2.5f}, {"eqHigh", -4}, {"dsMix", 0.1f}, {"dsDrive", 0.2f}, {"rvMix", 0.4f} });
    add(P, DARK, "Underworld", {
        {"o1Wave", Square}, {"o1Oct", -1}, {"o1Uni", 3}, {"o1Det", 30}, {"nLevel", 0.08f}, {"nTone", 0.15f},
        {"fType", LP24}, {"fCutoff", 350}, {"fRes", 0.4f}, {"fEnv", 0.15f}, {"fltA", 2.f},
        {"ampA", 3.f}, {"ampS", 1}, {"ampR", 4.f}, {"rvMix", 0.55f}, {"rvSize", 0.9f}, {"rvDamp", 0.8f} });
    add(P, DARK, "Distant Thunder", {
        {"o1Wave", Saw}, {"o1Oct", -1}, {"nLevel", 0.3f}, {"nTone", 0.1f},
        {"fType", LP12}, {"fCutoff", 300}, {"fRes", 0.2f}, {"fEnv", 0.5f}, {"fltA", 2.5f}, {"fltD", 3.f}, {"fltS", 0.4f},
        {"ampA", 2.f}, {"ampS", 0.9f}, {"ampR", 3.f}, {"cpThresh", -20}, {"cpRatio", 3}, {"rvMix", 0.6f}, {"rvSize", 0.95f} });

    // ===================================== 9. Long sustained background synths =====================================
    add(P, LONG, "Background Motion", {
        {"o1Wave", Saw}, {"o1Uni", 3}, {"o1Det", 12}, {"o2Wave", Triangle}, {"o2Level", 0.5f},
        {"fType", LP24}, {"fCutoff", 1800}, {"fRes", 0.15f}, {"fEnv", 0.1f},
        {"l3Shape", LfoSine}, {"l3Rate", 0.3f}, {"m1Src", SrcLfo3}, {"m1Dst", DstCutoff}, {"m1Amt", 0.15f},
        {"ampA", 0.35f}, {"ampD", 0.5f}, {"ampS", 1}, {"ampR", 1.5f}, {"chMix", 0.3f}, {"rvMix", 0.25f} });
    add(P, LONG, "Velvet Pad", {
        {"o1Wave", Triangle}, {"o1Uni", 3}, {"o1Det", 12}, {"o2Wave", Saw}, {"o2Level", 0.3f},
        {"fType", LP12}, {"fCutoff", 1500}, {"fEnv", 0.1f},
        {"ampA", 0.6f}, {"ampS", 1}, {"ampR", 2.f}, {"saAmt", 0.2f}, {"rvMix", 0.3f}, {"chMix", 0.35f} });
    add(P, LONG, "Steady Glow", {
        {"o1Wave", Square}, {"o1Uni", 2}, {"o1Det", 8}, {"o2Wave", Sine}, {"o2Level", 0.4f},
        {"fType", LP24}, {"fCutoff", 1400}, {"fRes", 0.1f}, {"fEnv", 0},
        {"m1Src", SrcWheel}, {"m1Dst", DstCutoff}, {"m1Amt", 0.5f},
        {"ampA", 0.25f}, {"ampS", 1}, {"ampR", 1.2f}, {"dlMix", 0.1f}, {"dlSync", 3}, {"rvMix", 0.2f} });
    add(P, LONG, "Drift Strings", {
        {"o1Wave", Saw}, {"o1Uni", 7}, {"o1Det", 18}, {"o1Spread", 0.9f}, {"o2Wave", Saw}, {"o2Oct", 1}, {"o2Level", 0.2f},
        {"fType", LP24}, {"fCutoff", 2600}, {"fRes", 0.12f}, {"fEnv", 0.1f},
        {"ampA", 0.8f}, {"ampS", 1}, {"ampR", 1.8f}, {"chMix", 0.45f}, {"flMix", 0.1f}, {"flRate", 0.1f}, {"rvMix", 0.35f} });
    add(P, LONG, "Low Sustain", {
        {"o1Wave", Saw}, {"o1Oct", -1}, {"subLevel", 0.4f},
        {"fType", LP24}, {"fCutoff", 700}, {"fRes", 0.15f}, {"fEnv", 0.1f},
        {"ampA", 0.2f}, {"ampS", 1}, {"ampR", 1.f}, {"rvMix", 0.2f}, {"cpThresh", -18}, {"cpRatio", 3} });
    add(P, LONG, "Slow Bloom", {
        {"o1Wave", Wavetable}, {"o1WT", 0.1f}, {"o1Uni", 3}, {"o1Det", 14},
        {"l2Rate", 0.1f}, {"l2Shape", LfoTri}, {"m1Src", SrcLfo2}, {"m1Dst", DstWT1}, {"m1Amt", 0.45f},
        {"fType", LP24}, {"fCutoff", 3000}, {"fRes", 0.1f}, {"fEnv", 0},
        {"ampA", 1.5f}, {"ampS", 1}, {"ampR", 2.5f}, {"rvMix", 0.35f}, {"chMix", 0.25f} });

    // ===================================== 10. Counter-melody synths =====================================
    add(P, COUNTER, "Counter Glow", {
        {"o1Wave", Triangle}, {"o2Wave", Saw}, {"o2Level", 0.4f}, {"o2Uni", 2}, {"o2Det", 10},
        {"fType", LP24}, {"fCutoff", 2200}, {"fRes", 0.15f}, {"fEnv", 0.3f}, {"fltD", 0.4f}, {"fltS", 0.4f},
        {"ampA", 0.02f}, {"ampD", 0.4f}, {"ampS", 0.6f}, {"ampR", 0.4f}, {"dlMix", 0.25f}, {"dlSync", 3}, {"chMix", 0.25f} });
    add(P, COUNTER, "Echo Reply", {
        {"o1Wave", Square}, {"o1Level", 0.6f},
        {"fType", LP12}, {"fCutoff", 1800}, {"fEnv", 0.4f}, {"fltD", 0.25f}, {"fltS", 0.1f},
        {"ampD", 0.5f}, {"ampS", 0.2f}, {"ampR", 0.3f}, {"dlMix", 0.35f}, {"dlSync", 3}, {"dlFb", 0.5f}, {"dlPing", 1}, {"rvMix", 0.25f} });
    add(P, COUNTER, "Hollow Answer", {
        {"o1Wave", Wavetable}, {"o1WT", 0.73f},
        {"fType", LP12}, {"fCutoff", 2500}, {"fEnv", 0.2f}, {"fltD", 0.3f},
        {"ampA", 0.02f}, {"ampD", 0.3f}, {"ampS", 0.6f}, {"ampR", 0.4f},
        {"l1Rate", 4.8f}, {"m1Src", SrcLfo1}, {"m1Dst", DstPitch}, {"m1Amt", 0.015f}, {"rvMix", 0.3f}, {"dlMix", 0.15f} });
    add(P, COUNTER, "Side Whistle", {
        {"o1Wave", Sine}, {"o2Wave", Triangle}, {"o2Oct", 1}, {"o2Level", 0.3f},
        {"fType", LP12}, {"fCutoff", 6000}, {"fEnv", 0},
        {"ampA", 0.05f}, {"ampS", 0.8f}, {"ampR", 0.3f}, {"l1Rate", 5.f},
        {"m1Src", SrcLfo1}, {"m1Dst", DstPitch}, {"m1Amt", 0.02f}, {"rvMix", 0.3f}, {"dlMix", 0.18f}, {"dlSync", 2} });
    add(P, COUNTER, "Pluck Reply", {
        {"o1Wave", Saw}, {"o1Uni", 2}, {"o1Det", 9},
        {"fType", LP24}, {"fCutoff", 2800}, {"fRes", 0.2f}, {"fEnv", 0.6f}, {"fltD", 0.25f}, {"fltS", 0},
        {"ampD", 0.45f}, {"ampS", 0.1f}, {"ampR", 0.25f}, {"dlMix", 0.3f}, {"dlSync", 3}, {"dlFb", 0.45f}, {"rvMix", 0.15f} });
    add(P, COUNTER, "Soft Spotlight", {
        {"o1Wave", Wavetable}, {"o1WT", 0.2f}, {"o1Uni", 2}, {"o1Det", 7},
        {"fType", LP12}, {"fCutoff", 3500}, {"fEnv", 0.1f},
        {"ampA", 0.03f}, {"ampD", 0.6f}, {"ampS", 0.5f}, {"ampR", 0.5f}, {"phMix", 0.2f}, {"phRate", 0.3f}, {"rvMix", 0.35f} });

    // ===================================== 11. Glassy digital leads =====================================
    add(P, GLASSY, "Liquid Lead", {
        {"o1Wave", Wavetable}, {"o1WT", 0.7f}, {"o2Wave", Sine}, {"o2Oct", 1}, {"o2Level", 0.2f},
        {"fType", LP24}, {"fCutoff", 6000}, {"fRes", 0.15f}, {"fEnv", 0.2f}, {"fltD", 0.3f}, {"fltS", 0.6f},
        {"ampA", 0.008f}, {"ampS", 0.85f}, {"ampR", 0.25f}, {"vMode", Legato}, {"vGlide", 0.06f},
        {"l1Rate", 5.5f}, {"m1Src", SrcLfo1}, {"m1Dst", DstPitch}, {"m1Amt", 0.015f},
        {"chMix", 0.3f}, {"dlMix", 0.15f}, {"dlSync", 2}, {"rvMix", 0.2f} });
    add(P, GLASSY, "Glass Beam", {
        {"o1Wave", Wavetable}, {"o1WT", 0.75f}, {"o1Uni", 3}, {"o1Det", 10},
        {"fType", LP12}, {"fCutoff", 8000}, {"fEnv", 0.1f},
        {"ampS", 0.9f}, {"ampR", 0.2f}, {"rvMix", 0.25f}, {"dlMix", 0.2f}, {"dlSync", 4}, {"dlPing", 1} });
    add(P, GLASSY, "Crystal Sine Lead", {
        {"o1Wave", Sine}, {"fmAmt", 0.15f}, {"fmRatio", 2}, {"o2Wave", Pulse}, {"o2PW", 0.4f}, {"o2Oct", 1}, {"o2Level", 0.15f},
        {"fType", LP12}, {"fCutoff", 9000}, {"fEnv", 0},
        {"ampA", 0.01f}, {"ampS", 0.9f}, {"ampR", 0.3f}, {"chMix", 0.3f}, {"rvMix", 0.3f} });
    add(P, GLASSY, "Prism", {
        {"o1Wave", Wavetable}, {"o1WT", 0.5f}, {"o1Uni", 2}, {"o1Det", 8},
        {"l1Rate", 0.6f}, {"m1Src", SrcLfo1}, {"m1Dst", DstWT1}, {"m1Amt", 0.35f},
        {"fType", LP24}, {"fCutoff", 7000}, {"fRes", 0.2f}, {"fEnv", 0.15f},
        {"ampS", 0.85f}, {"ampR", 0.3f}, {"stWidth", 1.3f}, {"rvMix", 0.25f}, {"flMix", 0.1f} });
    add(P, GLASSY, "Ice Wire", {
        {"o1Wave", Pulse}, {"o1PW", 0.12f}, {"o2Wave", Square}, {"o2Oct", 1}, {"o2Level", 0.2f},
        {"fType", LP12}, {"fCutoff", 9000}, {"fRes", 0.1f}, {"fEnv", 0}, {"fxHP", 250},
        {"ampS", 0.85f}, {"ampR", 0.25f}, {"phMix", 0.2f}, {"phRate", 0.4f}, {"rvMix", 0.2f} });
    add(P, GLASSY, "Digital Halo", {
        {"o1Wave", Wavetable}, {"o1WT", 0.9f}, {"o1Uni", 4}, {"o1Det", 14}, {"o2Wave", Triangle}, {"o2Level", 0.3f},
        {"fType", LP24}, {"fCutoff", 6500}, {"fRes", 0.15f}, {"fEnv", 0.2f},
        {"ampA", 0.02f}, {"ampS", 0.85f}, {"ampR", 0.4f}, {"flMix", 0.15f}, {"flRate", 0.2f}, {"rvMix", 0.3f} });

    // ===================================== 12. Retro 1980s synths =====================================
    add(P, RETRO, "Retro Brass 84", {
        {"o1Wave", Saw}, {"o1Uni", 2}, {"o1Det", 12}, {"o2Wave", Saw}, {"o2Fine", 9}, {"o2Level", 0.5f},
        {"fType", LP24}, {"fCutoff", 1200}, {"fRes", 0.15f}, {"fEnv", 0.8f}, {"fltA", 0.05f}, {"fltD", 0.5f}, {"fltS", 0.5f},
        {"ampA", 0.03f}, {"ampS", 0.85f}, {"ampR", 0.3f}, {"chMix", 0.35f}, {"rvMix", 0.2f},
        {"ampD", 0.4f}, {"fKey", 0.6f}, {"phMix", 0.2f}, {"phRate", 0.35f}, {"dlMix", 0.15f}, {"dlSync", 2}, {"o1Spread", 0.9f}, {"vVelSens", 0.8f} });
    add(P, RETRO, "Neon Stab", {
        {"o1Wave", Saw}, {"o2Wave", Pulse}, {"o2PW", 0.4f}, {"o2Level", 0.5f}, {"o2Fine", 7},
        {"fType", LP24}, {"fCutoff", 2000}, {"fRes", 0.25f}, {"fEnv", 0.9f}, {"fltD", 0.2f}, {"fltS", 0},
        {"ampD", 0.3f}, {"ampS", 0}, {"ampR", 0.2f}, {"dlMix", 0.25f}, {"dlSync", 3}, {"rvMix", 0.2f}, {"chMix", 0.2f} });
    add(P, RETRO, "Synthwave Lead", {
        {"o1Wave", Saw}, {"o1Uni", 3}, {"o1Det", 14}, {"subLevel", 0.25f}, {"subWave", SubSquare},
        {"fType", LP24}, {"fCutoff", 3500}, {"fRes", 0.2f}, {"fEnv", 0.3f}, {"fltD", 0.4f}, {"fltS", 0.6f},
        {"ampS", 0.85f}, {"ampR", 0.3f}, {"vMode", Mono}, {"vGlide", 0.08f},
        {"l1Rate", 5.f}, {"m1Src", SrcLfo1}, {"m1Dst", DstPitch}, {"m1Amt", 0.015f},
        {"dlMix", 0.3f}, {"dlSync", 3}, {"rvMix", 0.3f}, {"chMix", 0.3f} });
    add(P, RETRO, "Miami Pad", {
        {"o1Wave", Pulse}, {"o2Wave", Saw}, {"o2Level", 0.5f}, {"l2Rate", 0.5f},
        {"m1Src", SrcLfo2}, {"m1Dst", DstPW1}, {"m1Amt", 0.4f},
        {"fType", LP12}, {"fCutoff", 2400}, {"fEnv", 0.1f},
        {"ampA", 0.5f}, {"ampS", 0.9f}, {"ampR", 1.2f}, {"chMix", 0.6f}, {"chRate", 0.5f}, {"chDepth", 0.7f}, {"rvMix", 0.3f},
        {"subLevel", 0.2f}, {"o1PW", 0.4f}, {"l1Rate", 4.5f}, {"m2Src", SrcLfo1}, {"m2Dst", DstPitch}, {"m2Amt", 0.01f}, {"dlMix", 0.18f}, {"dlSync", 3}, {"fRes", 0.25f} });
    add(P, RETRO, "Arcade Bass-Lead", {
        {"o1Wave", Square}, {"o2Wave", Pulse}, {"o2PW", 0.25f}, {"o2Level", 0.3f},
        {"fType", LP12}, {"fCutoff", 1800}, {"fEnv", 0.6f}, {"fltD", 0.2f}, {"fltS", 0.3f},
        {"ampS", 0.8f}, {"ampR", 0.1f}, {"vMode", Mono}, {"bcBits", 8}, {"bcMix", 0.2f}, {"dlMix", 0.1f}, {"dlSync", 4} });
    add(P, RETRO, "Sunset Strings", {
        {"o1Wave", Saw}, {"o1Uni", 5}, {"o1Det", 16}, {"o1Spread", 0.9f},
        {"fType", LP24}, {"fCutoff", 2800}, {"fRes", 0.12f}, {"fEnv", 0.1f},
        {"ampA", 0.6f}, {"ampS", 0.9f}, {"ampR", 1.5f}, {"phMix", 0.25f}, {"phRate", 0.2f}, {"chMix", 0.4f}, {"rvMix", 0.35f} });

    // ===================================== 13. Arpeggiated synths =====================================
    add(P, ARP, "Arp Spark", {
        {"arpOn", 1}, {"arpRate", 2}, {"arpPat", 0}, {"arpOct", 2}, {"arpGate", 0.5f},
        {"o1Wave", Saw}, {"o1Uni", 2}, {"o1Det", 8},
        {"fType", LP24}, {"fCutoff", 2800}, {"fRes", 0.25f}, {"fEnv", 0.7f}, {"fltD", 0.2f}, {"fltS", 0},
        {"ampD", 0.25f}, {"ampS", 0}, {"ampR", 0.15f}, {"dlMix", 0.25f}, {"dlSync", 3}, {"rvMix", 0.2f} });
    add(P, ARP, "Arp Glass", {
        {"arpOn", 1}, {"arpRate", 1}, {"arpPat", 2}, {"arpOct", 2}, {"arpGate", 0.6f},
        {"o1Wave", Wavetable}, {"o1WT", 0.7f},
        {"fType", LP12}, {"fCutoff", 6000}, {"fEnv", 0.3f}, {"fltD", 0.2f}, {"fltS", 0.2f},
        {"ampD", 0.35f}, {"ampS", 0.1f}, {"ampR", 0.3f}, {"rvMix", 0.3f}, {"chMix", 0.2f} });
    add(P, ARP, "Arp Pulse", {
        {"arpOn", 1}, {"arpRate", 2}, {"arpPat", 0}, {"arpOct", 1}, {"arpGate", 0.45f},
        {"o1Wave", Pulse}, {"o1PW", 0.3f}, {"o2Wave", Square}, {"o2Oct", -1}, {"o2Level", 0.3f},
        {"fType", LP24}, {"fCutoff", 2200}, {"fRes", 0.3f}, {"fEnv", 0.6f}, {"fltD", 0.15f}, {"fltS", 0.1f},
        {"ampD", 0.2f}, {"ampS", 0.4f}, {"ampR", 0.1f}, {"dlMix", 0.2f}, {"dlSync", 4}, {"bcBits", 11}, {"bcMix", 0.1f} });
    add(P, ARP, "Arp Dream", {
        {"arpOn", 1}, {"arpRate", 4}, {"arpPat", 0}, {"arpOct", 3}, {"arpGate", 0.8f},
        {"o1Wave", Triangle}, {"o2Wave", Sine}, {"o2Oct", 1}, {"o2Level", 0.3f},
        {"fType", LP12}, {"fCutoff", 4000}, {"fEnv", 0.1f},
        {"ampA", 0.01f}, {"ampD", 0.5f}, {"ampS", 0.5f}, {"ampR", 0.6f}, {"rvMix", 0.45f}, {"chMix", 0.3f}, {"dlMix", 0.2f}, {"dlSync", 3} });
    add(P, ARP, "Arp Descend", {
        {"arpOn", 1}, {"arpRate", 2}, {"arpPat", 1}, {"arpOct", 2}, {"arpGate", 0.55f},
        {"o1Wave", Saw}, {"o1Uni", 2}, {"o1Det", 10},
        {"fType", LP24}, {"fCutoff", 2200}, {"fRes", 0.3f}, {"fEnv", 0.5f}, {"fltD", 0.2f}, {"fltS", 0.1f},
        {"ampD", 0.3f}, {"ampS", 0.2f}, {"ampR", 0.2f}, {"dlMix", 0.3f}, {"dlSync", 3}, {"rvMix", 0.2f},
        {"o1Wave", Pulse}, {"o1PW", 0.4f}, {"o2Wave", Saw}, {"o2Level", 0.3f}, {"o2Oct", 1}, {"vVelSens", 0.3f}, {"phMix", 0.2f}, {"phRate", 0.4f}, {"fKey", 0.6f} });
    add(P, ARP, "Arp Bells", {
        {"arpOn", 1}, {"arpRate", 1}, {"arpPat", 0}, {"arpOct", 2}, {"arpGate", 0.9f},
        {"o1Wave", Sine}, {"fmAmt", 0.3f}, {"fmRatio", 3.5f}, {"o1Oct", 1},
        {"fType", LP12}, {"fCutoff", 10000}, {"fEnv", 0},
        {"ampD", 1.2f}, {"ampS", 0}, {"ampR", 1.f}, {"rvMix", 0.45f}, {"rvSize", 0.8f}, {"dlMix", 0.2f}, {"dlSync", 2} });

    // ===================================== 14. Ambient textures =====================================
    add(P, AMBIENT, "Slow Shimmer", {
        {"o1Wave", Wavetable}, {"o1WT", 0.95f}, {"o1Uni", 5}, {"o1Det", 30}, {"o2Wave", Sine}, {"o2Oct", 2}, {"o2Level", 0.15f},
        {"fType", LP12}, {"fCutoff", 8000}, {"fRes", 0.1f}, {"fEnv", 0},
        {"l1Rate", 0.12f}, {"m1Src", SrcLfo1}, {"m1Dst", DstCutoff}, {"m1Amt", 0.3f},
        {"l2Rate", 0.09f}, {"m2Src", SrcLfo2}, {"m2Dst", DstWT1}, {"m2Amt", 0.2f},
        {"ampA", 2.5f}, {"ampS", 1}, {"ampR", 4.f}, {"rvMix", 0.6f}, {"rvSize", 0.95f}, {"dlMix", 0.3f}, {"dlSync", 3} });
    add(P, AMBIENT, "Texture Rain", {
        {"o1Wave", Sine}, {"o1Level", 0.4f}, {"nLevel", 0.4f}, {"nTone", 0.9f},
        {"fType", BP12}, {"fCutoff", 4000}, {"fRes", 0.3f}, {"fEnv", 0},
        {"l3Shape", LfoSH}, {"l3Rate", 3.f}, {"m1Src", SrcLfo3}, {"m1Dst", DstCutoff}, {"m1Amt", 0.3f},
        {"ampA", 1.f}, {"ampS", 1}, {"ampR", 2.f}, {"rvMix", 0.5f}, {"rvSize", 0.8f} });
    add(P, AMBIENT, "Tape Dust", {
        {"o1Wave", Triangle}, {"nLevel", 0.2f}, {"nTone", 0.5f},
        {"fType", LP12}, {"fCutoff", 3000}, {"fEnv", 0},
        {"ampA", 1.2f}, {"ampS", 1}, {"ampR", 2.f}, {"saAmt", 0.4f}, {"bcBits", 10}, {"bcMix", 0.15f}, {"chMix", 0.4f}, {"rvMix", 0.4f} });
    add(P, AMBIENT, "Drift Fog", {
        {"o1Wave", Saw}, {"o1Uni", 7}, {"o1Det", 35}, {"o1Spread", 1},
        {"fType", LP24}, {"fCutoff", 800}, {"fRes", 0.2f}, {"fEnv", 0},
        {"l1Rate", 0.1f}, {"m1Src", SrcLfo1}, {"m1Dst", DstCutoff}, {"m1Amt", 0.4f},
        {"ampA", 2.f}, {"ampS", 1}, {"ampR", 3.f}, {"phMix", 0.4f}, {"phRate", 0.08f}, {"rvMix", 0.55f}, {"rvSize", 0.9f} });
    add(P, AMBIENT, "Glass Drone", {
        {"o1Wave", Sine}, {"o2Wave", Wavetable}, {"o2WT", 0.8f}, {"o2Level", 0.5f}, {"o2Uni", 3}, {"o2Det", 20}, {"fmAmt", 0.1f}, {"fmRatio", 2},
        {"fType", LP12}, {"fCutoff", 5000}, {"fEnv", 0},
        {"ampA", 3.f}, {"ampS", 1}, {"ampR", 5.f}, {"rvMix", 0.6f}, {"rvSize", 0.95f}, {"stWidth", 1.4f} });
    add(P, AMBIENT, "Breathing Air", {
        {"o1Wave", Saw}, {"o1Level", 0.5f}, {"nLevel", 0.35f}, {"nTone", 0.7f},
        {"fType", BP12}, {"fCutoff", 1200}, {"fRes", 0.5f}, {"fEnv", 0},
        {"l1Rate", 0.2f}, {"m1Src", SrcLfo1}, {"m1Dst", DstCutoff}, {"m1Amt", 0.4f},
        {"ampA", 2.f}, {"ampS", 1}, {"ampR", 3.f}, {"rvMix", 0.5f}, {"rvSize", 0.85f}, {"chMix", 0.2f} });

    // ===================================== 15. Dreamy & emotional =====================================
    add(P, DREAMY, "Dream Sequence", {
        {"o1Wave", Saw}, {"o1Uni", 3}, {"o1Det", 14}, {"o2Wave", Triangle}, {"o2Level", 0.5f},
        {"fType", LP24}, {"fCutoff", 2500}, {"fRes", 0.15f}, {"fEnv", 0.2f}, {"fltD", 0.6f}, {"fltS", 0.5f},
        {"l1Rate", 0.3f}, {"m1Src", SrcLfo1}, {"m1Dst", DstCutoff}, {"m1Amt", 0.12f},
        {"ampA", 0.1f}, {"ampD", 0.6f}, {"ampS", 0.7f}, {"ampR", 1.f},
        {"dlMix", 0.3f}, {"dlSync", 3}, {"dlPing", 1}, {"rvMix", 0.4f}, {"chMix", 0.4f} });
    add(P, DREAMY, "Teardrop Keys", {
        {"o1Wave", Triangle}, {"o2Wave", Sine}, {"o2Oct", 1}, {"o2Level", 0.3f},
        {"fType", LP12}, {"fCutoff", 3000}, {"fEnv", 0.2f}, {"fltD", 0.8f}, {"fltS", 0.2f},
        {"ampD", 1.2f}, {"ampS", 0.15f}, {"ampR", 0.9f}, {"l1Rate", 4.8f}, {"m1Src", SrcLfo1}, {"m1Dst", DstPitch}, {"m1Amt", 0.01f},
        {"dlMix", 0.25f}, {"dlSync", 3}, {"rvMix", 0.45f}, {"chMix", 0.3f} });
    add(P, DREAMY, "Lonely Lead", {
        {"o1Wave", Wavetable}, {"o1WT", 0.3f},
        {"fType", LP24}, {"fCutoff", 3200}, {"fRes", 0.15f}, {"fEnv", 0.2f}, {"fltD", 0.4f}, {"fltS", 0.5f},
        {"ampA", 0.02f}, {"ampS", 0.85f}, {"ampR", 0.5f}, {"vMode", Legato}, {"vGlide", 0.08f},
        {"l1Rate", 5.f}, {"m1Src", SrcLfo1}, {"m1Dst", DstPitch}, {"m1Amt", 0.03f},
        {"dlMix", 0.3f}, {"dlSync", 3}, {"rvMix", 0.4f} });
    add(P, DREAMY, "Faded Memory", {
        {"o1Wave", Saw}, {"o2Wave", Square}, {"o2Fine", 8}, {"o2Level", 0.4f},
        {"fType", LP12}, {"fCutoff", 1300}, {"fEnv", 0.1f},
        {"ampA", 0.05f}, {"ampS", 0.8f}, {"ampR", 1.f}, {"bcBits", 12}, {"bcMix", 0.12f}, {"saAmt", 0.3f},
        {"chMix", 0.5f}, {"chRate", 0.35f}, {"rvMix", 0.4f}, {"fxLP", 7000} });
    add(P, DREAMY, "Heart Glow", {
        {"o1Wave", Saw}, {"o1Uni", 5}, {"o1Det", 22}, {"o2Wave", Sine}, {"o2Oct", 1}, {"o2Level", 0.25f},
        {"fType", LP24}, {"fCutoff", 3000}, {"fRes", 0.12f}, {"fEnv", 0.1f},
        {"ampA", 0.3f}, {"ampS", 0.9f}, {"ampR", 1.5f}, {"phMix", 0.2f}, {"phRate", 0.2f}, {"rvMix", 0.45f}, {"chMix", 0.25f},
        {"o1Wave", Wavetable}, {"o1WT", 0.15f}, {"fRes", 0.2f}, {"l1Rate", 0.25f}, {"m1Src", SrcLfo1}, {"m1Dst", DstCutoff}, {"m1Amt", 0.2f}, {"dlMix", 0.2f}, {"dlSync", 3} });
    add(P, DREAMY, "Moonlight Pluck", {
        {"o1Wave", Triangle}, {"o2Wave", Sine}, {"o2Oct", 1}, {"o2Level", 0.2f},
        {"fType", LP12}, {"fCutoff", 3500}, {"fEnv", 0.3f}, {"fltD", 0.3f}, {"fltS", 0},
        {"ampD", 0.9f}, {"ampS", 0}, {"ampR", 0.6f}, {"dlMix", 0.35f}, {"dlSync", 3}, {"dlFb", 0.5f}, {"rvMix", 0.5f}, {"rvSize", 0.75f} });
}
} // namespace

namespace
{
void buildPart3(std::vector<Preset>& P)
{
    // ===================================== 16. Distorted synths =====================================
    add(P, DIST, "Grit Lead", {
        {"o1Wave", Saw}, {"o1Uni", 3}, {"o1Det", 14},
        {"fType", LP24}, {"fCutoff", 3500}, {"fRes", 0.2f}, {"fEnv", 0.3f}, {"fltD", 0.3f}, {"fltS", 0.6f},
        {"ampS", 0.85f}, {"ampR", 0.15f}, {"dsType", 1}, {"dsDrive", 0.6f}, {"dsMix", 0.5f}, {"rvMix", 0.12f} });
    add(P, DIST, "Fuzz Pad", {
        {"o1Wave", Saw}, {"o1Uni", 5}, {"o1Det", 20},
        {"fType", LP24}, {"fCutoff", 1500}, {"fRes", 0.15f}, {"fEnv", 0.1f},
        {"ampA", 0.4f}, {"ampS", 0.9f}, {"ampR", 1.f}, {"dsType", 0}, {"dsDrive", 0.8f}, {"dsMix", 0.6f}, {"rvMix", 0.25f}, {"eqHigh", -3} });
    add(P, DIST, "Fold Lead", {
        {"o1Wave", Triangle}, {"o2Wave", Sine}, {"o2Oct", 1}, {"o2Level", 0.3f},
        {"fType", LP12}, {"fCutoff", 6000}, {"fEnv", 0.2f}, {"fltD", 0.3f},
        {"ampS", 0.85f}, {"ampR", 0.2f}, {"vMode", Mono}, {"vGlide", 0.04f},
        {"dsType", 2}, {"dsDrive", 0.55f}, {"dsMix", 0.7f}, {"dlMix", 0.15f}, {"dlSync", 2} });
    add(P, DIST, "Broken Radio", {
        {"o1Wave", Square}, {"o1Uni", 2}, {"o1Det", 20},
        {"fType", BP12}, {"fCutoff", 2000}, {"fRes", 0.4f}, {"fEnv", 0.2f},
        {"ampS", 0.8f}, {"ampR", 0.15f}, {"bcBits", 6}, {"bcRate", 6}, {"bcMix", 0.6f}, {"dsType", 1}, {"dsDrive", 0.4f}, {"dsMix", 0.3f} });
    add(P, DIST, "Overdrive Stab", {
        {"o1Wave", Saw}, {"o2Wave", Pulse}, {"o2PW", 0.4f}, {"o2Level", 0.5f},
        {"fType", LP24}, {"fCutoff", 2200}, {"fRes", 0.25f}, {"fDrive", 0.6f}, {"fEnv", 0.8f}, {"fltD", 0.2f}, {"fltS", 0},
        {"ampD", 0.3f}, {"ampS", 0}, {"ampR", 0.15f}, {"saAmt", 0.5f}, {"dsMix", 0.3f}, {"rvMix", 0.15f},
        {"vMode", Mono}, {"o1Uni", 3}, {"o1Det", 20}, {"fKey", 0.5f}, {"eqMid", 3}, {"eqMidFreq", 1800}, {"cpRatio", 4}, {"cpThresh", -16}, {"vVelSens", 0.9f} });
    add(P, DIST, "Screamer", {
        {"o1Wave", Saw}, {"o1Uni", 2}, {"fmAmt", 0.3f}, {"fmRatio", 2},
        {"fType", LP24}, {"fCutoff", 4000}, {"fRes", 0.3f}, {"fEnv", 0.3f},
        {"l1Rate", 0.7f}, {"m1Src", SrcLfo1}, {"m1Dst", DstCutoff}, {"m1Amt", 0.25f},
        {"ampS", 0.85f}, {"ampR", 0.2f}, {"dsType", 0}, {"dsDrive", 0.9f}, {"dsMix", 0.6f}, {"eqMid", 3}, {"eqMidFreq", 2500} });

    // ===================================== 17. Bass synths =====================================
    add(P, BASS, "Sub Pulse", {
        {"o1Wave", Square}, {"o1Oct", -1}, {"subLevel", 0.6f}, {"subWave", SubSine},
        {"fType", LP24}, {"fCutoff", 400}, {"fRes", 0.15f}, {"fEnv", 0.5f}, {"fltD", 0.2f}, {"fltS", 0.3f},
        {"ampA", 0.003f}, {"ampD", 0.3f}, {"ampS", 0.8f}, {"ampR", 0.15f}, {"vMode", Mono}, {"saAmt", 0.2f} });
    add(P, BASS, "Saw Bass", {
        {"o1Wave", Saw}, {"o1Oct", -1}, {"o1Uni", 2}, {"o1Det", 8}, {"o1Spread", 0.2f},
        {"fType", LP24}, {"fCutoff", 600}, {"fRes", 0.2f}, {"fEnv", 0.7f}, {"fltD", 0.3f}, {"fltS", 0.3f},
        {"ampS", 0.85f}, {"ampR", 0.12f}, {"vMode", Mono}, {"dsMix", 0.2f}, {"dsDrive", 0.3f}, {"cpThresh", -14}, {"cpRatio", 3} });
    add(P, BASS, "Round Bass", {
        {"o1Wave", Triangle}, {"o1Oct", -1}, {"subLevel", 0.5f},
        {"fType", LP12}, {"fCutoff", 500}, {"fEnv", 0.3f}, {"fltD", 0.3f},
        {"ampS", 0.9f}, {"ampR", 0.2f}, {"vMode", Legato}, {"vGlide", 0.05f}, {"saAmt", 0.3f} });
    add(P, BASS, "Reese Lite", {
        {"o1Wave", Saw}, {"o1Oct", -1}, {"o1Uni", 3}, {"o1Det", 25}, {"o2Wave", Saw}, {"o2Oct", -1}, {"o2Fine", 14}, {"o2Level", 0.5f},
        {"fType", LP24}, {"fCutoff", 700}, {"fRes", 0.25f}, {"fEnv", 0.1f},
        {"l1Rate", 0.3f}, {"m1Src", SrcLfo1}, {"m1Dst", DstCutoff}, {"m1Amt", 0.25f},
        {"ampS", 0.9f}, {"ampR", 0.2f}, {"vMode", Mono}, {"chMix", 0.15f}, {"saAmt", 0.3f} });
    add(P, BASS, "FM Bass", {
        {"o1Wave", Sine}, {"o1Oct", -1}, {"fmAmt", 0.35f}, {"fmRatio", 1}, {"subLevel", 0.4f},
        {"fType", LP12}, {"fCutoff", 1500}, {"fEnv", 0.2f},
        {"m1Src", SrcFiltEnv}, {"m1Dst", DstFM}, {"m1Amt", 0.3f}, {"fltD", 0.25f}, {"fltS", 0.2f},
        {"ampD", 0.5f}, {"ampS", 0.7f}, {"ampR", 0.15f}, {"vMode", Mono} });
    add(P, BASS, "Pluck Bass", {
        {"o1Wave", Pulse}, {"o1PW", 0.35f}, {"o1Oct", -1}, {"subLevel", 0.35f},
        {"fType", LP24}, {"fCutoff", 900}, {"fRes", 0.25f}, {"fEnv", 0.9f}, {"fltD", 0.15f}, {"fltS", 0},
        {"ampD", 0.28f}, {"ampS", 0}, {"ampR", 0.1f}, {"vMode", Mono}, {"saAmt", 0.2f} });

    // ===================================== 18. 808-style basses =====================================
    add(P, B808, "808 Classic", {
        {"o1Wave", Sine}, {"o1Oct", -1}, {"o1Level", 0.9f},
        {"fType", LP12}, {"fCutoff", 1200}, {"fEnv", 0}, {"fKey", 0},
        {"m1Src", SrcFiltEnv}, {"m1Dst", DstPitch}, {"m1Amt", 0.45f}, {"fltA", 0.001f}, {"fltD", 0.07f}, {"fltS", 0},
        {"ampA", 0.002f}, {"ampD", 1.8f}, {"ampS", 0}, {"ampR", 0.3f}, {"vMode", Mono}, {"vGlide", 0.04f},
        {"saAmt", 0.35f}, {"cpThresh", -12}, {"cpRatio", 3},
        {"eqLow", 2}, {"vVelSens", 0.5f}, {"ampR", 0.2f} });
    add(P, B808, "808 Distorted", {
        {"o1Wave", Sine}, {"o1Oct", -1}, {"o1Level", 0.9f}, {"o2Wave", Triangle}, {"o2Oct", -1}, {"o2Level", 0.25f},
        {"fType", LP12}, {"fCutoff", 1500}, {"fEnv", 0}, {"fKey", 0},
        {"m1Src", SrcFiltEnv}, {"m1Dst", DstPitch}, {"m1Amt", 0.4f}, {"fltD", 0.08f}, {"fltS", 0},
        {"ampD", 1.6f}, {"ampS", 0}, {"ampR", 0.3f}, {"vMode", Mono}, {"vGlide", 0.03f},
        {"dsType", 0}, {"dsDrive", 0.5f}, {"dsMix", 0.45f}, {"eqLow", 3}, {"fxLP", 9000} });
    add(P, B808, "808 Glide", {
        {"o1Wave", Sine}, {"o1Oct", -1}, {"o1Level", 0.9f}, {"subLevel", 0.2f},
        {"fType", LP12}, {"fCutoff", 1000}, {"fEnv", 0}, {"fKey", 0},
        {"ampD", 2.5f}, {"ampS", 0}, {"ampR", 0.4f}, {"vMode", Legato}, {"vGlide", 0.12f}, {"saAmt", 0.3f},
        {"m1Src", SrcFiltEnv}, {"m1Dst", DstPitch}, {"m1Amt", 0.25f}, {"fltD", 0.06f}, {"fltS", 0},
        {"fType", LP24}, {"dsMix", 0.15f}, {"dsDrive", 0.3f}, {"eqLow", 2}, {"ampA", 0.004f}, {"vBend", 12}, {"cpThresh", -14}, {"cpRatio", 2} });
    add(P, B808, "808 Boom", {
        {"o1Wave", Sine}, {"o1Oct", -1}, {"o1Level", 0.85f}, {"o2Wave", Triangle}, {"o2Oct", -2}, {"o2Level", 0.3f},
        {"fType", LP12}, {"fCutoff", 800}, {"fEnv", 0}, {"fKey", 0},
        {"m1Src", SrcFiltEnv}, {"m1Dst", DstPitch}, {"m1Amt", 0.8f}, {"fltD", 0.1f}, {"fltS", 0},
        {"ampD", 1.2f}, {"ampS", 0}, {"ampR", 0.25f}, {"vMode", Mono}, {"saAmt", 0.2f}, {"cpThresh", -10}, {"cpRatio", 4} });
    add(P, B808, "808 Long", {
        {"o1Wave", Sine}, {"o1Oct", -1}, {"o1Level", 0.9f},
        {"fType", LP12}, {"fCutoff", 300}, {"fEnv", 0}, {"fKey", 0},
        {"m1Src", SrcFiltEnv}, {"m1Dst", DstPitch}, {"m1Amt", 0.3f}, {"fltD", 0.05f}, {"fltS", 0},
        {"ampD", 4.f}, {"ampS", 0}, {"ampR", 0.5f}, {"vMode", Mono}, {"vGlide", 0.05f}, {"saAmt", 0.25f},
        {"o2Wave", Sine}, {"o2Oct", -2}, {"o2Level", 0.3f}, {"eqLow", 4}, {"eqHigh", -4}, {"fxLP", 6000}, {"vVelSens", 0.3f}, {"ampR", 0.8f}, {"fType", LP24} });
    add(P, B808, "808 Click", {
        {"o1Wave", Sine}, {"o1Oct", -1}, {"o1Level", 0.9f}, {"fmAmt", 0.0f}, {"fmRatio", 4},
        {"fType", LP12}, {"fCutoff", 1400}, {"fEnv", 0}, {"fKey", 0},
        {"m1Src", SrcFiltEnv}, {"m1Dst", DstPitch}, {"m1Amt", 0.4f},
        {"m2Src", SrcFiltEnv}, {"m2Dst", DstFM}, {"m2Amt", 0.35f}, {"fltD", 0.04f}, {"fltS", 0},
        {"ampD", 1.5f}, {"ampS", 0}, {"ampR", 0.25f}, {"vMode", Mono}, {"saAmt", 0.3f},
        {"o2Wave", Pulse}, {"o2Oct", 1}, {"o2Level", 0.08f}, {"fxHP", 25}, {"vVelSens", 0.8f}, {"eqHigh", 2}, {"ampS", 0.0f}, {"rvMix", 0.05f}, {"rvSize", 0.2f} });

    // ===================================== 19. Vocal-like formant synths =====================================
    add(P, VOCAL, "Aah Choir", {
        {"o1Wave", Saw}, {"o1Uni", 5}, {"o1Det", 14}, {"o1Spread", 0.8f},
        {"fType", Formant}, {"fCutoff", 2000}, {"fRes", 0.3f}, {"fVowel", 0}, {"fEnv", 0}, {"fKey", 0},
        {"ampA", 0.35f}, {"ampS", 0.9f}, {"ampR", 1.2f}, {"chMix", 0.3f}, {"rvMix", 0.4f},
        {"o2Wave", Wavetable}, {"o2WT", 0.8f}, {"o2Level", 0.2f}, {"o2Oct", 1}, {"l1Rate", 0.3f}, {"m1Src", SrcLfo1}, {"m1Dst", DstVowel}, {"m1Amt", 0.05f}, {"subLevel", 0.1f} });
    add(P, VOCAL, "Ooh Pad", {
        {"o1Wave", Saw}, {"o1Uni", 3}, {"o1Det", 12}, {"o2Wave", Triangle}, {"o2Level", 0.3f},
        {"fType", Formant}, {"fCutoff", 2000}, {"fRes", 0.25f}, {"fVowel", 1}, {"fEnv", 0}, {"fKey", 0},
        {"ampA", 0.5f}, {"ampS", 0.9f}, {"ampR", 1.5f}, {"rvMix", 0.45f}, {"chMix", 0.25f},
        {"subLevel", 0.15f}, {"l2Rate", 0.2f}, {"m1Src", SrcLfo2}, {"m1Dst", DstCutoff}, {"m1Amt", 0.15f}, {"eqLow", 2}, {"fxLP", 9000}, {"o1Spread", 1} });
    add(P, VOCAL, "Vowel Sweep", {
        {"o1Wave", Saw}, {"o1Uni", 2}, {"o1Det", 10},
        {"fType", Formant}, {"fCutoff", 2000}, {"fRes", 0.35f}, {"fVowel", 0.4f}, {"fEnv", 0}, {"fKey", 0},
        {"l1Rate", 0.4f}, {"m1Src", SrcLfo1}, {"m1Dst", DstVowel}, {"m1Amt", 0.6f},
        {"ampA", 0.1f}, {"ampS", 1}, {"ampR", 0.6f}, {"dlMix", 0.2f}, {"dlSync", 3}, {"rvMix", 0.3f} });
    add(P, VOCAL, "Talk Lead", {
        {"o1Wave", Pulse}, {"o1PW", 0.4f}, {"o2Wave", Saw}, {"o2Level", 0.5f},
        {"fType", Formant}, {"fCutoff", 2200}, {"fRes", 0.35f}, {"fVowel", 0.2f}, {"fEnv", 0}, {"fKey", 0},
        {"m1Src", SrcFiltEnv}, {"m1Dst", DstVowel}, {"m1Amt", 0.5f}, {"fltD", 0.25f}, {"fltS", 0.3f},
        {"ampA", 0.01f}, {"ampS", 0.85f}, {"ampR", 0.2f}, {"vMode", Legato}, {"vGlide", 0.05f},
        {"l1Rate", 5.f}, {"m2Src", SrcLfo1}, {"m2Dst", DstPitch}, {"m2Amt", 0.02f}, {"rvMix", 0.2f} });
    add(P, VOCAL, "Eee Synth", {
        {"o1Wave", Saw}, {"o1Uni", 2}, {"o1Det", 8},
        {"fType", Formant}, {"fCutoff", 2300}, {"fRes", 0.5f}, {"fVowel", 0.5f}, {"fEnv", 0}, {"fKey", 0},
        {"ampA", 0.02f}, {"ampS", 0.85f}, {"ampR", 0.3f}, {"chMix", 0.3f}, {"rvMix", 0.25f},
        {"o1Wave", Pulse}, {"o1PW", 0.35f}, {"nLevel", 0.04f}, {"nTone", 0.8f}, {"vMode", Legato}, {"vGlide", 0.03f}, {"l2Rate", 5.f}, {"m1Src", SrcLfo2}, {"m1Dst", DstPitch}, {"m1Amt", 0.012f} });
    add(P, VOCAL, "Robot Choir", {
        {"o1Wave", Square}, {"o1Uni", 4}, {"o1Det", 16},
        {"fType", Formant}, {"fCutoff", 1800}, {"fRes", 0.4f}, {"fVowel", 0.75f}, {"fEnv", 0}, {"fKey", 0},
        {"ampA", 0.15f}, {"ampS", 0.9f}, {"ampR", 0.6f}, {"bcBits", 9}, {"bcRate", 2}, {"bcMix", 0.3f}, {"rvMix", 0.35f} });

    // ===================================== 20. Experimental sound design =====================================
    add(P, EXPERIMENT, "Sync Scream", {
        {"o1Wave", Saw}, {"o1Sync", 1}, {"o2Wave", Saw}, {"o2Oct", -1}, {"o2Level", 0},
        {"m1Src", SrcFiltEnv}, {"m1Dst", DstPitch1}, {"m1Amt", 0.8f}, {"fltD", 0.6f}, {"fltS", 0.3f},
        {"fType", LP24}, {"fCutoff", 6000}, {"fRes", 0.15f}, {"fEnv", 0},
        {"ampA", 0.005f}, {"ampD", 0.5f}, {"ampS", 0.8f}, {"ampR", 0.25f}, {"dlMix", 0.15f}, {"dlSync", 2}, {"rvMix", 0.15f} });
    add(P, EXPERIMENT, "FM Alien", {
        {"o1Wave", Sine}, {"fmAmt", 0.7f}, {"fmRatio", 1.41f}, {"o2Wave", Sine}, {"o2Oct", 1}, {"o2Semi", 5}, {"o2Level", 0.2f},
        {"l3Shape", LfoSine}, {"l3Rate", 0.8f}, {"m1Src", SrcLfo3}, {"m1Dst", DstFM}, {"m1Amt", 0.3f},
        {"fType", LP12}, {"fCutoff", 9000}, {"fEnv", 0},
        {"ampA", 0.02f}, {"ampS", 0.8f}, {"ampR", 0.4f}, {"phMix", 0.35f}, {"phRate", 0.7f}, {"rvMix", 0.3f} });
    add(P, EXPERIMENT, "Wavetable Rain", {
        {"o1Wave", Wavetable}, {"o1WT", 0.5f}, {"o1Uni", 2}, {"o1Det", 10},
        {"l3Shape", LfoSH}, {"l3Rate", 8.f}, {"m1Src", SrcLfo3}, {"m1Dst", DstWT1}, {"m1Amt", 0.6f},
        {"fType", LP24}, {"fCutoff", 5000}, {"fRes", 0.2f}, {"fEnv", 0.3f}, {"fltD", 0.25f}, {"fltS", 0},
        {"ampD", 0.4f}, {"ampS", 0.1f}, {"ampR", 0.3f}, {"dlMix", 0.3f}, {"dlSync", 4}, {"dlFb", 0.5f}, {"rvMix", 0.3f} });
    add(P, EXPERIMENT, "Noise Sculpt", {
        {"o1Wave", Square}, {"o1Level", 0.5f}, {"nLevel", 0.5f}, {"nTone", 0.3f},
        {"fType", BP12}, {"fCutoff", 1500}, {"fRes", 0.7f}, {"fEnv", 0},
        {"l2Rate", 0.6f}, {"m1Src", SrcLfo2}, {"m1Dst", DstCutoff}, {"m1Amt", 0.5f},
        {"ampA", 0.3f}, {"ampS", 0.9f}, {"ampR", 1.f}, {"rvMix", 0.4f}, {"flMix", 0.2f}, {"flRate", 0.1f} });
    add(P, EXPERIMENT, "Metal Machine", {
        {"o1Wave", Square}, {"fmAmt", 0.6f}, {"fmRatio", 5.43f}, {"o2Wave", Square}, {"o2Oct", 1}, {"o2Fine", 23}, {"o2Level", 0.3f},
        {"fType", Notch}, {"fCutoff", 1500}, {"fRes", 0.4f}, {"fEnv", 0},
        {"l1Rate", 0.5f}, {"m1Src", SrcLfo1}, {"m1Dst", DstCutoff}, {"m1Amt", 0.5f},
        {"ampS", 0.8f}, {"ampR", 0.4f}, {"dsType", 2}, {"dsDrive", 0.4f}, {"dsMix", 0.4f}, {"rvMix", 0.25f} });
    add(P, EXPERIMENT, "Stutter Gate", {
        {"o1Wave", Saw}, {"o1Uni", 3}, {"o1Det", 14},
        {"l3Shape", LfoSquare}, {"l3Rate", 8.f}, {"m1Src", SrcLfo3}, {"m1Dst", DstAmp}, {"m1Amt", -0.5f},
        {"fType", LP24}, {"fCutoff", 3000}, {"fRes", 0.2f}, {"fEnv", 0.2f},
        {"ampS", 0.85f}, {"ampR", 0.2f}, {"dlMix", 0.2f}, {"dlSync", 2}, {"rvMix", 0.2f} });
}

#include "PresetsNew.inc"
} // namespace

// ------------------------------------------------------------------------------------------
const juce::StringArray& categoryNames()
{
    static const juce::StringArray names {
        "Bright Digital Plucks", "Nasal Buzzy Leads", "Kazoo & Trumpet Leads", "Soft Melodic Keys", "Bells & Mallets",
        "Warm Analog Synths", "Airy Background Pads", "Dark Atmospheric Pads", "Long Sustained Synths", "Counter-Melody Synths",
        "Glassy Digital Leads", "Retro 80s Synths", "Arpeggiated Synths", "Ambient Textures", "Dreamy & Emotional",
        "Distorted Synths", "Bass Synths", "808 Basses", "Vocal Formant Synths", "Experimental" };
    return names;
}

const juce::StringArray& genreNames()
{
    static const juce::StringArray n { "HIP-HOP", "MELODIC TRAP", "R&B", "JAZZ", "AMBIENT", "POP" };
    return n;
}
const juce::StringArray& groupNames()
{
    static const juce::StringArray n { "Plucks", "Leads", "Pads", "Keys", "Bells", "Textures", "Bass" };
    return n;
}
juce::String genreStarterPreset(int g)
{
    static const char* s[kNumGenres] = { "Warm Hip-Hop Keys", "Plan A Pluck", "Silk E-Piano", "Mellow Jazz EP", "Endless Air", "Bright Pop Pluck" };
    return s[juce::jlimit(0, kNumGenres - 1, g)];
}

int categoryIndexOf(const juce::String& category) { return categoryNames().indexOf(category); }

const juce::StringArray& presetBuildErrors() { factoryPresets(); return errorLog(); }

const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets = [] {
        std::vector<Preset> p;
        p.reserve(128);
        buildPart4(p);        // the new "front page" library first: Melodic Trap core, plucks, pads, genres
        buildPart1(p);
        buildPart2(p);
        buildPart3(p);
        return p;
    }();
    return presets;
}

std::vector<float> resolveValues(const Preset& p)
{
    std::vector<float> v((size_t) kNumParams);
    for (int i = 0; i < kNumParams; ++i) v[(size_t) i] = paramDefault(i);
    for (const auto& e : p.values)
    {
        const auto& d = paramDef(e.first);
        float x = e.second;
        if (d.type == ParamType::Choice) x = juce::jlimit(0.f, (float) (juce::StringArray::fromTokens(d.choices, "|", "").size() - 1), x);
        else x = juce::jlimit(d.minV, d.maxV, x);
        v[(size_t) e.first] = x;
    }
    return v;
}
} // namespace aura
