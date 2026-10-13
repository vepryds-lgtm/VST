#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// =====================================================================================
//  AuraSynth - single source of truth for every parameter.
//  F(id, name, min, max, default, skew)     float
//  C(id, name, "A|B|C", defaultIndex)       choice
//  B(id, name, default)                     bool
//  I(id, name, min, max, default)           int
// =====================================================================================
#define AURA_WAVES    "Saw|Square|Triangle|Sine|Pulse|Wavetable"
#define AURA_SUBWAVES "Sine|Triangle|Square"
#define AURA_FILTERS  "LP 12|LP 24|HP 12|BP 12|Notch|Formant"
#define AURA_LFOSHAPE "Sine|Triangle|Saw|Square|S&H"
#define AURA_VMODE    "Poly|Mono|Legato"
#define AURA_MODSRC   "None|LFO 1|LFO 2|LFO 3|Filter Env|Amp Env|Velocity|Mod Wheel|Key Track"
#define AURA_MODDST   "None|Pitch|Cutoff|Resonance|Osc1 WT|Osc2 WT|Osc1 PW|Osc2 PW|Osc1 Level|Osc2 Level|FM Amount|Amp Level|Detune|Vowel|Osc1 Pitch|Osc2 Pitch"
#define AURA_ARPRATE  "1/4|1/8|1/16|1/32|1/8T|1/16T"
#define AURA_ARPPAT   "Up|Down|Up/Down|Random"
#define AURA_DISTTYPE "Soft|Hard|Fold"
#define AURA_DLSYNC   "Free|1/4|1/8|1/8 dotted|1/16|1/4 dotted"

