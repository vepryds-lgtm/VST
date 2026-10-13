#include "DrumsEditor.h"
#include <cmath>

namespace drums
{
using juce::Colour;
using juce::Graphics;
using juce::Justification;

static juce::Font mkFont(float h, bool bold = false) { return juce::Font(juce::FontOptions(h, bold ? juce::Font::bold : juce::Font::plain)); }

static juce::String noteName(int n)
{
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String(names[((n % 12) + 12) % 12]) + juce::String(n / 12);      // FL Studio naming: MIDI 60 = C5
}

Colour DrumsLnf::catColour(Cat c)
{
    switch (c)
    {
        case Cat::Kick:    return Colour(0xfff6ad55);
        case Cat::Bass808: return Colour(0xffed64a6);
        case Cat::Clap:    return Colour(0xff4fd1c5);
        case Cat::Snare:   return Colour(0xff63b3ed);
        case Cat::Hat:     return Colour(0xfff6e05e);
        case Cat::Perc:    return Colour(0xff9ae6b4);
        case Cat::Fx:      break;
    }
    return Colour(0xffb794f4);
}

DrumsLnf::DrumsLnf()
{
    const Colour text(0xffe2e8f0);
    setColour(juce::ResizableWindow::backgroundColourId, Colour(0xff12141a));
    setColour(juce::Label::textColourId, text);
    setColour(juce::Slider::textBoxTextColourId, Colour(0xffcbd5e1));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId, Colour(0xff252a36));
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::outlineColourId, Colour(0xff343b4d));
    setColour(juce::ComboBox::arrowColourId, accent());
    setColour(juce::PopupMenu::backgroundColourId, Colour(0xff1e222d));
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accent().withAlpha(0.35f));
}

juce::Font DrumsLnf::getLabelFont(juce::Label& l) { return mkFont(l.getFont().getHeight(), l.getFont().isBold()); }

