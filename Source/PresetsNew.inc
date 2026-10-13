// ======================================================================================================
//  AuraSynth - "front page" library (v2): the 10 MELODIC TRAP core sounds, plucks, pads and the
//  genre sets (Hip-Hop, R&B, Jazz, Ambient, Pop).  Included from Presets.cpp (inside namespace aura::{anon}).
//
//  Sound-design notes used throughout (all values are plain parameters, nothing hidden):
//    * Plucks:  short exponential amp decay (ampD = time to silence), filter envelope opens ~3-4 octaves and closes
//               fast (fltD 0.1-0.3 s), key-tracking keeps high notes bright, velocity opens the filter a little.
//    * Leads:   legato + a few ms of glide gives the "rubbery" connected feel, vibrato <= 12 cents,
//               a presence bump in the EQ gives the nasal character, movement adds slow life.
//    * Pads:    slow attack / release, low movement-rate filter drift, high-pass in the FX chain (clean lows),
//               side content is high-passed by the master stage, reverb with a long tail but dark damping.
//    * Keys:    FM / triangle / sine bodies with a filter that closes with the note, light chorus, warm saturation.
// ======================================================================================================
void buildPart4(std::vector<Preset>& P)
{
    const unsigned HH = GenreHipHop, TR = GenreTrap, RB = GenreRnB, JZ = GenreJazz, AM = GenreAmbient, PO = GenrePop;

    // ============================================================================================
    //  MELODIC TRAP - the 10 core presets
    // ============================================================================================
    addN(P, PLUCK, GroupPluck, TR | PO, "Plan A Pluck", {
        // bright catchy digital pluck: soft 4 ms attack, rounded saw body, glassy wavetable sheen, crisp top, short tail
        {"o1Wave", Saw}, {"o1Level", 0.62f}, {"o1Uni", 2}, {"o1Det", 8}, {"o1Spread", 0.3f},
        {"o2Wave", Wavetable}, {"o2WT", 0.67f}, {"o2Oct", 1}, {"o2Fine", 6}, {"o2Level", 0.30f}, {"o2Uni", 1},
        {"subLevel", 0.10f}, {"subWave", SubSine},
        {"fType", LP24}, {"fCutoff", 1500}, {"fRes", 0.18f}, {"fKey", 0.5f}, {"fEnv", 0.75f},
        {"fltA", 0.001f}, {"fltD", 0.16f}, {"fltS", 0.05f}, {"fltR", 0.15f},
        {"ampA", 0.004f}, {"ampD", 0.32f}, {"ampS", 0}, {"ampR", 0.18f},
        {"vVelSens", 0.7f}, {"m1Src", SrcVel}, {"m1Dst", DstCutoff}, {"m1Amt", 0.22f},
        {"chMix", 0.12f}, {"chRate", 0.6f}, {"chDepth", 0.3f},
        {"dlMix", 0.14f}, {"dlSync", 2}, {"dlFb", 0.25f}, {"dlTone", 5500},
        {"rvMix", 0.15f}, {"rvSize", 0.30f}, {"rvDamp", 0.5f}, {"stWidth", 1.1f} });

    addN(P, NASAL, GroupLead, TR | HH, "Dark Thoughts Lead", {
        // slightly nasal, rubbery futuristic lead: reedy wavetable + narrow pulse, resonant LP, legato glide, subtle movement
        {"o1Wave", Wavetable}, {"o1WT", 0.47f}, {"o1Level", 0.62f}, {"o1Uni", 2}, {"o1Det", 6}, {"o1Spread", 0.4f},
        {"o2Wave", Pulse}, {"o2PW", 0.30f}, {"o2Fine", -7}, {"o2Level", 0.33f},
        {"fType", LP24}, {"fCutoff", 2600}, {"fRes", 0.32f}, {"fKey", 0.5f}, {"fEnv", 0.25f},
        {"fltA", 0.03f}, {"fltD", 0.35f}, {"fltS", 0.55f}, {"fltR", 0.25f},
        {"ampA", 0.015f}, {"ampD", 0.2f}, {"ampS", 0.85f}, {"ampR", 0.22f},
        {"vMode", Legato}, {"vGlide", 0.07f}, {"vVelSens", 0.5f},
        {"l1Shape", LfoSine}, {"l1Rate", 5.4f}, {"m1Src", SrcLfo1}, {"m1Dst", DstPitch}, {"m1Amt", 0.008f},
        {"m2Src", SrcWheel}, {"m2Dst", DstPitch}, {"m2Amt", 0.02f},
        {"movement", 0.25f}, {"warmth", 0.15f},
        {"eqMid", 2.5f}, {"eqMidFreq", 1600}, {"eqHigh", -1.f},
        {"chMix", 0.10f}, {"dlMix", 0.18f}, {"dlSync", 3}, {"dlFb", 0.30f}, {"dlTone", 4500},
        {"rvMix", 0.20f}, {"rvSize", 0.45f}, {"stWidth", 1.1f} });

    addN(P, GLASSY, GroupPluck, TR | PO, "Glass Bounce", {
        // glassy bell-like pluck: sine FM with an inharmonic ratio whose index follows the filter envelope (the "bounce"),
        // soft top end (LP12 + FX low-pass) so it is bright but never piercing
        {"o1Wave", Sine}, {"o1Level", 0.7f}, {"fmAmt", 0.26f}, {"fmRatio", 3.5f},
        {"o2Wave", Wavetable}, {"o2WT", 0.67f}, {"o2Oct", 1}, {"o2Level", 0.16f},
        {"fType", LP12}, {"fCutoff", 6500}, {"fRes", 0.05f}, {"fKey", 0.3f}, {"fEnv", 0.2f},
        {"fltA", 0.001f}, {"fltD", 0.22f}, {"fltS", 0.1f},
        {"m1Src", SrcFiltEnv}, {"m1Dst", DstFM}, {"m1Amt", 0.30f},
        {"m2Src", SrcFiltEnv}, {"m2Dst", DstPitch}, {"m2Amt", 0.010f},
        {"ampA", 0.002f}, {"ampD", 0.9f}, {"ampS", 0}, {"ampR", 0.5f}, {"vVelSens", 0.7f},
        {"fxLP", 12000}, {"eqHigh", -1.5f}, {"stWidth", 1.15f},
        {"dlMix", 0.16f}, {"dlSync", 2}, {"dlFb", 0.30f}, {"dlTone", 4500},
        {"rvMix", 0.22f}, {"rvSize", 0.50f}, {"rvDamp", 0.6f} });

    addN(P, KAZOO, GroupKeys, TR | HH | PO, "Rubber Keys", {
        // soft, slightly hollow, almost trumpet-like: square body + triangle, the filter "wahs" open on every note
        {"o1Wave", Square}, {"o1Level", 0.65f}, {"o2Wave", Triangle}, {"o2Level", 0.32f}, {"o2Fine", 5},
        {"fType", LP24}, {"fCutoff", 700}, {"fRes", 0.20f}, {"fKey", 0.55f}, {"fEnv", 0.60f},
        {"fltA", 0.05f}, {"fltD", 0.25f}, {"fltS", 0.45f}, {"fltR", 0.2f},
        {"ampA", 0.02f}, {"ampD", 0.30f}, {"ampS", 0.65f}, {"ampR", 0.18f},
        {"vMode", Legato}, {"vGlide", 0.03f}, {"vVelSens", 0.5f},
        {"l1Shape", LfoSine}, {"l1Rate", 5.2f}, {"m1Src", SrcLfo1}, {"m1Dst", DstPitch}, {"m1Amt", 0.007f},
        {"saAmt", 0.10f}, {"chMix", 0.15f}, {"chRate", 0.35f},
        {"dlMix", 0.10f}, {"dlSync", 2}, {"rvMix", 0.18f}, {"rvSize", 0.40f} });

    addN(P, AIRY, GroupPad, TR | AM | RB, "Airy Trap Pad", {
        // wide, smooth background: detuned saws + a mellow wavetable one octave up, a breath of noise, slow filter bloom.
        // Presence dip at 1.5 kHz and high-passed lows leave room for the melody and the vocal.
        {"o1Wave", Saw}, {"o1Level", 0.45f}, {"o1Uni", 5}, {"o1Det", 18}, {"o1Spread", 0.9f},
        {"o2Wave", Wavetable}, {"o2WT", 0.13f}, {"o2Oct", 1}, {"o2Level", 0.25f}, {"o2Uni", 3}, {"o2Det", 22}, {"o2Spread", 0.9f},
        {"nLevel", 0.04f}, {"nTone", 0.9f},
        {"fType", LP24}, {"fCutoff", 2200}, {"fRes", 0.10f}, {"fKey", 0.4f}, {"fEnv", 0.10f},
        {"fltA", 1.2f}, {"fltD", 2.0f}, {"fltS", 0.6f}, {"fltR", 1.5f},
        {"ampA", 0.9f}, {"ampD", 1.0f}, {"ampS", 0.85f}, {"ampR", 2.2f}, {"vVelSens", 0.3f},
        {"movement", 0.40f}, {"vDrift", 0.30f}, {"warmth", 0.20f},
        {"eqLow", -2.f}, {"eqMid", -2.f}, {"eqMidFreq", 1500}, {"fxHP", 160},
        {"chMix", 0.35f}, {"chRate", 0.30f}, {"chDepth", 0.6f},
        {"dlMix", 0.12f}, {"dlSync", 5}, {"dlFb", 0.35f}, {"dlTone", 3500},
        {"rvMix", 0.38f}, {"rvSize", 0.80f}, {"rvDamp", 0.55f}, {"stWidth", 1.4f} });

    addN(P, DARK, GroupPad, TR | HH | AM | RB, "Dark Floating Pad", {
        // darker, emotional: saw + triangle one octave down + sub, very slow filter bloom and slow LFO, dark long reverb
        {"o1Wave", Saw}, {"o1Level", 0.5f}, {"o1Uni", 3}, {"o1Det", 12}, {"o1Spread", 0.8f},
        {"o2Wave", Triangle}, {"o2Oct", -1}, {"o2Level", 0.35f},
        {"subLevel", 0.12f},
        {"fType", LP24}, {"fCutoff", 700}, {"fRes", 0.15f}, {"fKey", 0.4f}, {"fEnv", 0.35f},
        {"fltA", 2.2f}, {"fltD", 3.0f}, {"fltS", 0.55f}, {"fltR", 2.0f},
        {"ampA", 1.4f}, {"ampD", 1.5f}, {"ampS", 0.85f}, {"ampR", 3.0f}, {"vVelSens", 0.3f},
        {"l2Shape", LfoSine}, {"l2Rate", 0.12f}, {"m1Src", SrcLfo2}, {"m1Dst", DstCutoff}, {"m1Amt", 0.22f},
        {"movement", 0.35f}, {"vDrift", 0.25f}, {"warmth", 0.30f}, {"saAmt", 0.2f},
        {"fxHP", 120}, {"fxLP", 9000},
        {"chMix", 0.30f}, {"chRate", 0.25f},
        {"dlMix", 0.15f}, {"dlSync", 1}, {"dlFb", 0.35f}, {"dlTone", 2500},
        {"rvMix", 0.45f}, {"rvSize", 0.88f}, {"rvDamp", 0.75f}, {"stWidth", 1.25f} });

    addN(P, BELLS, GroupBell, TR | AM | PO, "Soft Digital Bells", {
        // delicate, slightly metallic: sine FM (ratio 2.76) whose index decays with the note, soft top, controlled decay
        {"o1Wave", Sine}, {"o1Oct", 1}, {"o1Level", 0.7f}, {"fmAmt", 0.22f}, {"fmRatio", 2.76f},
        {"o2Wave", Triangle}, {"o2Oct", 2}, {"o2Level", 0.12f},
        {"m1Src", SrcAmpEnv}, {"m1Dst", DstFM}, {"m1Amt", 0.30f},
        {"fType", LP12}, {"fCutoff", 9000}, {"fEnv", 0}, {"fKey", 0.2f},
        {"ampA", 0.002f}, {"ampD", 1.4f}, {"ampS", 0}, {"ampR", 0.8f}, {"vVelSens", 0.7f},
        {"fxLP", 10000}, {"chMix", 0.12f}, {"stWidth", 1.2f},
        {"dlMix", 0.15f}, {"dlSync", 3}, {"dlFb", 0.30f}, {"dlTone", 5000},
        {"rvMix", 0.30f}, {"rvSize", 0.60f}, {"rvDamp", 0.65f} });

    addN(P, DREAMY, GroupLead, TR | RB | AM, "Dreamy Lead", {
        // smooth, slightly detuned, futuristic: saw unison + glassy wavetable, legato, ping-pong delay for the stereo echo
        {"o1Wave", Saw}, {"o1Level", 0.5f}, {"o1Uni", 3}, {"o1Det", 14}, {"o1Spread", 0.6f},
        {"o2Wave", Wavetable}, {"o2WT", 0.67f}, {"o2Level", 0.28f}, {"o2Fine", 6},
        {"fType", LP24}, {"fCutoff", 3200}, {"fRes", 0.12f}, {"fKey", 0.5f}, {"fEnv", 0.15f},
        {"fltA", 0.05f}, {"fltD", 0.5f}, {"fltS", 0.6f},
        {"ampA", 0.03f}, {"ampD", 0.3f}, {"ampS", 0.8f}, {"ampR", 0.45f},
        {"vMode", Legato}, {"vGlide", 0.04f}, {"vVelSens", 0.5f},
        {"l1Shape", LfoSine}, {"l1Rate", 5.0f}, {"m1Src", SrcLfo1}, {"m1Dst", DstPitch}, {"m1Amt", 0.007f},
        {"movement", 0.25f}, {"warmth", 0.10f},
        {"chMix", 0.25f}, {"chRate", 0.5f},
        {"dlMix", 0.28f}, {"dlSync", 3}, {"dlFb", 0.40f}, {"dlTone", 4500}, {"dlPing", 1},
        {"rvMix", 0.30f}, {"rvSize", 0.55f}, {"stWidth", 1.2f} });

    addN(P, KEYS, GroupKeys, TR | HH | RB, "Warm Background Keys", {
        // mellow, rounded keyboard that supports the melody: triangle + sine, filter closes with the note, EP-style chorus
        {"o1Wave", Triangle}, {"o1Level", 0.7f}, {"o2Wave", Sine}, {"o2Oct", 1}, {"o2Level", 0.20f}, {"subLevel", 0.06f},
        {"fType", LP12}, {"fCutoff", 1800}, {"fRes", 0.05f}, {"fKey", 0.4f}, {"fEnv", 0.30f},
        {"fltA", 0.002f}, {"fltD", 0.7f}, {"fltS", 0.3f}, {"fltR", 0.4f},
        {"ampA", 0.004f}, {"ampD", 1.4f}, {"ampS", 0.30f}, {"ampR", 0.6f},
        {"vVelSens", 0.75f}, {"m1Src", SrcVel}, {"m1Dst", DstCutoff}, {"m1Amt", 0.30f},
        {"warmth", 0.35f}, {"saAmt", 0.15f}, {"fxLP", 9000},
        {"chMix", 0.35f}, {"chRate", 0.35f}, {"chDepth", 0.5f},
        {"dlMix", 0.10f}, {"dlSync", 3}, {"rvMix", 0.25f}, {"rvSize", 0.55f} });

    addN(P, LONG, GroupPluck, TR | PO, "Full Melodic Stack", {
        // ONE preset, two layers: a focused saw pluck on the main path + a wide, slow pad on "Layer B" (osc 2, own envelope + filter)
        {"o1Wave", Saw}, {"o1Level", 0.6f}, {"o1Uni", 2}, {"o1Det", 8}, {"o1Spread", 0.3f}, {"subLevel", 0.08f},
        {"fType", LP24}, {"fCutoff", 1400}, {"fRes", 0.15f}, {"fKey", 0.5f}, {"fEnv", 0.70f},
        {"fltA", 0.001f}, {"fltD", 0.15f}, {"fltS", 0.05f},
        {"ampA", 0.003f}, {"ampD", 0.28f}, {"ampS", 0}, {"ampR", 0.15f},
        {"o2Wave", Saw}, {"o2Level", 0.45f}, {"o2Uni", 4}, {"o2Det", 20}, {"o2Spread", 0.9f},
        {"layOn", 1}, {"layCut", 1500}, {"layA", 0.45f}, {"layD", 1.0f}, {"layS", 0.8f}, {"layR", 1.6f},
        {"vVelSens", 0.65f}, {"m1Src", SrcVel}, {"m1Dst", DstCutoff}, {"m1Amt", 0.2f},
        {"movement", 0.30f}, {"vDrift", 0.15f}, {"fxHP", 140}, {"eqMid", -1.5f}, {"eqMidFreq", 2200},
        {"chMix", 0.20f}, {"chRate", 0.4f},
        {"dlMix", 0.15f}, {"dlSync", 2}, {"dlFb", 0.30f}, {"dlTone", 4500},
        {"rvMix", 0.28f}, {"rvSize", 0.60f}, {"rvDamp", 0.6f}, {"stWidth", 1.2f} });

    // ============================================================================================
    //  PLUCKS - ten different characters
    // ============================================================================================
    addN(P, PLUCK, GroupPluck, TR | RB | PO, "Soft Round Pluck", {
        {"o1Wave", Triangle}, {"o1Level", 0.7f}, {"o2Wave", Sine}, {"o2Oct", 1}, {"o2Level", 0.22f},
        {"fType", LP12}, {"fCutoff", 2200}, {"fKey", 0.45f}, {"fEnv", 0.40f}, {"fltA", 0.002f}, {"fltD", 0.20f}, {"fltS", 0.1f},
        {"ampA", 0.008f}, {"ampD", 0.50f}, {"ampS", 0}, {"ampR", 0.30f}, {"vVelSens", 0.7f},
        {"warmth", 0.15f}, {"chMix", 0.15f}, {"chRate", 0.4f}, {"rvMix", 0.18f}, {"rvSize", 0.40f}, {"dlMix", 0.08f}, {"dlSync", 3} });

    addN(P, NASAL, GroupPluck, TR | HH, "Nasal Pluck", {
        {"o1Wave", Wavetable}, {"o1WT", 0.47f}, {"o1Level", 0.7f}, {"o2Wave", Pulse}, {"o2PW", 0.25f}, {"o2Level", 0.28f},
        {"fType", LP12}, {"fCutoff", 1800}, {"fRes", 0.45f}, {"fKey", 0.5f}, {"fEnv", 0.55f}, {"fltD", 0.14f}, {"fltS", 0.05f},
        {"ampA", 0.003f}, {"ampD", 0.28f}, {"ampS", 0}, {"ampR", 0.15f}, {"vVelSens", 0.65f},
        {"eqMid", 3.f}, {"eqMidFreq", 1800}, {"chMix", 0.10f}, {"rvMix", 0.14f}, {"rvSize", 0.30f}, {"dlMix", 0.12f}, {"dlSync", 2} });

    addN(P, KAZOO, GroupPluck, TR | HH, "Hollow Boing", {
        // hollow + rubbery: square through a resonant LP24 with a short pitch "boing" (filter env -> pitch)
        {"o1Wave", Square}, {"o1Level", 0.85f}, {"o1PW", 0.5f},
        {"fType", LP24}, {"fCutoff", 700}, {"fRes", 0.40f}, {"fKey", 0.5f}, {"fEnv", 0.80f}, {"fltA", 0.001f}, {"fltD", 0.12f}, {"fltS", 0},
        {"m1Src", SrcFiltEnv}, {"m1Dst", DstPitch}, {"m1Amt", 0.030f},
        {"ampA", 0.002f}, {"ampD", 0.24f}, {"ampS", 0}, {"ampR", 0.12f},
        {"vMode", Mono}, {"vGlide", 0.03f}, {"saAmt", 0.2f}, {"rvMix", 0.12f}, {"rvSize", 0.30f}, {"dlMix", 0.10f}, {"dlSync", 2} });

    addN(P, PLUCK, GroupPluck, TR | PO, "Steel Pluck", {
        // metallic but controlled: FM (ratio 4) with a low-passed, high-passed top so it stays clean in a mix
        {"o1Wave", Sine}, {"o1Level", 0.7f}, {"fmAmt", 0.32f}, {"fmRatio", 4.0f}, {"o2Wave", Triangle}, {"o2Oct", 1}, {"o2Level", 0.18f},
        {"m1Src", SrcFiltEnv}, {"m1Dst", DstFM}, {"m1Amt", 0.2f},
        {"fType", LP12}, {"fCutoff", 5500}, {"fKey", 0.3f}, {"fEnv", 0.25f}, {"fltD", 0.15f}, {"fltS", 0},
        {"ampA", 0.002f}, {"ampD", 0.30f}, {"ampS", 0}, {"ampR", 0.20f}, {"vVelSens", 0.7f},
        {"fxHP", 200}, {"fxLP", 9000}, {"eqHigh", -2.f}, {"chMix", 0.12f}, {"rvMix", 0.14f}, {"rvSize", 0.35f}, {"dlMix", 0.10f}, {"dlSync", 4} });

    addN(P, PLUCK, GroupPluck, PO | TR | HH, "Dry Tick", {
        // short, dry and mono: no reverb, no delay - a rhythmic melody tool
        {"o1Wave", Pulse}, {"o1PW", 0.30f}, {"o1Level", 0.75f}, {"o2Wave", Saw}, {"o2Level", 0.20f}, {"o2Oct", 1},
        {"fType", LP24}, {"fCutoff", 2500}, {"fRes", 0.15f}, {"fKey", 0.5f}, {"fEnv", 0.60f}, {"fltD", 0.07f}, {"fltS", 0},
        {"ampA", 0.001f}, {"ampD", 0.13f}, {"ampS", 0}, {"ampR", 0.05f}, {"o1Spread", 0}, {"vVelSens", 0.8f} });

    addN(P, DREAMY, GroupPluck, TR | RB | AM, "Wide Dream Pluck", {
        {"o1Wave", Saw}, {"o1Level", 0.5f}, {"o1Uni", 5}, {"o1Det", 22}, {"o1Spread", 0.9f}, {"o2Wave", Triangle}, {"o2Level", 0.3f}, {"o2Oct", 1},
        {"fType", LP24}, {"fCutoff", 2800}, {"fKey", 0.4f}, {"fEnv", 0.50f}, {"fltD", 0.30f}, {"fltS", 0.1f},
        {"ampA", 0.005f}, {"ampD", 0.60f}, {"ampS", 0}, {"ampR", 0.50f}, {"movement", 0.20f},
        {"chMix", 0.35f}, {"chRate", 0.35f}, {"dlMix", 0.22f}, {"dlSync", 3}, {"dlFb", 0.4f}, {"rvMix", 0.35f}, {"rvSize", 0.60f}, {"stWidth", 1.4f} });

    addN(P, PLUCK, GroupPluck, TR | PO | RB, "Echo Pluck", {
        // plucks with a clear stereo ping-pong echo (dotted 1/8) - the echo carries the rhythm
        {"o1Wave", Saw}, {"o1Level", 0.55f}, {"o1Uni", 2}, {"o1Det", 7}, {"o2Wave", Triangle}, {"o2Level", 0.35f},
        {"fType", LP24}, {"fCutoff", 1700}, {"fKey", 0.5f}, {"fEnv", 0.65f}, {"fltD", 0.14f}, {"fltS", 0.05f},
        {"ampA", 0.003f}, {"ampD", 0.30f}, {"ampS", 0}, {"ampR", 0.15f},
        {"dlMix", 0.32f}, {"dlSync", 3}, {"dlFb", 0.45f}, {"dlTone", 5000}, {"dlPing", 1}, {"rvMix", 0.12f}, {"rvSize", 0.30f}, {"stWidth", 1.2f} });

    addN(P, BASS, GroupPluck, HH | TR, "Low Body Pluck", {
        // extra weight for melodies in the lower register
        {"o1Wave", Saw}, {"o1Level", 0.6f}, {"o2Wave", Square}, {"o2Oct", -1}, {"o2Level", 0.30f}, {"subLevel", 0.25f},
        {"fType", LP24}, {"fCutoff", 900}, {"fRes", 0.15f}, {"fKey", 0.6f}, {"fEnv", 0.60f}, {"fltD", 0.20f}, {"fltS", 0.05f},
        {"ampA", 0.003f}, {"ampD", 0.35f}, {"ampS", 0}, {"ampR", 0.18f}, {"saAmt", 0.2f}, {"fxHP", 45},
        {"rvMix", 0.12f}, {"rvSize", 0.30f}, {"dlMix", 0.08f}, {"dlSync", 2} });

    // ============================================================================================
    //  PADS - background layers
    // ============================================================================================
    addN(P, ANALOG, GroupPad, HH | RB | JZ, "Soft Analog Pad", {
        {"o1Wave", Saw}, {"o1Level", 0.5f}, {"o1Uni", 2}, {"o1Det", 10}, {"o2Wave", Saw}, {"o2Fine", 9}, {"o2Level", 0.4f},
        {"fType", LP12}, {"fCutoff", 1400}, {"fKey", 0.4f}, {"fEnv", 0.12f}, {"fltA", 1.0f}, {"fltD", 2.0f}, {"fltS", 0.7f},
        {"ampA", 1.1f}, {"ampD", 1.0f}, {"ampS", 0.85f}, {"ampR", 2.0f}, {"vDrift", 0.50f}, {"warmth", 0.5f}, {"movement", 0.2f},
        {"fxHP", 130}, {"chMix", 0.40f}, {"chRate", 0.3f}, {"rvMix", 0.35f}, {"rvSize", 0.7f}, {"rvDamp", 0.6f}, {"stWidth", 1.2f} });

    addN(P, GLASSY, GroupPad, TR | AM | PO, "Glass Digital Pad", {
        {"o1Wave", Wavetable}, {"o1WT", 0.67f}, {"o1Level", 0.5f}, {"o1Uni", 3}, {"o1Det", 14}, {"o1Spread", 0.8f},
        {"o2Wave", Sine}, {"o2Oct", 1}, {"o2Level", 0.2f},
        {"fType", LP12}, {"fCutoff", 5000}, {"fKey", 0.4f}, {"fEnv", 0.10f}, {"fltA", 0.8f}, {"fltD", 1.5f}, {"fltS", 0.7f},
        {"ampA", 0.8f}, {"ampD", 1.0f}, {"ampS", 0.85f}, {"ampR", 1.8f}, {"movement", 0.50f}, {"fxHP", 150}, {"eqHigh", 1.f},
        {"chMix", 0.25f}, {"dlMix", 0.15f}, {"dlSync", 5}, {"dlFb", 0.35f}, {"rvMix", 0.40f}, {"rvSize", 0.75f}, {"stWidth", 1.3f} });

    addN(P, AMBIENT, GroupPad, AM | TR, "Moving Atmos Pad", {
        // the wavetable position is scanned by a slow LFO, the filter drifts per voice
        {"o1Wave", Wavetable}, {"o1WT", 0.30f}, {"o1Level", 0.5f}, {"o1Uni", 3}, {"o1Det", 12}, {"o2Wave", Triangle}, {"o2Level", 0.3f}, {"o2Oct", -1},
        {"l2Shape", LfoSine}, {"l2Rate", 0.15f}, {"m1Src", SrcLfo2}, {"m1Dst", DstWT1}, {"m1Amt", 0.5f},
        {"fType", LP24}, {"fCutoff", 1800}, {"fKey", 0.4f}, {"fEnv", 0.1f}, {"fltA", 1.2f}, {"fltD", 2.0f}, {"fltS", 0.7f},
        {"ampA", 1.2f}, {"ampD", 1.0f}, {"ampS", 0.85f}, {"ampR", 2.5f}, {"movement", 0.60f}, {"vDrift", 0.2f}, {"fxHP", 140},
        {"chMix", 0.30f}, {"phMix", 0.15f}, {"phRate", 0.10f}, {"phDepth", 0.5f}, {"rvMix", 0.40f}, {"rvSize", 0.80f}, {"stWidth", 1.3f} });

    addN(P, AIRY, GroupPad, RB | AM | PO, "Silk Ensemble", {
        {"o1Wave", Saw}, {"o1Level", 0.5f}, {"o1Uni", 6}, {"o1Det", 12}, {"o1Spread", 0.9f}, {"o2Wave", Triangle}, {"o2Level", 0.25f},
        {"fType", LP12}, {"fCutoff", 2000}, {"fKey", 0.4f}, {"fEnv", 0.08f}, {"fltA", 0.7f}, {"fltD", 1.5f}, {"fltS", 0.8f},
        {"ampA", 0.7f}, {"ampD", 1.0f}, {"ampS", 0.88f}, {"ampR", 1.6f}, {"warmth", 0.25f}, {"movement", 0.2f}, {"fxHP", 140},
        {"chMix", 0.50f}, {"chRate", 0.35f}, {"chDepth", 0.6f}, {"rvMix", 0.30f}, {"rvSize", 0.65f}, {"rvDamp", 0.55f}, {"stWidth", 1.3f} });

    addN(P, LONG, GroupPad, TR | HH, "Pluck Partner Pad", {
        // sits underneath bright plucks: dark, narrow in the highs, a scoop at 2.5 kHz that leaves the pluck the space
        {"o1Wave", Triangle}, {"o1Level", 0.6f}, {"o2Wave", Square}, {"o2Oct", -1}, {"o2Level", 0.2f}, {"o1Uni", 2}, {"o1Det", 10},
        {"fType", LP12}, {"fCutoff", 1000}, {"fKey", 0.3f}, {"fEnv", 0.1f}, {"fltA", 0.6f}, {"fltD", 1.0f}, {"fltS", 0.8f},
        {"ampA", 0.5f}, {"ampD", 1.0f}, {"ampS", 0.85f}, {"ampR", 1.4f}, {"fxHP", 180}, {"eqMid", -3.f}, {"eqMidFreq", 2500},
        {"chMix", 0.25f}, {"movement", 0.25f}, {"rvMix", 0.25f}, {"rvSize", 0.6f}, {"rvDamp", 0.7f}, {"stWidth", 1.2f} });

    // ============================================================================================
    //  R&B
    // ============================================================================================
    addN(P, KEYS, GroupKeys, RB | JZ | HH, "Silk E-Piano", {
        // FM electric piano: FM index follows the amp envelope (bright strike, mellow body), velocity adds "bark"
        {"o1Wave", Sine}, {"o1Level", 0.7f}, {"fmAmt", 0.18f}, {"fmRatio", 1.0f}, {"o2Wave", Sine}, {"o2Oct", 2}, {"o2Level", 0.07f},
        {"m1Src", SrcAmpEnv}, {"m1Dst", DstFM}, {"m1Amt", 0.25f}, {"m2Src", SrcVel}, {"m2Dst", DstFM}, {"m2Amt", 0.22f},
        {"fType", LP12}, {"fCutoff", 6000}, {"fEnv", 0},
        {"ampA", 0.003f}, {"ampD", 1.8f}, {"ampS", 0.15f}, {"ampR", 0.5f}, {"vVelSens", 0.75f},
        {"warmth", 0.25f}, {"chMix", 0.45f}, {"chRate", 0.45f}, {"chDepth", 0.5f}, {"rvMix", 0.25f}, {"rvSize", 0.55f} });

    addN(P, KEYS, GroupKeys, RB | JZ, "Smooth Rhodes", {
        {"o1Wave", Triangle}, {"o1Level", 0.6f}, {"fmAmt", 0.10f}, {"fmRatio", 1.0f}, {"o2Wave", Sine}, {"o2Level", 0.35f},
        {"m1Src", SrcAmpEnv}, {"m1Dst", DstFM}, {"m1Amt", 0.15f},
        {"fType", LP12}, {"fCutoff", 4000}, {"fEnv", 0.1f}, {"fltD", 0.8f}, {"fltS", 0.5f},
        {"ampA", 0.004f}, {"ampD", 2.2f}, {"ampS", 0.20f}, {"ampR", 0.6f}, {"vVelSens", 0.7f},
        {"warmth", 0.4f}, {"saAmt", 0.1f}, {"chMix", 0.40f}, {"chRate", 0.30f}, {"rvMix", 0.22f}, {"rvSize", 0.50f}, {"dlMix", 0.08f}, {"dlSync", 3} });

    addN(P, DREAMY, GroupLead, RB | AM, "Soft Cry Lead", {
        // emotional, spacious lead: triangle + soft saw, slow vibrato, long dotted ping-pong echo and a big, soft room
        {"o1Wave", Triangle}, {"o1Level", 0.6f}, {"o2Wave", Saw}, {"o2Level", 0.25f}, {"o2Fine", 5},
        {"fType", LP12}, {"fCutoff", 2800}, {"fRes", 0.1f}, {"fKey", 0.5f}, {"fEnv", 0.2f}, {"fltA", 0.08f}, {"fltD", 0.5f}, {"fltS", 0.6f},
        {"ampA", 0.08f}, {"ampD", 0.3f}, {"ampS", 0.85f}, {"ampR", 0.5f}, {"vMode", Legato}, {"vGlide", 0.05f},
        {"l1Shape", LfoSine}, {"l1Rate", 4.8f}, {"m1Src", SrcLfo1}, {"m1Dst", DstPitch}, {"m1Amt", 0.009f}, {"m2Src", SrcWheel}, {"m2Dst", DstPitch}, {"m2Amt", 0.02f},
        {"warmth", 0.2f}, {"chMix", 0.20f}, {"dlMix", 0.30f}, {"dlSync", 5}, {"dlFb", 0.40f}, {"dlTone", 4000}, {"rvMix", 0.45f}, {"rvSize", 0.75f}, {"stWidth", 1.2f} });

    addN(P, KEYS, GroupKeys, RB | HH, "Velvet R&B Keys", {
        {"o1Wave", Triangle}, {"o1Level", 0.55f}, {"o2Wave", Saw}, {"o2Level", 0.2f}, {"o2Fine", 4}, {"subLevel", 0.06f},
        {"fType", LP12}, {"fCutoff", 1500}, {"fKey", 0.4f}, {"fEnv", 0.3f}, {"fltD", 0.7f}, {"fltS", 0.25f},
        {"ampA", 0.005f}, {"ampD", 1.2f}, {"ampS", 0.40f}, {"ampR", 0.55f}, {"vVelSens", 0.7f}, {"warmth", 0.4f},
        {"chMix", 0.40f}, {"chRate", 0.35f}, {"rvMix", 0.30f}, {"rvSize", 0.60f}, {"dlMix", 0.10f}, {"dlSync", 3} });

    // ============================================================================================
    //  JAZZ
    // ============================================================================================
    addN(P, KEYS, GroupKeys, JZ | RB, "Mellow Jazz EP", {
        // Wurlitzer-ish: gentle FM, slow amplitude tremolo, saturation for warmth, natural short room
        {"o1Wave", Sine}, {"o1Level", 0.65f}, {"fmAmt", 0.12f}, {"fmRatio", 1.0f}, {"o2Wave", Triangle}, {"o2Level", 0.30f},
        {"m1Src", SrcAmpEnv}, {"m1Dst", DstFM}, {"m1Amt", 0.18f},
        {"l1Shape", LfoSine}, {"l1Rate", 4.5f}, {"m2Src", SrcLfo1}, {"m2Dst", DstAmp}, {"m2Amt", 0.12f},
        {"fType", LP12}, {"fCutoff", 3000}, {"fEnv", 0},
        {"ampA", 0.004f}, {"ampD", 1.6f}, {"ampS", 0.25f}, {"ampR", 0.5f}, {"vVelSens", 0.7f},
        {"warmth", 0.5f}, {"saAmt", 0.15f}, {"chMix", 0.20f}, {"chRate", 0.3f}, {"rvMix", 0.18f}, {"rvSize", 0.35f}, {"rvDamp", 0.5f} });

    addN(P, KEYS, GroupKeys, JZ | HH | RB, "Soft Round Keys", {
        {"o1Wave", Sine}, {"o1Level", 0.6f}, {"o2Wave", Triangle}, {"o2Level", 0.4f}, {"o2Fine", 4},
        {"fType", LP12}, {"fCutoff", 1300}, {"fEnv", 0.3f}, {"fltD", 0.5f}, {"fltS", 0.2f}, {"fKey", 0.4f},
        {"ampA", 0.006f}, {"ampD", 1.0f}, {"ampS", 0.20f}, {"ampR", 0.45f}, {"vVelSens", 0.8f}, {"warmth", 0.35f},
        {"rvMix", 0.15f}, {"rvSize", 0.35f}, {"chMix", 0.12f} });

    addN(P, ANALOG, GroupKeys, JZ | HH, "Analog Smooth", {
        {"o1Wave", Saw}, {"o1Level", 0.55f}, {"o2Wave", Square}, {"o2Level", 0.2f}, {"o2Fine", -4},
        {"fType", LP24}, {"fCutoff", 900}, {"fRes", 0.12f}, {"fKey", 0.5f}, {"fEnv", 0.45f}, {"fltA", 0.01f}, {"fltD", 0.6f}, {"fltS", 0.25f},
        {"ampA", 0.01f}, {"ampD", 0.8f}, {"ampS", 0.45f}, {"ampR", 0.4f}, {"vDrift", 0.5f}, {"warmth", 0.5f},
        {"chMix", 0.15f}, {"rvMix", 0.15f}, {"rvSize", 0.4f} });

    addN(P, BELLS, GroupBell, JZ | AM | RB, "Gentle Vibes", {
        {"o1Wave", Sine}, {"o1Level", 0.7f}, {"fmAmt", 0.14f}, {"fmRatio", 4.0f}, {"o2Wave", Sine}, {"o2Oct", 2}, {"o2Level", 0.08f},
        {"m1Src", SrcAmpEnv}, {"m1Dst", DstFM}, {"m1Amt", 0.18f}, {"l1Shape", LfoSine}, {"l1Rate", 4.0f}, {"m2Src", SrcLfo1}, {"m2Dst", DstAmp}, {"m2Amt", 0.25f},
        {"fType", LP12}, {"fCutoff", 6000}, {"fEnv", 0},
        {"ampA", 0.003f}, {"ampD", 2.0f}, {"ampS", 0}, {"ampR", 0.8f}, {"warmth", 0.2f}, {"rvMix", 0.25f}, {"rvSize", 0.5f}, {"chMix", 0.1f} });

    addN(P, ANALOG, GroupKeys, JZ | HH, "Soft Drawbar", {
        {"o1Wave", Sine}, {"o1Level", 0.55f}, {"o2Wave", Sine}, {"o2Oct", 1}, {"o2Level", 0.35f}, {"subLevel", 0.35f}, {"subWave", SubSine},
        {"fType", LP12}, {"fCutoff", 5000}, {"fEnv", 0},
        {"l1Shape", LfoSine}, {"l1Rate", 5.5f}, {"m1Src", SrcLfo1}, {"m1Dst", DstAmp}, {"m1Amt", 0.08f},
        {"ampA", 0.008f}, {"ampD", 0.2f}, {"ampS", 1.0f}, {"ampR", 0.12f}, {"warmth", 0.3f}, {"chMix", 0.25f}, {"chRate", 0.6f}, {"rvMix", 0.18f}, {"rvSize", 0.4f} });

    // ============================================================================================
    //  AMBIENT
    // ============================================================================================
    addN(P, AIRY, GroupPad, AM, "Endless Air", {
        {"o1Wave", Saw}, {"o1Level", 0.45f}, {"o1Uni", 6}, {"o1Det", 24}, {"o1Spread", 1.0f}, {"o2Wave", Triangle}, {"o2Oct", 1}, {"o2Level", 0.2f}, {"nLevel", 0.06f}, {"nTone", 0.85f},
        {"fType", LP24}, {"fCutoff", 1600}, {"fKey", 0.4f}, {"fEnv", 0.12f}, {"fltA", 2.0f}, {"fltD", 3.0f}, {"fltS", 0.65f},
        {"ampA", 2.2f}, {"ampD", 1.0f}, {"ampS", 0.9f}, {"ampR", 4.0f}, {"movement", 0.60f}, {"vDrift", 0.3f}, {"fxHP", 150},
        {"chMix", 0.30f}, {"chRate", 0.2f}, {"dlMix", 0.25f}, {"dlSync", 5}, {"dlFb", 0.45f}, {"dlTone", 3000},
        {"rvMix", 0.50f}, {"rvSize", 0.92f}, {"rvDamp", 0.6f}, {"stWidth", 1.5f} });

    addN(P, AMBIENT, GroupTexture, AM | TR, "Slow Glass Texture", {
        {"o1Wave", Wavetable}, {"o1WT", 0.67f}, {"o1Level", 0.5f}, {"o1Uni", 3}, {"o1Det", 16}, {"o2Wave", Sine}, {"o2Oct", 1}, {"o2Level", 0.2f},
        {"l2Shape", LfoSine}, {"l2Rate", 0.10f}, {"m1Src", SrcLfo2}, {"m1Dst", DstWT1}, {"m1Amt", 0.6f},
        {"fType", LP12}, {"fCutoff", 4500}, {"fEnv", 0}, {"ampA", 1.5f}, {"ampD", 1.0f}, {"ampS", 0.9f}, {"ampR", 3.0f},
        {"movement", 0.80f}, {"fxHP", 160}, {"dlMix", 0.30f}, {"dlSync", 1}, {"dlFb", 0.5f}, {"dlTone", 3500}, {"rvMix", 0.50f}, {"rvSize", 0.85f}, {"stWidth", 1.4f} });

    addN(P, DREAMY, GroupPad, AM | RB, "Dream Echo Pad", {
        {"o1Wave", Triangle}, {"o1Level", 0.5f}, {"o1Uni", 3}, {"o1Det", 14}, {"o2Wave", Saw}, {"o2Level", 0.25f}, {"o2Fine", 7},
        {"fType", LP12}, {"fCutoff", 1800}, {"fEnv", 0.1f}, {"fltA", 1.0f}, {"fltD", 2.0f}, {"fltS", 0.7f},
        {"ampA", 1.0f}, {"ampD", 1.0f}, {"ampS", 0.88f}, {"ampR", 2.5f}, {"movement", 0.4f}, {"fxHP", 140},
        {"chMix", 0.25f}, {"dlMix", 0.40f}, {"dlSync", 1}, {"dlFb", 0.55f}, {"dlTone", 3500}, {"dlPing", 1}, {"rvMix", 0.50f}, {"rvSize", 0.8f}, {"stWidth", 1.3f} });

    addN(P, AMBIENT, GroupTexture, AM, "Breathing Mist", {
        {"o1Wave", Sine}, {"o1Level", 0.4f}, {"o1Oct", 1}, {"nLevel", 0.18f}, {"nTone", 0.30f}, {"o2Wave", Triangle}, {"o2Level", 0.2f},
        {"fType", BP12}, {"fCutoff", 1400}, {"fRes", 0.35f}, {"fKey", 0.3f}, {"fEnv", 0.1f},
        {"l2Shape", LfoSine}, {"l2Rate", 0.12f}, {"m1Src", SrcLfo2}, {"m1Dst", DstCutoff}, {"m1Amt", 0.4f},
        {"ampA", 3.0f}, {"ampD", 1.0f}, {"ampS", 1.0f}, {"ampR", 5.0f}, {"movement", 0.7f}, {"fxHP", 200},
        {"chMix", 0.2f}, {"rvMix", 0.55f}, {"rvSize", 0.9f}, {"rvDamp", 0.5f}, {"stWidth", 1.5f} });

    // ============================================================================================
    //  POP
    // ============================================================================================
    addN(P, PLUCK, GroupPluck, PO | TR, "Bright Pop Pluck", {
        {"o1Wave", Saw}, {"o1Level", 0.55f}, {"o2Wave", Square}, {"o2Oct", 1}, {"o2Level", 0.25f}, {"o2Fine", 4},
        {"fType", LP24}, {"fCutoff", 4000}, {"fRes", 0.1f}, {"fKey", 0.5f}, {"fEnv", 0.60f}, {"fltD", 0.12f}, {"fltS", 0.1f},
        {"ampA", 0.002f}, {"ampD", 0.25f}, {"ampS", 0}, {"ampR", 0.15f}, {"vVelSens", 0.7f}, {"bright", 0.6f},
        {"rvMix", 0.10f}, {"rvSize", 0.3f}, {"dlMix", 0.06f}, {"dlSync", 2}, {"eqHigh", 1.5f} });

    addN(P, GLASSY, GroupLead, PO | TR, "Catchy Lead", {
        {"o1Wave", Saw}, {"o1Level", 0.5f}, {"o1Uni", 3}, {"o1Det", 10}, {"o1Spread", 0.5f}, {"o2Wave", Pulse}, {"o2PW", 0.4f}, {"o2Level", 0.3f},
        {"fType", LP24}, {"fCutoff", 4500}, {"fRes", 0.15f}, {"fKey", 0.5f}, {"fEnv", 0.2f}, {"fltD", 0.3f}, {"fltS", 0.6f},
        {"ampA", 0.01f}, {"ampD", 0.3f}, {"ampS", 0.85f}, {"ampR", 0.25f}, {"vMode", Legato}, {"vGlide", 0.02f},
        {"l1Rate", 5.5f}, {"m1Src", SrcLfo1}, {"m1Dst", DstPitch}, {"m1Amt", 0.007f}, {"dlMix", 0.15f}, {"dlSync", 2}, {"rvMix", 0.18f}, {"rvSize", 0.45f}, {"chMix", 0.15f} });

    addN(P, KEYS, GroupKeys, PO | RB, "Polished Keys", {
        {"o1Wave", Triangle}, {"o1Level", 0.5f}, {"o2Wave", Saw}, {"o2Level", 0.25f}, {"subLevel", 0.05f},
        {"fType", LP12}, {"fCutoff", 3500}, {"fKey", 0.4f}, {"fEnv", 0.3f}, {"fltD", 0.6f}, {"fltS", 0.3f},
        {"ampA", 0.004f}, {"ampD", 1.0f}, {"ampS", 0.25f}, {"ampR", 0.4f}, {"vVelSens", 0.7f}, {"chMix", 0.25f}, {"rvMix", 0.20f}, {"rvSize", 0.45f} });

    addN(P, BELLS, GroupBell, PO | TR, "Clear Bell Lead", {
        {"o1Wave", Sine}, {"o1Level", 0.7f}, {"fmAmt", 0.20f}, {"fmRatio", 2.0f}, {"m1Src", SrcAmpEnv}, {"m1Dst", DstFM}, {"m1Amt", 0.2f},
        {"fType", LP12}, {"fCutoff", 8000}, {"fEnv", 0}, {"ampA", 0.005f}, {"ampD", 0.9f}, {"ampS", 0.2f}, {"ampR", 0.35f},
        {"o2Wave", Triangle}, {"o2Oct", 1}, {"o2Level", 0.12f}, {"rvMix", 0.20f}, {"rvSize", 0.45f}, {"dlMix", 0.10f}, {"dlSync", 2} });

    // ============================================================================================
    //  HIP-HOP
    // ============================================================================================
    addN(P, KEYS, GroupKeys, HH | RB | JZ, "Warm Hip-Hop Keys", {
        {"o1Wave", Triangle}, {"o1Level", 0.6f}, {"o2Wave", Saw}, {"o2Fine", 6}, {"o2Level", 0.25f},
        {"fType", LP12}, {"fCutoff", 1500}, {"fKey", 0.4f}, {"fEnv", 0.3f}, {"fltD", 0.6f}, {"fltS", 0.25f},
        {"ampA", 0.005f}, {"ampD", 1.1f}, {"ampS", 0.30f}, {"ampR", 0.4f}, {"vVelSens", 0.7f}, {"warmth", 0.4f}, {"saAmt", 0.3f},
        {"chMix", 0.20f}, {"chRate", 0.3f}, {"vDrift", 0.4f}, {"rvMix", 0.20f}, {"rvSize", 0.45f} });

    addN(P, ANALOG, GroupLead, HH | TR, "Round Synth", {
        {"o1Wave", Square}, {"o1Level", 0.55f}, {"o2Wave", Triangle}, {"o2Level", 0.4f},
        {"fType", LP12}, {"fCutoff", 1300}, {"fKey", 0.5f}, {"fEnv", 0.3f}, {"fltD", 0.3f}, {"fltS", 0.5f},
        {"ampA", 0.01f}, {"ampD", 0.4f}, {"ampS", 0.6f}, {"ampR", 0.25f}, {"saAmt", 0.2f}, {"dlMix", 0.12f}, {"dlSync", 2}, {"rvMix", 0.15f}, {"rvSize", 0.4f} });

    addN(P, COUNTER, GroupLead, HH | TR, "Simple Hook Lead", {
        {"o1Wave", Pulse}, {"o1PW", 0.35f}, {"o1Level", 0.6f}, {"o2Wave", Saw}, {"o2Level", 0.25f},
        {"fType", LP12}, {"fCutoff", 2400}, {"fRes", 0.2f}, {"fKey", 0.5f}, {"fEnv", 0.25f}, {"fltD", 0.3f}, {"fltS", 0.5f},
        {"ampA", 0.008f}, {"ampD", 0.3f}, {"ampS", 0.8f}, {"ampR", 0.2f}, {"vMode", Legato}, {"vGlide", 0.03f},
        {"l1Rate", 5.2f}, {"m1Src", SrcLfo1}, {"m1Dst", DstPitch}, {"m1Amt", 0.006f}, {"dsMix", 0.10f}, {"dsDrive", 0.2f}, {"dlMix", 0.12f}, {"dlSync", 2}, {"rvMix", 0.15f} });

    addN(P, DARK, GroupPad, HH | TR, "Smoked Pad", {
        {"o1Wave", Saw}, {"o1Level", 0.5f}, {"o1Uni", 3}, {"o1Det", 10}, {"subLevel", 0.15f},
        {"fType", LP24}, {"fCutoff", 600}, {"fKey", 0.4f}, {"fEnv", 0.2f}, {"fltA", 1.2f}, {"fltD", 2.0f}, {"fltS", 0.6f},
        {"ampA", 1.2f}, {"ampD", 1.0f}, {"ampS", 0.85f}, {"ampR", 2.0f}, {"warmth", 0.4f}, {"saAmt", 0.3f}, {"vDrift", 0.6f}, {"fxHP", 100},
        {"chMix", 0.20f}, {"rvMix", 0.35f}, {"rvSize", 0.7f}, {"rvDamp", 0.8f} });

    addN(P, EXPERIMENT, GroupTexture, HH | TR, "Gritty Tape Texture", {
        {"o1Wave", Saw}, {"o1Level", 0.45f}, {"o1Uni", 2}, {"o1Det", 14}, {"nLevel", 0.08f}, {"nTone", 0.5f},
        {"fType", LP12}, {"fCutoff", 1800}, {"fEnv", 0.1f}, {"ampA", 0.5f}, {"ampD", 1.0f}, {"ampS", 0.9f}, {"ampR", 1.0f},
        {"bcBits", 10}, {"bcRate", 2}, {"bcMix", 0.20f}, {"saAmt", 0.4f}, {"dsMix", 0.12f}, {"dsDrive", 0.25f}, {"vDrift", 0.8f},
        {"chMix", 0.2f}, {"rvMix", 0.30f}, {"rvSize", 0.55f} });
}
