#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <memory>
#include <vector>
#include "DrumsProcessor.h"

namespace drums
{
class DrumsLnf : public juce::LookAndFeel_V4
{
public:
    DrumsLnf();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getLabelFont(juce::Label&) override;
    static juce::Colour accent() { return juce::Colour(0xff4fd1c5); }
    static juce::Colour catColour(Cat c);
};

// all the sound tiles of the selected category, painted and hit-tested by hand
class SoundGrid : public juce::Component, private juce::SettableTooltipClient
{
public:
    explicit SoundGrid(DrumsProcessor& p) : proc(p) { setWantsKeyboardFocus(true); }
    void setCategory(Cat c);
    Cat category() const { return cat; }
    void refreshState();                                   // call when selection / mode / kit changed
    int  tileCount() const { return (int) ids.size(); }
    int  soundAtTile(int i) const { return ids[(size_t) i]; }
    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    std::function<void(int sound)> onPick;
    static constexpr int kCols = 6, kTileH = 100, kGap = 10;
private:
    int tileHeight() const;
    juce::Rectangle<int> tileBounds(int i) const;
    int tileAt(juce::Point<int>) const;
    DrumsProcessor& proc;
    Cat cat = Cat::Kick;
    std::vector<int> ids;
    int hover = -1, selected = -1;
    bool kitMode = false;
    int kit = 0;
};

class LevelMeter : public juce::Component
{
public:
    void set(float v) { if (std::abs(v - value) > 0.004f) { value = v; repaint(); } }
    void paint(juce::Graphics&) override;
private:
    float value = 0.f;
};

class DrumsEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DrumsEditor(DrumsProcessor&);
    ~DrumsEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

    // test hooks (offline screenshot tool)
    void showCategory(int c) { chooseCategory(c); }
    void pumpForTesting() { timerCallback(); }
    int  currentCategory() const { return (int) grid.category(); }
    int  tilesShown() const { return grid.tileCount(); }
    void clickTile(int i) { if (i >= 0 && i < grid.tileCount()) pick(grid.soundAtTile(i)); }

private:
    static constexpr int kDesignW = 1100, kDesignH = 770;
    struct KnobBox
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label caption;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    void addKnob(KnobBox& k, const char* paramId, const char* text, std::function<juce::String(double)> fmt, int x);
    void chooseCategory(int c);
    void pick(int sound);
    void setKit(bool kit);
    void updateInfo();
    void timerCallback() override;

    DrumsProcessor& proc;
    DrumsLnf lnf;
    juce::Component content;
    SoundGrid grid;
    std::array<juce::TextButton, kNumCats> catBtns;
    juce::TextButton singleBtn { "SINGLE SOUND" }, kitBtn { "FULL KIT" };
    juce::ComboBox kitBox, keysBox;
    juce::Label nameLabel, tagLabel, hintLabel, kitMapLabel, keysCaption, meterCaption;
    std::array<KnobBox, 5> knobs;
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> kitAtt, keysAtt;
    int lastSound = -1, lastKit = -1, lastMode = -1;
    LevelMeter levelMeter;
    float meter = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumsEditor)
};
} // namespace drums