void DrumsLnf::drawRotarySlider(Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    auto b = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(3.f);
    const float r = juce::jmin(b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = start + pos * (end - start);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    juce::Path track;
    track.addCentredArc(c.x, c.y, r - 2.f, r - 2.f, 0.f, start, end, true);
    g.setColour(Colour(0xff2f3646));
    g.strokePath(track, juce::PathStrokeType(3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    float from = start;
    if (bipolar) from = start + (float) s.valueToProportionOfLength(0.0) * (end - start);
    juce::Path val;
    val.addCentredArc(c.x, c.y, r - 2.f, r - 2.f, 0.f, from, angle, true);
    g.setColour(accent());
    g.strokePath(val, juce::PathStrokeType(3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float br = r - 7.f;
    g.setColour(Colour(0xff2a2f3b));
    g.fillEllipse(c.x - br, c.y - br, br * 2.f, br * 2.f);
    g.setColour(Colour(0xff3a4153));
    g.drawEllipse(c.x - br, c.y - br, br * 2.f, br * 2.f, 1.f);
    juce::Path p;
    p.addRoundedRectangle(-1.5f, -br + 1.f, 3.f, br * 0.55f, 1.f);
    g.setColour(Colour(0xfff1f5f9));
    g.fillPath(p, juce::AffineTransform::rotation(angle).translated(c.x, c.y));
}

void DrumsLnf::drawButtonBackground(Graphics& g, juce::Button& b, const Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced(1.f);
    const bool on = b.getToggleState();
    const Colour base = b.findColour(juce::TextButton::buttonOnColourId);
    g.setColour(on ? base.withAlpha(0.9f) : Colour(0xff252a36).brighter(down ? 0.15f : highlighted ? 0.08f : 0.f));
    g.fillRoundedRectangle(r, 7.f);
    g.setColour(on ? base.brighter(0.3f) : Colour(0xff343b4d));
    g.drawRoundedRectangle(r, 7.f, 1.2f);
}

void DrumsLnf::drawButtonText(Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour(b.getToggleState() ? Colour(0xff0b1015) : Colour(0xffcbd5e1));
    g.setFont(mkFont(14.f, true));
    g.drawText(b.getButtonText(), b.getLocalBounds(), Justification::centred);
}

// ---------------------------------------------------------------------------------------------------- grid
void SoundGrid::setCategory(Cat c)
{
    cat = c; ids.clear();
    for (int i = 0; i < numSounds(); ++i) if (soundInfo(i).cat == c) ids.push_back(i);
    hover = -1;
    refreshState();
}

void SoundGrid::refreshState()
{
    selected = proc.selectedSound();
    kitMode = proc.isKitMode();
    kit = proc.selectedKit();
    repaint();
}

int SoundGrid::tileHeight() const
{
    const int rows = std::max(1, ((int) ids.size() + kCols - 1) / kCols);
    return juce::jlimit(kTileH, 150, (getHeight() - (rows - 1) * kGap) / rows);     // few sounds -> bigger tiles
}

juce::Rectangle<int> SoundGrid::tileBounds(int i) const
{
    const int w = (getWidth() - (kCols - 1) * kGap) / kCols;
    const int h = tileHeight();
    return { (i % kCols) * (w + kGap), (i / kCols) * (h + kGap), w, h };
}

int SoundGrid::tileAt(juce::Point<int> p) const
{
    for (int i = 0; i < (int) ids.size(); ++i) if (tileBounds(i).contains(p)) return i;
    return -1;
}

void SoundGrid::mouseMove(const juce::MouseEvent& e)
{
    const int h = tileAt(e.getPosition());
    if (h != hover) { hover = h; repaint(); }
}

void SoundGrid::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const int t = tileAt(e.getPosition());
    if (t >= 0 && onPick) onPick(ids[(size_t) t]);
}

bool SoundGrid::keyPressed(const juce::KeyPress& k)
{
    if (ids.empty() || !onPick) return false;
    int cur = -1;
    for (int i = 0; i < (int) ids.size(); ++i) if (ids[(size_t) i] == selected) cur = i;
    int next = cur;
    if (k == juce::KeyPress::rightKey) next = cur + 1;
    else if (k == juce::KeyPress::leftKey) next = cur - 1;
    else if (k == juce::KeyPress::downKey) next = cur + kCols;
    else if (k == juce::KeyPress::upKey) next = cur - kCols;
    else return false;
    if (cur < 0) next = 0;
    next = juce::jlimit(0, (int) ids.size() - 1, next);
    onPick(ids[(size_t) next]);
    return true;
}

void SoundGrid::paint(Graphics& g)
{
    const auto& bank = proc.bank();
    const Colour cc = DrumsLnf::catColour(cat);
    for (int i = 0; i < (int) ids.size(); ++i)
    {
        const int sid = ids[(size_t) i];
        const auto& si = soundInfo(sid);
        const auto r = tileBounds(i).toFloat();
        const bool sel = !kitMode && sid == selected;
        const bool inKit = kitMode && [&] { for (const auto& e : kits()[(size_t) kit].map) if (indexOfSound(e.sound) == sid) return true; return false; }();
        g.setColour(sel ? cc.withAlpha(0.20f) : Colour(0xff1b1e27).brighter(hover == i ? 0.06f : 0.f));
        g.fillRoundedRectangle(r, 8.f);
        g.setColour(sel ? cc : inKit ? cc.withAlpha(0.6f) : (hover == i ? Colour(0xff4b5468) : Colour(0xff2b3040)));
        g.drawRoundedRectangle(r.reduced(0.5f), 8.f, sel ? 2.f : 1.2f);

        g.setColour(Colour(0xfff1f5f9));
        g.setFont(mkFont(15.f, true));
        g.drawText(juce::String(si.name), r.toNearestInt().withTrimmedLeft(11).withTrimmedRight(40).withHeight(26).translated(0, 4), Justification::centredLeft, true);
        g.setColour(Colour(0xff94a3b8));
        g.setFont(mkFont(11.5f));
        g.drawText(juce::String(si.tag), r.toNearestInt().withTrimmedLeft(11).withTrimmedRight(8).withHeight(16).translated(0, 29), Justification::centredLeft, true);

        // waveform envelope
        const auto wf = r.reduced(11.f, 0.f).withTop(r.getY() + 52.f).withBottom(r.getBottom() - 20.f);
        const auto& th = bank.thumbs[(size_t) sid];
        const float bw = wf.getWidth() / (float) kThumbPoints;
        g.setColour(sel ? cc : cc.withAlpha(0.75f));
        for (int p = 0; p < kThumbPoints; ++p)
        {
            const float hh = juce::jmax(1.f, th[(size_t) p] * wf.getHeight() * 0.5f);
            g.fillRect(wf.getX() + (float) p * bw, wf.getCentreY() - hh, juce::jmax(1.f, bw - 0.6f), hh * 2.f);
        }
        // footer
        g.setFont(mkFont(10.5f));
        g.setColour(Colour(0xff7b87a3));
        const double secs = (double) bank.samples[(size_t) sid]->length() / kBankRate;
        g.drawText(juce::String(secs, 2) + " s", r.toNearestInt().withTrimmedRight(10).withTop((int) r.getBottom() - 16).translated(0, 0), Justification::centredRight);
        if (si.rootNote >= 0)
            g.drawText(si.gated ? "BASS NOTE" : "PITCHED", r.toNearestInt().withTrimmedLeft(11).withTop((int) r.getBottom() - 16), Justification::centredLeft);
        if (kitMode)
            for (const auto& e : kits()[(size_t) kit].map)
                if (indexOfSound(e.sound) == sid)
                {
                    g.setColour(cc);
                    g.setFont(mkFont(12.f, true));
                    g.drawText(noteName(e.note), r.toNearestInt().withTrimmedRight(8).withHeight(24).translated(0, 5), Justification::centredRight);
                    break;
                }
    }
}

void LevelMeter::paint(Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour(Colour(0xff252a36));
    g.fillRoundedRectangle(r, 3.f);
    const float h = r.getHeight() * juce::jlimit(0.f, 1.f, value);
    g.setColour(value > 0.9f ? Colour(0xfff56565) : DrumsLnf::accent());
    g.fillRoundedRectangle(r.withTop(r.getBottom() - h), 3.f);
}

// ---------------------------------------------------------------------------------------------------- editor
DrumsEditor::DrumsEditor(DrumsProcessor& p)
    : AudioProcessorEditor(&p), proc(p), grid(p), keyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&lnf);
    content.setBounds(0, 0, kDesignW, kDesignH);
    addAndMakeVisible(content);

    // header: mode switch
    for (auto* b : { &singleBtn, &kitBtn })
    {
        b->setClickingTogglesState(false);
        b->setColour(juce::TextButton::buttonOnColourId, DrumsLnf::accent());
        content.addAndMakeVisible(*b);
    }
    singleBtn.setBounds(640, 16, 150, 34);
    kitBtn.setBounds(796, 16, 120, 34);
    singleBtn.onClick = [this] { setKit(false); };
    kitBtn.onClick = [this] { setKit(true); };
    {
        int i = 0;
        for (const auto& k : kits()) kitBox.addItem(k.name, ++i);
    }
    kitBox.setBounds(926, 16, 146, 34);
    content.addAndMakeVisible(kitBox);
    kitAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, pid::kit, kitBox);

    // category tabs
    const int tabW = (1044 - 6 * 8) / 7;
    for (int c = 0; c < kNumCats; ++c)
    {
        auto& b = catBtns[(size_t) c];
        int count = 0;
        for (int i = 0; i < numSounds(); ++i) count += soundInfo(i).cat == (Cat) c ? 1 : 0;
        b.setButtonText(juce::String(catName((Cat) c)) + "   " + juce::String(count));
        b.setColour(juce::TextButton::buttonOnColourId, DrumsLnf::catColour((Cat) c));
        b.setBounds(28 + c * (tabW + 8), 76, tabW, 40);
        b.onClick = [this, c] { chooseCategory(c); };
        content.addAndMakeVisible(b);
    }

    grid.setBounds(28, 128, 1044, 3 * SoundGrid::kTileH + 2 * SoundGrid::kGap);
    grid.onPick = [this](int s) { pick(s); };
    content.addAndMakeVisible(grid);

    for (auto* l : { &nameLabel, &tagLabel, &hintLabel, &kitMapLabel })
    {
        l->setInterceptsMouseClicks(false, false);
        content.addAndMakeVisible(*l);
    }
    nameLabel.setFont(mkFont(22.f, true));  nameLabel.setBounds(44, 466, 600, 30);
    tagLabel.setFont(mkFont(13.f));         tagLabel.setBounds(44, 496, 1000, 20);   tagLabel.setColour(juce::Label::textColourId, Colour(0xff94a3b8));
    hintLabel.setFont(mkFont(13.f));        hintLabel.setBounds(44, 518, 1000, 22);  hintLabel.setColour(juce::Label::textColourId, DrumsLnf::accent());
    kitMapLabel.setFont(mkFont(12.f));      kitMapLabel.setBounds(44, 500, 1000, 44); kitMapLabel.setColour(juce::Label::textColourId, Colour(0xffcbd5e1));
    kitMapLabel.setMinimumHorizontalScale(1.f);

    // knobs
    addKnob(knobs[0], pid::volume, "VOLUME", [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + " %"; }, 60);
    addKnob(knobs[1], pid::tune, "TUNE", [](double v) { return (v >= 0 ? "+" : "") + juce::String(v, 1) + " st"; }, 170);
    addKnob(knobs[2], pid::length, "LENGTH", [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + " %"; }, 280);
    addKnob(knobs[3], pid::tone, "TONE", [](double v) { return v < -0.02 ? "Darker " + juce::String(juce::roundToInt(-v * 100.0)) : v > 0.02 ? "Brighter " + juce::String(juce::roundToInt(v * 100.0)) : juce::String("Neutral"); }, 390);
    addKnob(knobs[4], pid::space, "SPACE", [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + " %"; }, 500);

    keysCaption.setText("KEYS", juce::dontSendNotification);
    keysCaption.setFont(mkFont(11.f, true)); keysCaption.setColour(juce::Label::textColourId, Colour(0xff94a3b8));
    keysCaption.setBounds(660, 578, 200, 16);
    content.addAndMakeVisible(keysCaption);
    keysBox.addItemList(juce::StringArray { "Auto (pitched sounds follow the keys)", "All keys pitched", "Same pitch on all keys" }, 1);
    keysBox.setBounds(660, 598, 280, 30);
    content.addAndMakeVisible(keysBox);
    keysAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, pid::keyMode, keysBox);

    meterCaption.setText("LEVEL", juce::dontSendNotification);
    meterCaption.setFont(mkFont(11.f, true)); meterCaption.setColour(juce::Label::textColourId, Colour(0xff94a3b8));
    meterCaption.setJustificationType(Justification::centred);
    meterCaption.setBounds(1000, 570, 52, 16);
    content.addAndMakeVisible(meterCaption);
    levelMeter.setBounds(1016, 590, 20, 66);
    content.addAndMakeVisible(levelMeter);

    keyboard.setBounds(28, 690, 1044, 66);
    keyboard.setAvailableRange(24, 96);
    keyboard.setLowestVisibleKey(30);
    keyboard.setKeyWidth(24.f);
    keyboard.setScrollButtonsVisible(false);
    keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, DrumsLnf::accent().withAlpha(0.7f));
    content.addAndMakeVisible(keyboard);

    chooseCategory((int) soundInfo(proc.selectedSound()).cat);
    lastSound = proc.selectedSound(); lastMode = proc.isKitMode() ? 1 : 0; lastKit = proc.selectedKit();
    updateInfo();

    setResizable(true, true);
    getConstrainer()->setFixedAspectRatio((double) kDesignW / (double) kDesignH);
    setResizeLimits(620, (int) (620.0 * kDesignH / kDesignW), 2200, (int) (2200.0 * kDesignH / kDesignW));
    setSize(1100, kDesignH);
    startTimerHz(30);
}

