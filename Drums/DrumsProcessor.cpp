#include "DrumsProcessor.h"
#include "DrumsEditor.h"
#include <cmath>

namespace drums
{
juce::AudioProcessorValueTreeState::ParameterLayout DrumsProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const int n = numSounds();
    layout.add(std::make_unique<AudioParameterInt>(ParameterID { pid::sound, 1 }, "Sound", 0, n - 1, juce::jmax(0, indexOfSound("Plan A Kick"))));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { pid::mode, 1 }, "Mode", StringArray { "Single sound", "Kit" }, 0));
    StringArray kitNames;
    for (const auto& k : kits()) kitNames.add(k.name);
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { pid::kit, 1 }, "Kit", kitNames, 0));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { pid::keyMode, 1 }, "Keys", StringArray { "Auto", "All keys pitched", "Same pitch on all keys" }, 0));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { pid::volume, 1 }, "Volume", NormalisableRange<float>(0.f, 1.f, 0.001f), 0.8f));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { pid::tune, 1 }, "Tune", NormalisableRange<float>(-12.f, 12.f, 0.01f), 0.f));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { pid::length, 1 }, "Length", NormalisableRange<float>(0.05f, 1.f, 0.001f), 1.f));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { pid::tone, 1 }, "Tone", NormalisableRange<float>(-1.f, 1.f, 0.001f), 0.f));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { pid::space, 1 }, "Space", NormalisableRange<float>(0.f, 1.f, 0.001f), 0.f));
    return layout;
}

DrumsProcessor::DrumsProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "AuraDrums", createLayout()),
      bankPtr(Bank::get()),
      engine(bankPtr)
{
    pSound = apvts.getRawParameterValue(pid::sound);   pMode = apvts.getRawParameterValue(pid::mode);
    pKit = apvts.getRawParameterValue(pid::kit);       pKeyMode = apvts.getRawParameterValue(pid::keyMode);
    pVolume = apvts.getRawParameterValue(pid::volume); pTune = apvts.getRawParameterValue(pid::tune);
    pLength = apvts.getRawParameterValue(pid::length); pTone = apvts.getRawParameterValue(pid::tone);
    pSpace = apvts.getRawParameterValue(pid::space);
    updateSettings();
}

bool DrumsProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && l.getMainInputChannelSet().isDisabled();
}

void DrumsProcessor::prepareToPlay(double sr, int maxBlock)
{
    sampleRateHz = sr;
    engine.prepare(sr);
    updateSettings();
    scratch.setSize(2, juce::jmax(32, maxBlock), false, false, true);
    reverb.setSampleRate(sr);
    reverb.reset();
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.62f; rp.damping = 0.55f; rp.wetLevel = 0.33f; rp.dryLevel = 0.f; rp.width = 1.f; rp.freezeMode = 0.f;
    reverb.setParameters(rp);
    for (int c = 0; c < 2; ++c)
    {
        sendHp[c].reset(); lowShelf[c].reset(); highShelf[c].reset();
        sendHp[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, 320.0, 0.707f);
    }
    lastTone = 9.f;
    smoothVol = -1.f;
}

void DrumsProcessor::updateSettings()
{
    Settings s;
    s.kitMode = *pMode > 0.5f;
    s.sound = juce::jlimit(0, numSounds() - 1, (int) std::lround(*pSound));
    s.kit = juce::jlimit(0, (int) kits().size() - 1, (int) std::lround(*pKit));
    s.keyMode = (KeyMode) juce::jlimit(0, 2, (int) std::lround(*pKeyMode));
    s.tuneSemis = *pTune;
    s.length = *pLength;
    engine.setSettings(s);
}

void DrumsProcessor::updateTone()
{
    const float t = *pTone;
    if (std::abs(t - lastTone) < 1e-4f) return;
    lastTone = t;
    // tilt: positive = less low end and more top (brighter), negative = the other way round (max +-6 dB)
    const float g = juce::Decibels::decibelsToGain(6.f * t);
    for (int c = 0; c < 2; ++c)
    {
        lowShelf[c].coefficients  = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRateHz, 250.0, 0.6f, 1.f / g);
        highShelf[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRateHz, 3000.0, 0.6f, g);
    }
}

