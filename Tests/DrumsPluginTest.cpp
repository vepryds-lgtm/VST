// AuraDrums - tests of the real plug-in object (state, MIDI, tone / space, block-size independence) + screenshots + demo render.
// Usage:  AuraDrumsPluginTest                      run all checks
//         AuraDrumsPluginTest --screenshot <dir>   editor pictures
//         AuraDrumsPluginTest --rendermidi <mid> <out.wav> [kitIndex | soundName] [bpm]   plays a MIDI file through the plug-in (kit number = Kit mode, a name = Single mode)
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
#include "DrumsProcessor.h"
#include "DrumsEditor.h"

using namespace drums;

namespace
{
int failures = 0, checks = 0;
void check(bool ok, const juce::String& what)
{
    ++checks;
    if (!ok) { ++failures; std::cout << "  FAIL: " << what << std::endl; }
}
void info(const juce::String& s) { std::cout << s << std::endl; }

struct Rig
{
    explicit Rig(double sampleRate = 44100.0, int block = 512) : sr(sampleRate), blockSize(block)
    {
        proc.setPlayConfigDetails(0, 2, sr, block);
        proc.prepareToPlay(sr, block);
    }
    void setParam(const char* id, float plain)
    {
        auto* p = proc.apvts.getParameter(id);
        p->setValueNotifyingHost(p->convertTo0to1(plain));
    }
    // events: (sample, message)
    juce::AudioBuffer<float> render(int totalSamples, std::vector<std::pair<int, juce::MidiMessage>> ev, int block = 0)
    {
        if (block <= 0) block = blockSize;
        std::sort(ev.begin(), ev.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        juce::AudioBuffer<float> out(2, totalSamples), blk(2, block);
        size_t k = 0;
        for (int pos = 0; pos < totalSamples; pos += block)
        {
            const int n = std::min(block, totalSamples - pos);
            juce::MidiBuffer mb;
            while (k < ev.size() && ev[k].first < pos + n) { mb.addEvent(ev[k].second, ev[k].first - pos); ++k; }
            juce::AudioBuffer<float> view(blk.getArrayOfWritePointers(), 2, n);
            proc.processBlock(view, mb);
            for (int c = 0; c < 2; ++c) out.copyFrom(c, pos, view, c, 0, n);
        }
        return out;
    }
    double sr; int blockSize;
    DrumsProcessor proc;
};

juce::MidiMessage on(int note, int vel = 100) { return juce::MidiMessage::noteOn(1, note, (juce::uint8) vel); }
juce::MidiMessage off(int note) { return juce::MidiMessage::noteOff(1, note); }
float peakOf(const juce::AudioBuffer<float>& b, int from = 0, int to = -1)
{
    if (to < 0) to = b.getNumSamples();
    float p = 0.f;
    for (int c = 0; c < b.getNumChannels(); ++c) for (int i = from; i < to; ++i) p = std::max(p, std::abs(b.getSample(c, i)));
    return p;
}
double rmsOf(const juce::AudioBuffer<float>& b, int from, int to)
{
    double s = 0; for (int c = 0; c < b.getNumChannels(); ++c) for (int i = from; i < to; ++i) s += (double) b.getSample(c, i) * b.getSample(c, i);
    return std::sqrt(s / std::max(1, (to - from) * b.getNumChannels()));
}
bool allFinite(const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < b.getNumSamples(); ++i) if (!std::isfinite(b.getSample(c, i))) return false;
    return true;
}

void testBasics()
{
    info("[plug-in basics]");
    Rig r;
    check(r.proc.getName() == "AuraDrums", "name");
    check(r.proc.acceptsMidi() && !r.proc.producesMidi(), "midi flags");
    check(r.proc.getNumPrograms() == numSounds(), "programs = sounds");
    check(r.proc.getTotalNumOutputChannels() == 2 && r.proc.getTotalNumInputChannels() == 0, "bus layout");
    for (const char* id : { pid::sound, pid::mode, pid::kit, pid::keyMode, pid::volume, pid::tune, pid::length, pid::tone, pid::space })
        check(r.proc.apvts.getParameter(id) != nullptr, juce::String("parameter ") + id);
    check(r.proc.selectedSound() == indexOfSound("Plan A Kick") && !r.proc.isKitMode(), "default = Plan A Kick, single mode");

    // silence in, silence out; a note makes sound; no NaN
    auto silent = r.render(4096, {});
    check(peakOf(silent) == 0.f, "no notes = silence");
    auto kick = r.render(44100, { { 0, on(36) } });
    check(peakOf(kick) > 0.2f && peakOf(kick) < 1.0f && allFinite(kick), "kick plays at a sensible level: " + juce::String(peakOf(kick)));
    // sample accurate start
    auto late = r.render(2048, { { 700, on(36) } });
    int first = -1; for (int i = 0; i < 2048 && first < 0; ++i) if (std::abs(late.getSample(0, i)) > 1e-7f) first = i;
    check(first >= 700 && first <= 704, "note starts sample-accurately (first non-zero sample " + juce::String(first) + ")");

    // programs step through the library
    for (int i : { 0, 9, 31, 40, numSounds() - 1 })
    {
        r.proc.setCurrentProgram(i);
        check(r.proc.getCurrentProgram() == i && r.proc.getProgramName(i) == juce::String(soundInfo(i).name), "program " + juce::String(i));
    }
}

void testVolumeToneSpace()
{
    info("[volume / tone / space]");
    Rig r;
    r.proc.selectSound(indexOfSound("Plan A Clap"));
    auto base = r.render(44100, { { 0, on(60) } });
    r.setParam(pid::volume, 0.4f);
    r.render(2048, {});                              // let the volume ramp settle (it glides over one block)
    auto half = r.render(44100, { { 0, on(60) } });
    check(std::abs(peakOf(half) / peakOf(base) - 0.5f) < 0.02f, "volume 0.4 is half of 0.8: " + juce::String(peakOf(half) / peakOf(base)));
    r.setParam(pid::volume, 0.f);
    r.render(2048, {});
    check(peakOf(r.render(8192, { { 0, on(60) } })) == 0.f, "volume 0 = silence");
    r.setParam(pid::volume, 0.8f);
    r.render(2048, {});

    // tone: brighter = more energy above 3 kHz relative to total
    auto hfShare = [](const juce::AudioBuffer<float>& b)
    {
        juce::dsp::IIR::Filter<float> hp; hp.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(44100.0, 4000.0, 0.707f);
        double hf = 0, tot = 0;
        for (int i = 0; i < b.getNumSamples(); ++i) { const float x = b.getSample(0, i); const float y = hp.processSample(x); hf += y * y; tot += x * x; }
        return hf / std::max(1e-12, tot);
    };
    r.setParam(pid::tone, 1.f);  const double bright = hfShare(r.render(44100, { { 0, on(60) } }));
    r.setParam(pid::tone, 0.f);  const double neutral = hfShare(r.render(44100, { { 0, on(60) } }));
    r.setParam(pid::tone, -1.f); const double dark = hfShare(r.render(44100, { { 0, on(60) } }));
    check(bright > neutral * 1.15 && dark < neutral * 0.85, "tone moves the balance: dark " + juce::String(dark) + " neutral " + juce::String(neutral) + " bright " + juce::String(bright));
    r.setParam(pid::tone, 0.f);

    // space: adds a tail after the dry sound has ended, and is bass-free
    r.proc.selectSound(indexOfSound("Tight Clap"));
    const int tailFrom = 44100 / 2, total = 44100 * 2;
    auto dry = r.render(total, { { 0, on(60) } });
    r.setParam(pid::space, 1.f);
    auto wet = r.render(total, { { 0, on(60) } });
    check(rmsOf(dry, tailFrom, total) < 1e-6 && rmsOf(wet, tailFrom, total) > 1e-4, "space gives a tail: dry " + juce::String(rmsOf(dry, tailFrom, total)) + " wet " + juce::String(rmsOf(wet, tailFrom, total)));
    check(peakOf(wet) < 1.0f && allFinite(wet), "space output stays under full scale");
    // switching the send off must not leave a ghost tail behind
    r.setParam(pid::space, 0.f);
    r.render(4096, {});
    auto after = r.render(44100, {});
    check(peakOf(after) == 0.f, "no reverb tail after space is switched off");
    // 808 does not feed the reverb with sub bass
    r.proc.selectSound(indexOfSound("Clean Sub 808"));
    auto d8 = r.render(44100 * 3, { { 0, on(37) }, { 20000, off(37) } });
    r.setParam(pid::space, 1.f);
    auto w8 = r.render(44100 * 3, { { 0, on(37) }, { 20000, off(37) } });
    const double ratio8 = rmsOf(w8, 44100, 44100 * 3) / std::max(1e-9, rmsOf(w8, 5000, 20000));
    check(ratio8 < 0.03, "808 tail with space 100%% stays tiny (ratio " + juce::String(ratio8) + ")");
    (void) d8;
}

void testModesAndMidi()
{
    info("[modes and MIDI]");
    Rig r;
    // kit mode: GM notes trigger the right sounds, unmapped note is silent
    r.proc.setKitMode(true);
    check(peakOf(r.render(22050, { { 0, on(36) } })) > 0.1f, "kit kick");
    check(peakOf(r.render(22050, { { 0, on(38) } })) > 0.05f, "kit snare");
    check(peakOf(r.render(22050, { { 0, on(42) } })) > 0.02f, "kit hat");
    check(peakOf(r.render(22050, { { 0, on(3) } })) == 0.f, "unmapped key silent");
    // the kit renders exactly what the library says (kick at velocity 127 == sound at 48k->44.1k resample, so compare peak levels)
    const float p = peakOf(r.render(22050, { { 0, on(36, 127) } }));
    const float expect = std::pow(10.f, -7.8f / 20.f);       // Plan A Kick peak in the library (about -7.8 dBFS), Volume 80% = unity
    check(std::abs(p - expect) < 0.05f, "kit kick at full velocity peaks at " + juce::String(p) + ", library says " + juce::String(expect));
    // block size independence (also with tone + space active)
    Rig r1; r1.proc.setKitMode(true);
    r1.setParam(pid::tone, 0.5f); r1.setParam(pid::space, 0.4f);
    std::vector<std::pair<int, juce::MidiMessage>> ev;
    for (int i = 0; i < 16; ++i) { ev.push_back({ i * 4000 + 37, on(i % 2 ? 42 : 36, 60 + i * 4) }); ev.push_back({ i * 4000 + 2500, off(i % 2 ? 42 : 36) }); }
    ev.push_back({ 30000, on(39) });
    auto a = r1.render(88200, ev, 64);
    Rig r2; r2.proc.setKitMode(true); r2.setParam(pid::tone, 0.5f); r2.setParam(pid::space, 0.4f);
    auto b = r2.render(88200, ev, 1000);
    float maxDiff = 0.f; for (int c = 0; c < 2; ++c) for (int i = 0; i < 88200; ++i) maxDiff = std::max(maxDiff, std::abs(a.getSample(c, i) - b.getSample(c, i)));
    check(maxDiff < 1e-5f, "output independent of host block size (max diff " + juce::String(maxDiff) + ")");

    // single mode: 808 follows the keys, mono, and note-off ends it
    Rig s;
    s.proc.selectSound(indexOfSound("Warm 808"));
    auto n1 = s.render(44100 * 2, { { 0, on(37) }, { 44100, off(37) } });
    check(peakOf(n1, 0, 40000) > 0.2f && peakOf(n1, 48000, 44100 * 2) < 1e-6f, "808 stops after note-off");
    // other sample rates
    for (double sr : { 48000.0, 96000.0 })
    {
        Rig q(sr, 256);
        auto o = q.render((int) sr, { { 0, on(36) } });
        check(peakOf(o) > 0.2f && allFinite(o), "plays at " + juce::String(sr) + " Hz");
    }
    // all notes off releases gated sounds
    Rig t; t.proc.selectSound(indexOfSound("Long Boom 808"));
    auto cc = t.render(44100 * 2, { { 0, on(37) }, { 20000, juce::MidiMessage::allNotesOff(1) } });
    check(peakOf(cc, 30000, 44100 * 2) < 1e-6f, "all-notes-off releases an 808");
}

void testState()
{
    info("[state]");
    Rig a;
    a.proc.selectSound(indexOfSound("Wash Open Hat"));
    a.setParam(pid::tune, 3.5f); a.setParam(pid::space, 0.25f); a.setParam(pid::tone, -0.3f); a.setParam(pid::length, 0.6f);
    a.proc.selectKit(3);
    a.proc.setKitMode(false);
    juce::MemoryBlock mb; a.proc.getStateInformation(mb);
    Rig b;
    b.proc.setStateInformation(mb.getData(), (int) mb.getSize());
    check(b.proc.selectedSound() == indexOfSound("Wash Open Hat"), "sound restored");
    check(b.proc.selectedKit() == 3 && !b.proc.isKitMode(), "kit restored");
    check(std::abs(*b.proc.apvts.getRawParameterValue(pid::tune) - 3.5f) < 1e-3f && std::abs(*b.proc.apvts.getRawParameterValue(pid::space) - 0.25f) < 1e-3f &&
          std::abs(*b.proc.apvts.getRawParameterValue(pid::tone) + 0.3f) < 1e-3f && std::abs(*b.proc.apvts.getRawParameterValue(pid::length) - 0.6f) < 1e-3f, "knobs restored");
    // garbage must not crash
    Rig c; const char junk[] = "not a state"; c.proc.setStateInformation(junk, sizeof junk);
    check(c.proc.selectedSound() == indexOfSound("Plan A Kick"), "junk state ignored");
}

void screenshots(const juce::File& dir)
{
    dir.createDirectory();
    Rig r;
    std::unique_ptr<juce::AudioProcessorEditor> base(r.proc.createEditor());
    auto* ed = dynamic_cast<DrumsEditor*>(base.get());
    ed->setSize(1100, 770);
    juce::PNGImageFormat png;
    auto shot = [&](const juce::String& name)
    {
        juce::AudioBuffer<float> blk(2, 512);
        for (int i = 0; i < 20; ++i) { juce::MidiBuffer m; r.proc.processBlock(blk, m); ed->pumpForTesting(); }
        auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        auto file = dir.getChildFile("drums_" + name + ".png");
        file.deleteFile();
        juce::FileOutputStream fos(file);
        if (fos.openedOk()) png.writeImageToStream(img, fos);
    };
    shot("start");
    ed->showCategory((int) Cat::Bass808); ed->clickTile(1); shot("808");
    ed->showCategory((int) Cat::Hat);     ed->clickTile(6); shot("hats");
    ed->showCategory((int) Cat::Perc);    ed->clickTile(4); shot("percs");
    ed->showCategory((int) Cat::Fx);      ed->clickTile(2); shot("fx");
    ed->showCategory((int) Cat::Snare);   shot("snares");
    r.proc.selectKit(1); ed->showCategory((int) Cat::Clap); shot("kit");
    info("wrote screenshots to " + dir.getFullPathName());
}

void renderMidi(const juce::File& mid, const juce::File& wavOut, const juce::String& what, double bpm)
{
    juce::FileInputStream in(mid);
    juce::MidiFile mf; if (!in.openedOk() || !mf.readFrom(in)) { info("cannot read MIDI"); return; }
    mf.convertTimestampTicksToSeconds();
    Rig r(44100.0, 512);
    if (what.containsOnly("0123456789")) r.proc.selectKit(what.getIntValue());
    else
    {
        const int idx = indexOfSound(what.toStdString());
        if (idx < 0) { info("unknown sound " + what); return; }
        r.proc.selectSound(idx);
    }
    const double tickScale = 60.0 / bpm / (mf.getTimeFormat() > 0 ? mf.getTimeFormat() : 480);   // used only when the file has no tempo events
    (void) tickScale;
    std::vector<std::pair<int, juce::MidiMessage>> ev;
    double end = 0;
    for (int t = 0; t < mf.getNumTracks(); ++t)
        for (int i = 0; i < mf.getTrack(t)->getNumEvents(); ++i)
        {
            const auto& m = mf.getTrack(t)->getEventPointer(i)->message;
            if (m.isNoteOnOrOff()) { ev.push_back({ (int) std::lround(m.getTimeStamp() * r.sr), m }); end = std::max(end, m.getTimeStamp()); }
        }
    auto out = r.render((int) ((end + 3.0) * r.sr), ev);
    const float pk = peakOf(out);
    info("peak " + juce::String(pk) + "  length " + juce::String(end, 2) + " s");
    juce::WavAudioFormat wav;
    wavOut.deleteFile();
    std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(new juce::FileOutputStream(wavOut), r.sr, 2, 24, {}, 0));
    if (w) w->writeFromAudioSampleBuffer(out, 0, out.getNumSamples());
}
} // namespace

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::StringArray args;
    for (int i = 1; i < argc; ++i) args.add(argv[i]);
    if (args.size() >= 2 && args[0] == "--screenshot") { screenshots(juce::File::getCurrentWorkingDirectory().getChildFile(args[1])); return 0; }
    if (args.size() >= 3 && args[0] == "--rendermidi")
    {
        renderMidi(juce::File::getCurrentWorkingDirectory().getChildFile(args[1]), juce::File::getCurrentWorkingDirectory().getChildFile(args[2]),
                   args.size() > 3 ? args[3] : juce::String("0"), args.size() > 4 ? args[4].getDoubleValue() : 102.0);
        return 0;
    }
    testBasics();
    testVolumeToneSpace();
    testModesAndMidi();
    testState();
    std::cout << "\n" << checks << " checks, " << failures << " failures" << std::endl;
    return failures ? 1 : 0;
}