DrumsEditor::~DrumsEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void DrumsEditor::addKnob(KnobBox& k, const char* paramId, const char* text, std::function<juce::String(double)> fmt, int x)
{
    k.slider.setBounds(x, 592, 100, 82);
    k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 100, 18);
    k.slider.setPopupDisplayEnabled(false, false, nullptr);
    k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, paramId, k.slider);
    k.slider.textFromValueFunction = fmt;          // after the attachment, which installs its own text function
    k.slider.setDoubleClickReturnValue(true, proc.apvts.getParameter(paramId)->convertFrom0to1(proc.apvts.getParameter(paramId)->getDefaultValue()));
    k.slider.updateText();
    k.caption.setText(text, juce::dontSendNotification);
    k.caption.setJustificationType(Justification::centred);
    k.caption.setFont(mkFont(12.f, true));
    k.caption.setColour(juce::Label::textColourId, Colour(0xffe2e8f0));
    k.caption.setBounds(x, 572, 100, 18);
    k.caption.setInterceptsMouseClicks(false, false);
    content.addAndMakeVisible(k.slider);
    content.addAndMakeVisible(k.caption);
}

void DrumsEditor::paint(Graphics& g)
{
    g.fillAll(Colour(0xff12141a));
    const float s = juce::jmin((float) getWidth() / (float) kDesignW, (float) getHeight() / (float) kDesignH);
    g.addTransform(juce::AffineTransform::scale(s));
    g.setColour(DrumsLnf::accent());
    g.setFont(mkFont(30.f, true));
    g.drawText("AURADRUMS", 28, 10, 320, 38, Justification::centredLeft);
    g.setColour(Colour(0xff94a3b8));
    g.setFont(mkFont(13.f));
    g.drawText("Pick a sound. It is already mixed and level-matched - nothing else to do.", 30, 46, 600, 20, Justification::centredLeft);
    // info + control panels
    g.setColour(Colour(0xff1b1e27));
    g.fillRoundedRectangle(28.f, 458.f, 1044.f, 92.f, 10.f);
    g.fillRoundedRectangle(28.f, 560.f, 1044.f, 118.f, 10.f);
    g.setColour(Colour(0xff2b3040));
    g.drawRoundedRectangle(28.5f, 458.5f, 1043.f, 91.f, 10.f, 1.f);
    g.drawRoundedRectangle(28.5f, 560.5f, 1043.f, 117.f, 10.f, 1.f);
}