void DrumsProcessor::handleMidi(const juce::MidiMessage& m)
{
    if (m.isNoteOn())       engine.noteOn(m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff()) engine.noteOff(m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
}

void DrumsProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (buffer.getNumChannels() < 2) { midi.clear(); return; }
    updateSettings();

    const int a = pendingAudition.exchange(-1);
    if (a >= 0) engine.preview(a, 0.9f);

    float* L = buffer.getWritePointer(0);
    float* R = buffer.getWritePointer(1);

    keyboardState.processNextMidiBuffer(midi, 0, n, true);

    int done = 0;
    for (const auto meta : midi)
    {
        const int pos = juce::jlimit(done, n, meta.samplePosition);
        if (pos > done) { engine.render(L + done, R + done, pos - done); done = pos; }
        handleMidi(meta.getMessage());
    }
    if (done < n) engine.render(L + done, R + done, n - done);
    midi.clear();

    // ---- tone
    updateTone();
    if (std::abs(*pTone) > 0.004f)
        for (int i = 0; i < n; ++i)
        {
            L[i] = highShelf[0].processSample(lowShelf[0].processSample(L[i]));
            R[i] = highShelf[1].processSample(lowShelf[1].processSample(R[i]));
        }

    // ---- space (reverb send, bass kept out of it)
    const float space = *pSpace;
    if (space > 0.001f)
    {
        if (scratch.getNumSamples() < n) scratch.setSize(2, n, false, false, true);
        float* sl = scratch.getWritePointer(0);
        float* sr = scratch.getWritePointer(1);
        for (int i = 0; i < n; ++i) { sl[i] = sendHp[0].processSample(L[i]); sr[i] = sendHp[1].processSample(R[i]); }
        reverb.processStereo(sl, sr, n);
        const float w = 0.55f * space;
        for (int i = 0; i < n; ++i) { L[i] += sl[i] * w; R[i] += sr[i] * w; }
        reverbWasOn = true;
    }
    else if (reverbWasOn)
    {
        reverb.reset();            // the send was switched off: clear the tail once so it cannot come back later
        reverbWasOn = false;
    }

    // ---- volume (0.8 = unity) with a short ramp, then a transparent safety soft-clip for stacked hits
    const float target = *pVolume * 1.25f;
    if (smoothVol < 0.f) smoothVol = target;
    const float step = (target - smoothVol) / (float) juce::jmax(1, n);
    float pk = 0.f;
    for (int i = 0; i < n; ++i)
    {
        smoothVol += step;
        float* ch[2] = { L + i, R + i };
        for (auto* p : ch)
        {
            float x = *p * smoothVol;
            const float ax = std::abs(x);
            if (ax > 0.9f) x = (x < 0.f ? -1.f : 1.f) * (0.9f + 0.1f * std::tanh((ax - 0.9f) / 0.1f));
            *p = x; pk = juce::jmax(pk, std::abs(x));
        }
    }
    smoothVol = target;
    peakLevel.store(juce::jmax(pk, peakLevel.load() * 0.9f));
    voicesInUse.store(engine.activeVoices());
}

juce::AudioProcessorEditor* DrumsProcessor::createEditor() { return new DrumsEditor(*this); }

// ---------------------------------------------------------------------------------------------------- editor helpers
int DrumsProcessor::selectedSound() const { return juce::jlimit(0, numSounds() - 1, (int) std::lround(*pSound)); }
bool DrumsProcessor::isKitMode() const { return *pMode > 0.5f; }
int DrumsProcessor::selectedKit() const { return juce::jlimit(0, (int) kits().size() - 1, (int) std::lround(*pKit)); }

static void setChoice(juce::AudioProcessorValueTreeState& a, const char* id, int index)
{
    if (auto* p = a.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1((float) index));
}

void DrumsProcessor::selectSound(int index)
{
    index = juce::jlimit(0, numSounds() - 1, index);
    setChoice(apvts, pid::sound, index);
    setChoice(apvts, pid::mode, 0);
}
void DrumsProcessor::setKitMode(bool kit) { setChoice(apvts, pid::mode, kit ? 1 : 0); }
void DrumsProcessor::selectKit(int k) { setChoice(apvts, pid::kit, juce::jlimit(0, (int) kits().size() - 1, k)); setChoice(apvts, pid::mode, 1); }
void DrumsProcessor::auditionSound(int index) { pendingAudition.store(juce::jlimit(0, numSounds() - 1, index)); }

void DrumsProcessor::setCurrentProgram(int index) { selectSound(index); }
const juce::String DrumsProcessor::getProgramName(int index) { return soundInfo(index).name; }

void DrumsProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("soundName", juce::String(soundInfo(selectedSound()).name), nullptr);   // lets a later version re-find the sound
    state.setProperty("auraDrumsVersion", 1, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

void DrumsProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml(*xml);
            apvts.replaceState(vt);
            const int byName = indexOfSound(vt.getProperty("soundName").toString().toStdString());
            if (byName >= 0 && byName != selectedSound()) setChoice(apvts, pid::sound, byName);
        }
}
} // namespace drums

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new drums::DrumsProcessor(); }
