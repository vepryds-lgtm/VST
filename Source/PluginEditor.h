#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <functional>
#include <memory>
#include <vector>
#include "PluginProcessor.h"

namespace aura
{
enum class Fmt { Plain, Percent, Hz, Rate, Sec, Cents, Semi, Db, Ms, Int, Ratio };

class AuraLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AuraLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    static juce::Colour accent() { return juce::Colour(0xff4fd1c5); }
    static juce::Colour accent2() { return juce::Colour(0xfff6ad55); }
};

// One rotary parameter control: caption + knob + value read-out
class Knob : public juce::Component
{
public:
    Knob(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& caption, Fmt fmt = Fmt::Plain);
    void resized() override;
    void setBig();                       // larger caption / read-out for the front page
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label caption;
private:
    int captionH = 13;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class ChoiceBox : public juce::ComboBox
{
public:
    ChoiceBox(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

class ToggleParam : public juce::ToggleButton
{
public:
    ToggleParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& label);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

class HSlider : public juce::Slider
{
public:
    HSlider(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class Panel : public juce::Component
{
public:
    Panel(const juce::String& t = {}, juce::Colour c = AuraLookAndFeel::accent(), bool isFramed = true)
        : title(t), accent(c), framed(isFramed) {}
    void paint(juce::Graphics&) override;
    void resized() override { if (layout) layout(getLocalBounds()); }
    juce::String title;
    juce::Colour accent;
    bool framed;
    std::function<void(juce::Rectangle<int>)> layout;
};

// scrolling live trace of a modulation source
class ModScope : public juce::Component
{
public:
    explicit ModScope(juce::Colour c) : colour(c) { history.fill(0.f); }
    void push(float v) { history[(size_t) pos] = v; pos = (pos + 1) % (int) history.size(); repaint(); }
    void paint(juce::Graphics&) override;
private:
    std::array<float, 120> history;
    int pos = 0;
    juce::Colour colour;
};

class EnvMeter : public juce::Component
{
public:
    EnvMeter(const juce::String& n, juce::Colour c) : name(n), colour(c) {}
    void set(float v) { if (std::abs(v - value) > 0.002f) { value = v; repaint(); } }
    void paint(juce::Graphics&) override;
private:
    juce::String name; juce::Colour colour; float value = 0.f;
};

class AuraEditor : public juce::AudioProcessorEditor,
                   private juce::Timer,
                   private juce::ChangeListener,
                   private juce::ListBoxModel
{
public:
    explicit AuraEditor(AuraProcessor&);
    ~AuraEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

    // test hooks (used by the offline screenshot tool)
    void showTab(int index) { tabs.setCurrentTabIndex(index); }
    void showPage(bool advancedPage);
    bool isAdvancedPage() const { return advanced; }
    void clickGenre(int g) { selectGenre(g); }
    void clickType(int t) { selectType(t); }
    int  numShown() const { return (int) shown.size(); }
    void pumpForTesting() { timerCallback(); }   // lets the offline test tool refresh the live displays

private:
    static constexpr int kDesignW = 1280, kDesignH = 860;
    static constexpr int kNumTypeButtons = 10;   // All, 7 sound groups, Favorites, My presets

    // helpers
    Knob* addKnob(juce::Component& parent, const juce::String& id, const juce::String& caption, Fmt f = Fmt::Plain);
    ChoiceBox* addChoice(juce::Component& parent, const juce::String& id);
    HSlider* addHSlider(juce::Component& parent, const juce::String& id);
    ToggleParam* addToggle(juce::Component& parent, const juce::String& id, const juce::String& text);
    static void grid(const std::vector<juce::Component*>& items, int x, int y, int perRow, int cw = 62, int ch = 84, int gap = 2);
    void buildOscPanel(Panel& p, bool first);
    void buildMiscPanel(Panel& p);
    void buildFilterPanel(Panel& p);
    void buildEnvPanel(Panel& p, bool amp);
    void buildModTab(Panel& p);
    void buildFxTab(Panel& p);
    void buildVoiceTab(Panel& p);
    void buildLayerTab(Panel& p);
    void buildHeader();
    void buildBrowser();
    void buildSimplePage();
    void layoutBrowser();

    // front page logic
    void selectGenre(int g);          // -1 = no genre filter
    void selectType(int t);           // 0 = all, 1..7 = sound group, 8 = favourites, 9 = my presets
    void updateNowPlaying();
    void updateButtons();

    // preset browser
    void refreshList();
    void updateHeader();
    void showSaveDialog();
    void showLoadDialog();
    int getNumRows() override { return (int) shown.size(); }
    void paintListBoxItem(int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void returnKeyPressed(int row) override { activateRow(row); }
    void activateRow(int row);
    void changeListenerCallback(juce::ChangeBroadcaster*) override { refreshList(); updateHeader(); updateNowPlaying(); }
    void timerCallback() override;

    AuraProcessor& proc;
    AuraLookAndFeel lnf;
    juce::Component content;
    juce::Component simplePage, advPage;
    Panel headerPanel { {}, AuraLookAndFeel::accent(), true }, browserPanel { "PRESET BROWSER" }, nowPanel { "NOW PLAYING" };

    // header + browser
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, favBtn { "Fav" }, saveBtn { "Save" }, loadBtn { "Load" };
    juce::TextButton simpleBtn { "SIMPLE" }, advBtn { "ADVANCED" };
    juce::Label titleLabel, nameLabel, catLabel, countLabel;
    juce::TextEditor search;
    juce::ComboBox catBox;
    juce::ListBox list { {}, this };
    std::vector<int> shown;
    juce::String categoryFilter;          // advanced page only
    bool updatingList = false;
    bool advanced = false;
    int genreSel = -1, typeSel = 0;       // front page filters
    int tick = 0;
    std::unique_ptr<Knob> advVolume;
    std::unique_ptr<juce::FileChooser> chooser;

    // front page
    std::array<juce::TextButton, 6> genreBtns;
    std::array<juce::TextButton, kNumTypeButtons> typeBtns;
    juce::TextButton resetBtn { "RESET TO ORIGINAL SOUND" };
    juce::Label bigName, bigTags, editedLabel, helpLabel;
    std::vector<std::unique_ptr<Knob>> simpleKnobs;
    std::vector<std::unique_ptr<juce::Label>> simpleHints;

    // advanced panels
    Panel osc1 { "OSCILLATOR 1" }, osc2 { "OSCILLATOR 2" }, misc { "FM  /  SUB  /  NOISE" }, filter { "FILTER", AuraLookAndFeel::accent2() },
          fenv { "FILTER ENVELOPE", AuraLookAndFeel::accent2() }, aenv { "AMP ENVELOPE" };
    Panel modTab { {}, AuraLookAndFeel::accent(), false }, fxTab { {}, AuraLookAndFeel::accent(), false },
          voiceTab { {}, AuraLookAndFeel::accent(), false }, layerTab { {}, AuraLookAndFeel::accent(), false };
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    std::vector<std::unique_ptr<juce::Component>> owned;   // knobs, boxes, toggles, sub panels
    std::vector<std::unique_ptr<Panel>> subPanels;

    ModScope* scopes[3] {};
    EnvMeter *ampMeter = nullptr, *fltMeter = nullptr;
    juce::Label voicesLabel;

    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuraEditor)
};
} // namespace aura

using AuraEditor = aura::AuraEditor;