void DrumsEditor::resized()
{
    const float s = juce::jmin((float) getWidth() / (float) kDesignW, (float) getHeight() / (float) kDesignH);
    content.setTransform(juce::AffineTransform::scale(s));
}

void DrumsEditor::chooseCategory(int c)
{
    c = juce::jlimit(0, kNumCats - 1, c);
    grid.setCategory((Cat) c);
    for (int i = 0; i < kNumCats; ++i) catBtns[(size_t) i].setToggleState(i == c, juce::dontSendNotification);
    repaint();
}

void DrumsEditor::pick(int sound)
{
    if (proc.isKitMode()) proc.auditionSound(sound);          // kit: the tile only previews
    else { proc.selectSound(sound); proc.auditionSound(sound); }
    timerCallback();
}

void DrumsEditor::setKit(bool kit)
{
    proc.setKitMode(kit);
    timerCallback();
}

void DrumsEditor::updateInfo()
{
    const bool kit = proc.isKitMode();
    singleBtn.setToggleState(!kit, juce::dontSendNotification);
    kitBtn.setToggleState(kit, juce::dontSendNotification);
    kitBox.setVisible(kit);
    kitMapLabel.setVisible(kit);
    tagLabel.setVisible(!kit);
    hintLabel.setVisible(!kit);
    if (kit)
    {
        const auto& k = kits()[(size_t) proc.selectedKit()];
        nameLabel.setText(juce::String(k.name) + "  -  plays a normal drum MIDI file straight away", juce::dontSendNotification);
        auto map = k.map;
        std::sort(map.begin(), map.end(), [](const KitEntry& a, const KitEntry& b) { return a.note < b.note; });
        juce::String t;
        for (const auto& e : map) t << noteName(e.note) << " " << e.sound << "    ";
        kitMapLabel.setText(t.trim() + "      (808s are played in Single sound mode)", juce::dontSendNotification);
        nameLabel.setColour(juce::Label::textColourId, Colour(0xfff1f5f9));
        return;
    }
    const auto& si = soundInfo(proc.selectedSound());
    nameLabel.setText(juce::String(si.name), juce::dontSendNotification);
    nameLabel.setColour(juce::Label::textColourId, DrumsLnf::catColour(si.cat));
    tagLabel.setText(juce::String(si.tag) + "   |   " + juce::String(catSingular(si.cat)), juce::dontSendNotification);
    juce::String hint;
    if (si.gated) hint = "Plays as a bass note: key " + noteName(si.rootNote) + " is the original pitch, other keys play a melody. One note at a time, note length = 808 length.";
    else if (si.rootNote >= 0) hint = "Follows the keyboard: key " + noteName(si.rootNote) + " is the original pitch.";
    else hint = "Same sound on every key - just play any key (or click a tile to hear it).";
    hintLabel.setText(hint, juce::dontSendNotification);
}

void DrumsEditor::timerCallback()
{
    const int s = proc.selectedSound(), m = proc.isKitMode() ? 1 : 0, k = proc.selectedKit();
    if (s != lastSound || m != lastMode || k != lastKit)
    {
        if (s != lastSound && m == 0 && (int) soundInfo(s).cat != (int) grid.category()) chooseCategory((int) soundInfo(s).cat);
        lastSound = s; lastMode = m; lastKit = k;
        grid.refreshState();
        updateInfo();
    }
    meter = juce::jmax(proc.peakLevel.load(), meter * 0.85f);
    levelMeter.set(juce::jlimit(0.f, 1.f, std::sqrt(meter)));
}
} // namespace drums
