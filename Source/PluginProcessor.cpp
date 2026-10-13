#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace aura;

AuraProcessor::AuraProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "AURASTATE", createParameterLayout()),
      presets(apvts)
{
    for (int i = 0; i < kNumParams; ++i)
    {
        raw[(size_t) i] = apvts.getRawParameterValue(paramDef(i).id);
        values[i] = paramDefault(i);
    }
    presets.load(0); // sensible, immediately playable default sound
}

void AuraProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine.prepare(sampleRate, samplesPerBlock);
    fx.prepare(sampleRate, samplesPerBlock);
    keyboardState.reset();
}

bool AuraProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void AuraProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    for (int i = 0; i < kNumParams; ++i) values[i] = raw[(size_t) i]->load();

    keyboardState.processNextMidiBuffer(midi, 0, n, true);

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = *b;

    engine.process(buffer, midi, values, bpm);
    fx.process(buffer, values, bpm);
    midi.clear();
}

juce::AudioProcessorEditor* AuraProcessor::createEditor() { return new AuraEditor(*this); }

void AuraProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("presetName", presets.currentName(), nullptr);
    state.setProperty("presetCategory", presets.currentCategory(), nullptr);
    state.setProperty("auraVersion", 1, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void AuraProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            presets.setCurrentFromState(apvts.state.getProperty("presetName").toString(),
                                        apvts.state.getProperty("presetCategory").toString());
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AuraProcessor(); }