#define AURA_MODSLOT(F, C, n)                                                          \
    C(m##n##Src, "Mod " #n " Source", AURA_MODSRC, 0)                                  \
    C(m##n##Dst, "Mod " #n " Dest", AURA_MODDST, 0)                                    \
    F(m##n##Amt, "Mod " #n " Amount", -1.f, 1.f, 0.f, 1.f)

#define AURA_PARAMS(F, C, B, I)                                                        \
    /* ---------------- Oscillator 1 ---------------- */                               \
    F(o1Level, "Osc1 Level", 0.f, 1.f, 0.8f, 1.f)                                      \
    C(o1Wave, "Osc1 Wave", AURA_WAVES, 0)                                              \
    I(o1Oct, "Osc1 Octave", -3, 3, 0)                                                  \
    I(o1Semi, "Osc1 Semi", -12, 12, 0)                                                 \
    F(o1Fine, "Osc1 Fine", -100.f, 100.f, 0.f, 1.f)                                    \
    F(o1PW, "Osc1 Pulse Width", 0.05f, 0.95f, 0.5f, 1.f)                               \
    F(o1WT, "Osc1 WT Position", 0.f, 1.f, 0.f, 1.f)                                    \
    I(o1Uni, "Osc1 Unison Voices", 1, 7, 1)                                            \
    F(o1Det, "Osc1 Unison Detune", 0.f, 100.f, 15.f, 1.f)                              \
    F(o1Spread, "Osc1 Stereo Spread", 0.f, 1.f, 0.5f, 1.f)                             \
    B(o1Sync, "Osc1 Hard Sync", false)                                                 \
    /* ---------------- Oscillator 2 ---------------- */                               \
    F(o2Level, "Osc2 Level", 0.f, 1.f, 0.f, 1.f)                                       \
    C(o2Wave, "Osc2 Wave", AURA_WAVES, 0)                                              \
    I(o2Oct, "Osc2 Octave", -3, 3, 0)                                                  \
    I(o2Semi, "Osc2 Semi", -12, 12, 0)                                                 \
    F(o2Fine, "Osc2 Fine", -100.f, 100.f, 0.f, 1.f)                                    \
    F(o2PW, "Osc2 Pulse Width", 0.05f, 0.95f, 0.5f, 1.f)                               \
    F(o2WT, "Osc2 WT Position", 0.f, 1.f, 0.f, 1.f)                                    \
    I(o2Uni, "Osc2 Unison Voices", 1, 7, 1)                                            \
    F(o2Det, "Osc2 Unison Detune", 0.f, 100.f, 15.f, 1.f)                              \
    F(o2Spread, "Osc2 Stereo Spread", 0.f, 1.f, 0.5f, 1.f)                             \
    /* ---------------- FM / Sub / Noise ---------------- */                           \
    F(fmAmt, "FM Amount", 0.f, 1.f, 0.f, 1.f)                                          \
    F(fmRatio, "FM Ratio", 0.25f, 8.f, 1.f, 0.6f)                                      \
    F(subLevel, "Sub Level", 0.f, 1.f, 0.f, 1.f)                                       \
    C(subWave, "Sub Wave", AURA_SUBWAVES, 0)                                           \
    I(subOct, "Sub Octave", -2, -1, -1)                                                \
    F(nLevel, "Noise Level", 0.f, 1.f, 0.f, 0.6f)                                      \
    F(nTone, "Noise Tone", 0.f, 1.f, 0.6f, 1.f)                                        \
    /* ---------------- Filter ---------------- */                                     \
    C(fType, "Filter Type", AURA_FILTERS, 1)                                           \
    F(fCutoff, "Filter Cutoff", 20.f, 20000.f, 8000.f, 0.3f)                           \
    F(fRes, "Filter Resonance", 0.f, 1.f, 0.15f, 1.f)                                  \
    F(fDrive, "Filter Drive", 0.f, 1.f, 0.f, 1.f)                                      \
    F(fKey, "Filter Key Track", 0.f, 1.f, 0.3f, 1.f)                                   \
    F(fEnv, "Filter Env Amount", -1.f, 1.f, 0.3f, 1.f)                                 \
    F(fVowel, "Formant Vowel", 0.f, 1.f, 0.f, 1.f)                                     \
    /* ---------------- Envelopes ---------------- */                                  \
    F(ampA, "Amp Attack", 0.001f, 10.f, 0.005f, 0.3f)                                  \
    F(ampD, "Amp Decay", 0.001f, 10.f, 0.4f, 0.3f)                                     \
    F(ampS, "Amp Sustain", 0.f, 1.f, 0.7f, 1.f)                                        \
    F(ampR, "Amp Release", 0.001f, 15.f, 0.25f, 0.3f)                                  \
    F(fltA, "Filter Attack", 0.001f, 10.f, 0.005f, 0.3f)                               \
    F(fltD, "Filter Decay", 0.001f, 10.f, 0.4f, 0.3f)                                  \
    F(fltS, "Filter Sustain", 0.f, 1.f, 0.3f, 1.f)                                     \
    F(fltR, "Filter Release", 0.001f, 15.f, 0.3f, 0.3f)                                \
    /* ---------------- LFOs ---------------- */                                       \
    C(l1Shape, "LFO1 Shape", AURA_LFOSHAPE, 0)                                         \
    F(l1Rate, "LFO1 Rate", 0.05f, 25.f, 2.f, 0.4f)                                     \
    C(l2Shape, "LFO2 Shape", AURA_LFOSHAPE, 0)                                         \
    F(l2Rate, "LFO2 Rate", 0.05f, 25.f, 0.5f, 0.4f)                                    \
    C(l3Shape, "LFO3 Shape", AURA_LFOSHAPE, 1)                                         \
    F(l3Rate, "LFO3 Rate", 0.05f, 25.f, 5.f, 0.4f)                                     \
    /* ---------------- Mod matrix ---------------- */                                 \
    AURA_MODSLOT(F, C, 1) AURA_MODSLOT(F, C, 2) AURA_MODSLOT(F, C, 3)                  \
    AURA_MODSLOT(F, C, 4) AURA_MODSLOT(F, C, 5) AURA_MODSLOT(F, C, 6)                  \
    /* ---------------- Voice ---------------- */                                      \
    C(vMode, "Voice Mode", AURA_VMODE, 0)                                              \
    F(vGlide, "Glide Time", 0.f, 2.f, 0.f, 0.4f)                                       \
    I(vBend, "Pitch Bend Range", 0, 24, 2)                                             \
    F(vVelSens, "Velocity Sensitivity", 0.f, 1.f, 0.6f, 1.f)                           \
    /* ---------------- Arpeggiator ---------------- */                                \
    B(arpOn, "Arp On", false)                                                          \
    C(arpRate, "Arp Rate", AURA_ARPRATE, 2)                                            \
    C(arpPat, "Arp Pattern", AURA_ARPPAT, 0)                                           \
    I(arpOct, "Arp Octaves", 1, 3, 1)                                                  \
    F(arpGate, "Arp Gate", 0.05f, 1.f, 0.6f, 1.f)                                      \
    /* ---------------- Effects ---------------- */                                    \
    C(dsType, "Distortion Type", AURA_DISTTYPE, 0)                                     \
    F(dsDrive, "Distortion Drive", 0.f, 1.f, 0.3f, 1.f)                                \
    F(dsMix, "Distortion Mix", 0.f, 1.f, 0.f, 1.f)                                     \
    F(saAmt, "Saturation", 0.f, 1.f, 0.f, 1.f)                                         \
    F(bcBits, "Bitcrush Bits", 2.f, 16.f, 16.f, 1.f)                                   \
    F(bcRate, "Bitcrush Downsample", 1.f, 32.f, 1.f, 0.5f)                             \
    F(bcMix, "Bitcrush Mix", 0.f, 1.f, 0.f, 1.f)                                       \
    F(phRate, "Phaser Rate", 0.05f, 8.f, 0.4f, 0.4f)                                   \
    F(phDepth, "Phaser Depth", 0.f, 1.f, 0.7f, 1.f)                                    \
    F(phFb, "Phaser Feedback", 0.f, 0.95f, 0.4f, 1.f)                                  \
    F(phMix, "Phaser Mix", 0.f, 1.f, 0.f, 1.f)                                         \
    F(flRate, "Flanger Rate", 0.05f, 8.f, 0.3f, 0.4f)                                  \
    F(flDepth, "Flanger Depth", 0.f, 1.f, 0.6f, 1.f)                                   \
    F(flFb, "Flanger Feedback", -0.95f, 0.95f, 0.4f, 1.f)                              \
    F(flMix, "Flanger Mix", 0.f, 1.f, 0.f, 1.f)                                        \
    F(chRate, "Chorus Rate", 0.05f, 6.f, 0.8f, 0.4f)                                   \
    F(chDepth, "Chorus Depth", 0.f, 1.f, 0.5f, 1.f)                                    \
    F(chMix, "Chorus Mix", 0.f, 1.f, 0.f, 1.f)                                         \
    F(eqLow, "EQ Low Gain", -12.f, 12.f, 0.f, 1.f)                                     \
    F(eqMid, "EQ Mid Gain", -12.f, 12.f, 0.f, 1.f)                                     \
    F(eqMidFreq, "EQ Mid Freq", 200.f, 8000.f, 1200.f, 0.4f)                           \
    F(eqHigh, "EQ High Gain", -12.f, 12.f, 0.f, 1.f)                                   \
    F(fxHP, "FX High-Pass", 20.f, 2000.f, 20.f, 0.35f)                                 \
    F(fxLP, "FX Low-Pass", 500.f, 20000.f, 20000.f, 0.35f)                             \
    F(cpThresh, "Comp Threshold", -40.f, 0.f, 0.f, 1.f)                                \
    F(cpRatio, "Comp Ratio", 1.f, 20.f, 1.f, 0.5f)                                     \
    F(cpRel, "Comp Release", 20.f, 800.f, 150.f, 0.5f)                                 \
    F(dlTime, "Delay Time", 1.f, 1500.f, 320.f, 0.5f)                                  \
    C(dlSync, "Delay Sync", AURA_DLSYNC, 0)                                            \
    F(dlFb, "Delay Feedback", 0.f, 0.95f, 0.35f, 1.f)                                  \
    F(dlTone, "Delay Tone", 500.f, 18000.f, 6000.f, 0.4f)                              \
    B(dlPing, "Delay Ping-Pong", true)                                                 \
    F(dlMix, "Delay Mix", 0.f, 1.f, 0.f, 1.f)                                          \
    F(rvSize, "Reverb Size", 0.f, 1.f, 0.5f, 1.f)                                      \
    F(rvDamp, "Reverb Damping", 0.f, 1.f, 0.5f, 1.f)                                   \
    F(rvWidth, "Reverb Width", 0.f, 1.f, 1.f, 1.f)                                     \
    F(rvMix, "Reverb Mix", 0.f, 1.f, 0.f, 1.f)                                         \
    F(stWidth, "Stereo Width", 0.f, 2.f, 1.f, 1.f)                                     \
    /* ---------------- Simple macros / character ---------------- */                    \
    F(bright, "Brightness", 0.f, 1.f, 0.5f, 1.f)                                       \
    F(warmth, "Warmth", 0.f, 1.f, 0.f, 1.f)                                            \
    F(movement, "Movement", 0.f, 1.f, 0.f, 1.f)                                        \
    F(vDrift, "Analog Drift", 0.f, 1.f, 0.f, 1.f)                                      \
    /* ---------------- Layer B (Osc 2 with its own envelope + filter) --------------- */ \
    B(layOn, "Layer B On", false)                                                      \
    F(layCut, "Layer B Cutoff", 100.f, 12000.f, 2500.f, 0.3f)                          \
    F(layA, "Layer B Attack", 0.001f, 10.f, 0.6f, 0.3f)                                \
    F(layD, "Layer B Decay", 0.001f, 10.f, 1.0f, 0.3f)                                 \
    F(layS, "Layer B Sustain", 0.f, 1.f, 0.8f, 1.f)                                    \
    F(layR, "Layer B Release", 0.001f, 15.f, 1.2f, 0.3f)                               \
    F(master, "Preset Gain", 0.f, 2.f, 0.7f, 1.f)                                      \
    F(volume, "Volume", 0.f, 1.f, 0.8f, 1.f)

namespace aura
{
enum ParamIndex
{
#define AURA_ENUM_ANY(id, ...) id,
    AURA_PARAMS(AURA_ENUM_ANY, AURA_ENUM_ANY, AURA_ENUM_ANY, AURA_ENUM_ANY)
#undef AURA_ENUM_ANY
    kNumParams
};

enum class ParamType { Float, Choice, Bool, Int };

struct ParamDef
{
    const char* id;
    const char* name;
    ParamType type;
    float minV, maxV, defV, skew;
    const char* choices; // '|' separated, choice params only
};

const ParamDef& paramDef(int index);
int indexOfParam(const juce::String& id);              // -1 when unknown
float paramDefault(int index);
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Named constants for readability in the engine and the preset library
enum Wave { Saw = 0, Square, Triangle, Sine, Pulse, Wavetable };
enum SubWave { SubSine = 0, SubTri, SubSquare };
enum FilterType { LP12 = 0, LP24, HP12, BP12, Notch, Formant };
enum LfoShape { LfoSine = 0, LfoTri, LfoSaw, LfoSquare, LfoSH };
enum VoiceMode { Poly = 0, Mono, Legato };
enum ModSrc { SrcNone = 0, SrcLfo1, SrcLfo2, SrcLfo3, SrcFiltEnv, SrcAmpEnv, SrcVel, SrcWheel, SrcKey, kNumModSrc };
enum ModDst { DstNone = 0, DstPitch, DstCutoff, DstRes, DstWT1, DstWT2, DstPW1, DstPW2, DstLevel1, DstLevel2,
              DstFM, DstAmp, DstDetune, DstVowel, DstPitch1, DstPitch2, kNumModDst };
} // namespace aura
