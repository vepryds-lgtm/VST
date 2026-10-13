#include "PluginEditor.h"
#include "Presets.h"

namespace aura
{
using juce::Colour;
using juce::Font;
using juce::FontOptions;
using juce::Graphics;
using juce::Justification;

static Font mkFont(float h, bool bold = false) { return Font(FontOptions(h, bold ? Font::bold : Font::plain)); }

// =====================================================================================
//  Look & feel
// =====================================================================================
AuraLookAndFeel::AuraLookAndFeel()
{
    const Colour bg(0xff12141a), panel(0xff1b1e27), text(0xffe2e8f0);
    setColour(juce::ResizableWindow::backgroundColourId, bg);
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
    setColour(juce::TextButton::buttonColourId, Colour(0xff2a3040));
    setColour(juce::TextButton::textColourOffId, text);
    setColour(juce::TextEditor::backgroundColourId, Colour(0xff252a36));
    setColour(juce::TextEditor::textColourId, text);
    setColour(juce::TextEditor::outlineColourId, Colour(0xff343b4d));
    setColour(juce::TextEditor::focusedOutlineColourId, accent());
    setColour(juce::ListBox::backgroundColourId, panel);
    setColour(juce::ListBox::outlineColourId, Colour(0xff2b3040));
    setColour(juce::TabbedButtonBar::tabOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TabbedComponent::backgroundColourId, bg);
    setColour(juce::ScrollBar::thumbColourId, Colour(0xff3b4358));
    setColour(juce::AlertWindow::backgroundColourId, Colour(0xff1e222d));
    setColour(juce::AlertWindow::textColourId, text);
}

juce::Font AuraLookAndFeel::getLabelFont(juce::Label& l) { return Font(FontOptions(l.getFont().getHeight(), l.getFont().isBold() ? Font::bold : Font::plain)); }

juce::Font AuraLookAndFeel::getTextButtonFont(juce::TextButton&, int h)
{
    return mkFont(juce::jlimit(12.f, 19.f, (float) h * 0.38f), true);
}

void AuraLookAndFeel::drawRotarySlider(Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
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
    g.setColour(s.isEnabled() ? accent() : Colour(0xff4a5568));
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

void AuraLookAndFeel::drawLinearSlider(Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                       juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal) { LookAndFeel_V4::drawLinearSlider(g, x, y, w, h, pos, 0, 0, style, s); return; }
    const float cy = (float) y + (float) h * 0.5f;
    juce::Rectangle<float> track((float) x, cy - 2.f, (float) w, 4.f);
    g.setColour(Colour(0xff2f3646));
    g.fillRoundedRectangle(track, 2.f);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zero = bipolar ? (float) x + (float) s.valueToProportionOfLength(0.0) * (float) w : (float) x;
    g.setColour(accent());
    g.fillRoundedRectangle(juce::Rectangle<float>(juce::jmin(zero, pos), cy - 2.f, std::abs(pos - zero), 4.f), 2.f);
    g.setColour(Colour(0xfff1f5f9));
    g.fillEllipse(pos - 6.f, cy - 6.f, 12.f, 12.f);
}

void AuraLookAndFeel::drawToggleButton(Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced(1.f);
    const bool on = b.getToggleState();
    g.setColour(on ? accent().withAlpha(0.85f) : Colour(0xff2a3040));
    g.fillRoundedRectangle(r, 5.f);
    g.setColour(highlighted ? Colour(0xff7b87a3) : Colour(0xff3b4358));
    g.drawRoundedRectangle(r, 5.f, 1.f);
    g.setColour(on ? Colour(0xff0b1015) : Colour(0xffcbd5e1));
    g.setFont(mkFont(12.f, true));
    g.drawText(b.getButtonText(), b.getLocalBounds(), Justification::centred);
}

// =====================================================================================
//  Small components
// =====================================================================================
static juce::String fmtText(Fmt f, double v)
{
    switch (f)
    {
        case Fmt::Percent: return juce::String(juce::roundToInt(v * 100.0)) + " %";
        case Fmt::Hz:      return v >= 1000.0 ? juce::String(v / 1000.0, 2) + " kHz" : juce::String(juce::roundToInt(v)) + " Hz";
        case Fmt::Rate:    return juce::String(v, v < 1.0 ? 2 : 1) + " Hz";
        case Fmt::Sec:     return v < 1.0 ? juce::String(juce::roundToInt(v * 1000.0)) + " ms" : juce::String(v, 2) + " s";
        case Fmt::Cents:   return juce::String(juce::roundToInt(v)) + " ct";
        case Fmt::Semi:    return juce::String(juce::roundToInt(v)) + " st";
        case Fmt::Db:      return juce::String(v, 1) + " dB";
        case Fmt::Ms:      return juce::String(juce::roundToInt(v)) + " ms";
        case Fmt::Int:     return juce::String(juce::roundToInt(v));
        case Fmt::Ratio:   return juce::String(v, 2) + "x";
        default:           return juce::String(v, 2);
    }
}

static double fmtParse(Fmt f, const juce::String& t)
{
    const auto s = t.trim().toLowerCase();
    double v = s.getDoubleValue();
    switch (f)
    {
        case Fmt::Percent: return v / 100.0;
        case Fmt::Hz:      return s.contains("k") ? v * 1000.0 : v;
        case Fmt::Sec:     return s.contains("ms") ? v / 1000.0 : v;
        default:           return v;
    }
}

Knob::Knob(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& cap, Fmt fmt)
{
    addAndMakeVisible(slider);
    addAndMakeVisible(caption);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 14);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramId, slider);
    slider.textFromValueFunction = [fmt](double v) { return fmtText(fmt, v); };
    slider.valueFromTextFunction = [fmt](const juce::String& t) { return fmtParse(fmt, t); };
    slider.updateText();
    slider.setPopupDisplayEnabled(false, false, nullptr);
    caption.setText(cap, juce::dontSendNotification);
    caption.setJustificationType(Justification::centred);
    caption.setFont(mkFont(11.f));
    caption.setColour(juce::Label::textColourId, Colour(0xff94a3b8));
    caption.setInterceptsMouseClicks(false, false);
}

void Knob::setBig()
{
    captionH = 22;
    caption.setFont(mkFont(15.f, true));
    caption.setColour(juce::Label::textColourId, Colour(0xffe2e8f0));
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 22);
    slider.setColour(juce::Slider::textBoxTextColourId, AuraLookAndFeel::accent());
    resized();
}

void Knob::resized()
{
    caption.setBounds(0, 0, getWidth(), captionH);
    slider.setBounds(0, captionH, getWidth(), getHeight() - captionH);
}

ChoiceBox::ChoiceBox(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(paramId)))
        addItemList(p->choices, 1);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, paramId, *this);
    setJustificationType(Justification::centredLeft);
}

ToggleParam::ToggleParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& label)
{
    setButtonText(label);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, paramId, *this);
}

HSlider::HSlider(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId)
{
    setSliderStyle(juce::Slider::LinearHorizontal);
    setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 20);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramId, *this);
    textFromValueFunction = [](double v) { return juce::String(v, 2); };
    updateText();
}

void Panel::paint(Graphics& g)
{
    if (!framed) return;
    auto b = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(Colour(0xff1b1e27));
    g.fillRoundedRectangle(b, 8.f);
    g.setColour(Colour(0xff2b3040));
    g.drawRoundedRectangle(b, 8.f, 1.f);
    if (title.isNotEmpty())
    {
        g.setColour(accent);
        g.setFont(mkFont(11.f, true));
        g.drawText(title, 12, 4, getWidth() - 24, 14, Justification::centredLeft);
    }
}

void ModScope::paint(Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(Colour(0xff14171e));
    g.fillRoundedRectangle(b, 5.f);
    g.setColour(Colour(0xff2b3040));
    g.drawHorizontalLine((int) (b.getCentreY()), b.getX() + 2, b.getRight() - 2);
    juce::Path path;
    const int n = (int) history.size();
    for (int i = 0; i < n; ++i)
    {
        const float v = history[(size_t) ((pos + i) % n)];
        const float x = b.getX() + 3.f + (b.getWidth() - 6.f) * (float) i / (float) (n - 1);
        const float y = b.getCentreY() - v * (b.getHeight() * 0.5f - 6.f);
        if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
    }
    g.setColour(colour);
    g.strokePath(path, juce::PathStrokeType(1.6f));
    g.setColour(Colour(0xff2b3040));
    g.drawRoundedRectangle(b.reduced(0.5f), 5.f, 1.f);
}

void EnvMeter::paint(Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(Colour(0xff94a3b8));
    g.setFont(mkFont(11.f));
    g.drawText(name, 0, 0, 62, (int) b.getHeight(), Justification::centredLeft);
    auto bar = b.withTrimmedLeft(66.f).reduced(0.f, 3.f);
    g.setColour(Colour(0xff14171e));
    g.fillRoundedRectangle(bar, 3.f);
    g.setColour(colour);
    g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * juce::jlimit(0.f, 1.f, value)), 3.f);
}

// =====================================================================================
//  Editor
// =====================================================================================
Knob* AuraEditor::addKnob(juce::Component& parent, const juce::String& id, const juce::String& caption, Fmt f)
{
    auto k = std::make_unique<Knob>(proc.apvts, id, caption, f);
    auto* raw = k.get();
    parent.addAndMakeVisible(raw);
    owned.push_back(std::move(k));
    return raw;
}
ChoiceBox* AuraEditor::addChoice(juce::Component& parent, const juce::String& id)
{
    auto c = std::make_unique<ChoiceBox>(proc.apvts, id);
    auto* raw = c.get();
    parent.addAndMakeVisible(raw);
    owned.push_back(std::move(c));
    return raw;
}
HSlider* AuraEditor::addHSlider(juce::Component& parent, const juce::String& id)
{
    auto c = std::make_unique<HSlider>(proc.apvts, id);
    auto* raw = c.get();
    parent.addAndMakeVisible(raw);
    owned.push_back(std::move(c));
    return raw;
}
ToggleParam* AuraEditor::addToggle(juce::Component& parent, const juce::String& id, const juce::String& text)
{
    auto t = std::make_unique<ToggleParam>(proc.apvts, id, text);
    auto* raw = t.get();
    parent.addAndMakeVisible(raw);
    owned.push_back(std::move(t));
    return raw;
}

void AuraEditor::grid(const std::vector<juce::Component*>& items, int x, int y, int perRow, int cw, int ch, int gap)
{
    int i = 0;
    for (auto* c : items)
    {
        c->setBounds(x + (i % perRow) * (cw + gap), y + (i / perRow) * ch, cw, ch);
        ++i;
    }
}

void AuraEditor::buildOscPanel(Panel& p, bool first)
{
    const juce::String o = first ? "o1" : "o2";
    auto* wave = addChoice(p, o + "Wave");
    wave->setBounds(10, 24, 140, 24);
    if (first) addToggle(p, "o1Sync", "HARD SYNC")->setBounds(160, 24, 100, 24);
    grid({ addKnob(p, o + "Level", "Level", Fmt::Percent), addKnob(p, o + "Oct", "Octave", Fmt::Int),
           addKnob(p, o + "Semi", "Semi", Fmt::Semi),      addKnob(p, o + "Fine", "Fine", Fmt::Cents),
           addKnob(p, o + "PW", "Pulse W", Fmt::Percent),  addKnob(p, o + "WT", "WT Pos", Fmt::Percent),
           addKnob(p, o + "Uni", "Unison", Fmt::Int),      addKnob(p, o + "Det", "Detune", Fmt::Cents),
           addKnob(p, o + "Spread", "Stereo", Fmt::Percent) },
         10, 56, 5, 62, 80, 2);
}

void AuraEditor::buildMiscPanel(Panel& p)
{
    addChoice(p, "subWave")->setBounds(10, 24, 120, 24);
    grid({ addKnob(p, "fmAmt", "FM Amt", Fmt::Percent), addKnob(p, "fmRatio", "FM Ratio", Fmt::Ratio),
           addKnob(p, "subLevel", "Sub Lvl", Fmt::Percent), addKnob(p, "subOct", "Sub Oct", Fmt::Int),
           addKnob(p, "nLevel", "Noise", Fmt::Percent), addKnob(p, "nTone", "N Tone", Fmt::Percent) },
         10, 56, 4, 62, 80, 2);
}

void AuraEditor::buildFilterPanel(Panel& p)
{
    addChoice(p, "fType")->setBounds(10, 24, 140, 24);
    grid({ addKnob(p, "fCutoff", "Cutoff", Fmt::Hz), addKnob(p, "fRes", "Resonance", Fmt::Percent),
           addKnob(p, "fDrive", "Drive", Fmt::Percent), addKnob(p, "fKey", "Key Track", Fmt::Percent),
           addKnob(p, "fEnv", "Env Amt", Fmt::Percent), addKnob(p, "fVowel", "Vowel", Fmt::Percent) },
         10, 50, 6, 62, 80, 0);
}

void AuraEditor::buildEnvPanel(Panel& p, bool amp)
{
    const juce::String e = amp ? "amp" : "flt";
    grid({ addKnob(p, e + "A", "Attack", Fmt::Sec), addKnob(p, e + "D", "Decay", Fmt::Sec),
           addKnob(p, e + "S", "Sustain", Fmt::Percent), addKnob(p, e + "R", "Release", Fmt::Sec) },
         10, 26, 4, 66, 84, 2);
}

void AuraEditor::buildModTab(Panel& p)
{
    static const char* shapeIds[3] = { "l1Shape", "l2Shape", "l3Shape" };
    static const char* rateIds[3]  = { "l1Rate", "l2Rate", "l3Rate" };
    static const Colour cols[3] = { Colour(0xff4fd1c5), Colour(0xfff6ad55), Colour(0xffb794f4) };
    for (int i = 0; i < 3; ++i)
    {
        const int y = 4 + i * 104;
        auto lbl = std::make_unique<juce::Label>();
        lbl->setText("LFO " + juce::String(i + 1), juce::dontSendNotification);
        lbl->setFont(mkFont(12.f, true));
        lbl->setColour(juce::Label::textColourId, cols[i]);
        lbl->setBounds(6, y + 2, 100, 18);
        p.addAndMakeVisible(lbl.get());
        owned.push_back(std::move(lbl));
        addChoice(p, shapeIds[i])->setBounds(6, y + 24, 100, 24);
        addKnob(p, rateIds[i], "Rate", Fmt::Rate)->setBounds(112, y + 2, 66, 84);
        auto sc = std::make_unique<ModScope>(cols[i]);
        sc->setBounds(190, y + 6, 240, 80);
        p.addAndMakeVisible(sc.get());
        scopes[i] = sc.get();
        owned.push_back(std::move(sc));
    }

    // matrix
    const int mx = 456;
    const char* heads[3] = { "SOURCE", "DESTINATION", "AMOUNT" };
    const int hx[3] = { mx, mx + 160, mx + 320 };
    for (int h = 0; h < 3; ++h)
    {
        auto l = std::make_unique<juce::Label>();
        l->setText(heads[h], juce::dontSendNotification);
        l->setFont(mkFont(10.f, true));
        l->setColour(juce::Label::textColourId, Colour(0xff94a3b8));
        l->setBounds(hx[h], 2, 150, 16);
        p.addAndMakeVisible(l.get());
        owned.push_back(std::move(l));
    }
    for (int r = 0; r < 6; ++r)
    {
        const int y = 22 + r * 38;
        const juce::String n(r + 1);
        addChoice(p, "m" + n + "Src")->setBounds(mx, y, 150, 26);
        addChoice(p, "m" + n + "Dst")->setBounds(mx + 160, y, 150, 26);
        addHSlider(p, "m" + n + "Amt")->setBounds(mx + 320, y, 215, 26);
    }
    auto am = std::make_unique<EnvMeter>("Amp Env", AuraLookAndFeel::accent());
    auto fm = std::make_unique<EnvMeter>("Filter Env", AuraLookAndFeel::accent2());
    am->setBounds(mx, 262, 255, 18); fm->setBounds(mx + 280, 262, 255, 18);
    p.addAndMakeVisible(am.get()); p.addAndMakeVisible(fm.get());
    ampMeter = am.get(); fltMeter = fm.get();
    owned.push_back(std::move(am)); owned.push_back(std::move(fm));
    voicesLabel.setFont(mkFont(11.f));
    voicesLabel.setColour(juce::Label::textColourId, Colour(0xff94a3b8));
    voicesLabel.setBounds(mx, 286, 400, 18);
    p.addAndMakeVisible(voicesLabel);
}

void AuraEditor::buildFxTab(Panel& tab)
{
    auto group = [&](const juce::String& title, int x, int y, int nKnobs, int extraW = 0) {
        auto g = std::make_unique<Panel>(title);
        auto* raw = g.get();
        raw->setBounds(x, y, 12 + nKnobs * 58 + extraW, 100);
        tab.addAndMakeVisible(raw);
        subPanels.push_back(std::move(g));
        return raw;
    };
    auto put = [&](Panel* g, std::vector<juce::Component*> k) { grid(k, 6, 20, 8, 58, 80, 0); (void) g; };

    int x = 0;
    auto* dist = group("DISTORTION", x, 0, 3, 36);
    put(dist, { addKnob(*dist, "dsDrive", "Drive", Fmt::Percent), addKnob(*dist, "dsMix", "Mix", Fmt::Percent) });
    {
        auto* dsType = addChoice(*dist, "dsType");
        dsType->setBounds(6 + 2 * 58 + 4, 40, 86, 24);
    }
    x += dist->getWidth() + 6;
    auto* sat = group("SATURATE", x, 0, 1, 22);
    put(sat, { addKnob(*sat, "saAmt", "Amount", Fmt::Percent) });
    x += sat->getWidth() + 6;
    auto* bc = group("BITCRUSH", x, 0, 3);
    put(bc, { addKnob(*bc, "bcBits", "Bits", Fmt::Int), addKnob(*bc, "bcRate", "Downsmp", Fmt::Int), addKnob(*bc, "bcMix", "Mix", Fmt::Percent) });
    x += bc->getWidth() + 6;
    auto* ch = group("CHORUS", x, 0, 3);
    put(ch, { addKnob(*ch, "chRate", "Rate", Fmt::Rate), addKnob(*ch, "chDepth", "Depth", Fmt::Percent), addKnob(*ch, "chMix", "Mix", Fmt::Percent) });
    x += ch->getWidth() + 6;
    auto* ph = group("PHASER", x, 0, 4);
    put(ph, { addKnob(*ph, "phRate", "Rate", Fmt::Rate), addKnob(*ph, "phDepth", "Depth", Fmt::Percent),
              addKnob(*ph, "phFb", "Feedback", Fmt::Percent), addKnob(*ph, "phMix", "Mix", Fmt::Percent) });

    x = 0;
    auto* fl = group("FLANGER", x, 106, 4);
    put(fl, { addKnob(*fl, "flRate", "Rate", Fmt::Rate), addKnob(*fl, "flDepth", "Depth", Fmt::Percent),
              addKnob(*fl, "flFb", "Feedback", Fmt::Percent), addKnob(*fl, "flMix", "Mix", Fmt::Percent) });
    x += fl->getWidth() + 6;
    auto* eq = group("EQ", x, 106, 4);
    put(eq, { addKnob(*eq, "eqLow", "Low", Fmt::Db), addKnob(*eq, "eqMid", "Mid", Fmt::Db),
              addKnob(*eq, "eqMidFreq", "Mid Freq", Fmt::Hz), addKnob(*eq, "eqHigh", "High", Fmt::Db) });
    x += eq->getWidth() + 6;
    auto* flt = group("FX FILTER", x, 106, 2);
    put(flt, { addKnob(*flt, "fxHP", "High-Pass", Fmt::Hz), addKnob(*flt, "fxLP", "Low-Pass", Fmt::Hz) });
    x += flt->getWidth() + 6;
    auto* cp = group("COMPRESSOR", x, 106, 3);
    put(cp, { addKnob(*cp, "cpThresh", "Thresh", Fmt::Db), addKnob(*cp, "cpRatio", "Ratio", Fmt::Ratio), addKnob(*cp, "cpRel", "Release", Fmt::Ms) });

    x = 0;
    auto* dl = group("DELAY", x, 212, 4, 96);
    put(dl, { addKnob(*dl, "dlTime", "Time", Fmt::Ms), addKnob(*dl, "dlFb", "Feedback", Fmt::Percent),
              addKnob(*dl, "dlTone", "Tone", Fmt::Hz), addKnob(*dl, "dlMix", "Mix", Fmt::Percent) });
    addToggle(*dl, "dlPing", "PING-PONG")->setBounds(6 + 4 * 58 + 6, 24, 84, 26);
    addChoice(*dl, "dlSync")->setBounds(6 + 4 * 58 + 6, 56, 84, 24);
    x += dl->getWidth() + 6;
    auto* rv = group("REVERB", x, 212, 4);
    put(rv, { addKnob(*rv, "rvSize", "Size", Fmt::Percent), addKnob(*rv, "rvDamp", "Damping", Fmt::Percent),
              addKnob(*rv, "rvWidth", "Width", Fmt::Percent), addKnob(*rv, "rvMix", "Mix", Fmt::Percent) });
    x += rv->getWidth() + 6;
    auto* out = group("OUTPUT", x, 212, 1);
    put(out, { addKnob(*out, "stWidth", "St. Width", Fmt::Percent) });
}

void AuraEditor::buildVoiceTab(Panel& tab)
{
    auto v = std::make_unique<Panel>("VOICE");
    v->setBounds(0, 0, 380, 134);
    tab.addAndMakeVisible(v.get());
    addChoice(*v, "vMode")->setBounds(10, 24, 120, 24);
    grid({ addKnob(*v, "vGlide", "Glide", Fmt::Sec), addKnob(*v, "vBend", "Bend Range", Fmt::Semi),
           addKnob(*v, "vVelSens", "Vel Sens", Fmt::Percent) }, 10, 50, 3, 66, 80, 2);

    auto a = std::make_unique<Panel>("ARPEGGIATOR", AuraLookAndFeel::accent2());
    a->setBounds(390, 0, 440, 134);
    tab.addAndMakeVisible(a.get());
    addToggle(*a, "arpOn", "ARP ON")->setBounds(10, 24, 80, 24);
    addChoice(*a, "arpRate")->setBounds(100, 24, 80, 24);
    addChoice(*a, "arpPat")->setBounds(190, 24, 110, 24);
    grid({ addKnob(*a, "arpOct", "Octaves", Fmt::Int), addKnob(*a, "arpGate", "Gate", Fmt::Percent) }, 10, 50, 2, 66, 80, 2);
    subPanels.push_back(std::move(v));
    subPanels.push_back(std::move(a));

    auto tip = std::make_unique<juce::Label>();
    tip->setText("Pitch bend, mod wheel (CC1) and sustain pedal (CC64) are supported. Route the mod wheel to any destination "
                 "with the Mod Matrix. The arpeggiator follows the host tempo.", juce::dontSendNotification);
    tip->setFont(mkFont(12.f));
    tip->setColour(juce::Label::textColourId, Colour(0xff94a3b8));
    tip->setJustificationType(Justification::topLeft);
    tip->setBounds(0, 146, 830, 40);
    tab.addAndMakeVisible(tip.get());
    owned.push_back(std::move(tip));
}

void AuraEditor::buildLayerTab(Panel& tab)
{
    auto lay = std::make_unique<Panel>("LAYER B  -  background layer (uses Oscillator 2)", AuraLookAndFeel::accent2());
    lay->setBounds(0, 0, 400, 134);
    tab.addAndMakeVisible(lay.get());
    addToggle(*lay, "layOn", "ENABLE LAYER")->setBounds(10, 24, 130, 24);
    grid({ addKnob(*lay, "layCut", "Layer Tone", Fmt::Hz), addKnob(*lay, "layA", "Attack", Fmt::Sec), addKnob(*lay, "layD", "Decay", Fmt::Sec),
           addKnob(*lay, "layS", "Sustain", Fmt::Percent), addKnob(*lay, "layR", "Release", Fmt::Sec) },
         10, 50, 5, 76, 80, 2);
    subPanels.push_back(std::move(lay));

    auto mac = std::make_unique<Panel>("MACROS  (same knobs as the front page)");
    mac->setBounds(410, 0, 420, 134);
    tab.addAndMakeVisible(mac.get());
    grid({ addKnob(*mac, "bright", "Brightness", Fmt::Percent), addKnob(*mac, "warmth", "Warmth", Fmt::Percent),
           addKnob(*mac, "movement", "Movement", Fmt::Percent), addKnob(*mac, "vDrift", "Analog Drift", Fmt::Percent),
           addKnob(*mac, "master", "Preset Gain", Fmt::Ratio) },
         10, 28, 5, 76, 80, 2);
    subPanels.push_back(std::move(mac));

    auto tip = std::make_unique<juce::Label>();
    tip->setText("Layer B turns Oscillator 2 into a separate background sound with its own tone filter and envelope - for example a short "
                 "pluck from Oscillator 1 with a slow pad underneath, played from one MIDI track. Preset Gain is the loudness-match that "
                 "every factory preset has; use the Volume knob for your own level.",
                 juce::dontSendNotification);
    tip->setFont(mkFont(12.f));
    tip->setColour(juce::Label::textColourId, Colour(0xff94a3b8));
    tip->setJustificationType(Justification::topLeft);
    tip->setBounds(0, 146, 830, 52);
    tab.addAndMakeVisible(tip.get());
    owned.push_back(std::move(tip));
}

static juce::Colour genreColour(int g)
{
    static const Colour c[kNumGenres] = { Colour(0xfff6ad55), Colour(0xff4fd1c5), Colour(0xfff687b3),
                                          Colour(0xfff2d06b), Colour(0xffb794f4), Colour(0xff63b3ed) };
    return c[juce::jlimit(0, kNumGenres - 1, g)];
}

void AuraEditor::buildHeader()
{
    headerPanel.setBounds(0, 0, kDesignW, 66);
    content.addAndMakeVisible(headerPanel);

    titleLabel.setText("AuraSynth", juce::dontSendNotification);
    titleLabel.setFont(mkFont(26.f, true));
    titleLabel.setColour(juce::Label::textColourId, AuraLookAndFeel::accent());
    titleLabel.setBounds(16, 6, 200, 34);
    content.addAndMakeVisible(titleLabel);
    auto sub = std::make_unique<juce::Label>();
    sub->setText("pick a sound - play", juce::dontSendNotification);
    sub->setFont(mkFont(11.f));
    sub->setColour(juce::Label::textColourId, Colour(0xff64748b));
    sub->setBounds(18, 38, 200, 16);
    content.addAndMakeVisible(sub.get());
    owned.push_back(std::move(sub));

    simpleBtn.setBounds(206, 16, 92, 32);
    advBtn.setBounds(302, 16, 100, 32);
    for (auto* b : { &simpleBtn, &advBtn })
    {
        b->setColour(juce::TextButton::buttonOnColourId, AuraLookAndFeel::accent());
        b->setColour(juce::TextButton::textColourOnId, Colour(0xff0b1015));
        content.addAndMakeVisible(b);
    }
    simpleBtn.onClick = [this] { showPage(false); };
    advBtn.onClick = [this] { showPage(true); };

    prevBtn.setBounds(430, 16, 36, 32);
    nameLabel.setFont(mkFont(22.f, true));
    nameLabel.setJustificationType(Justification::centred);
    nameLabel.setBounds(470, 6, 330, 32);
    catLabel.setFont(mkFont(12.f));
    catLabel.setColour(juce::Label::textColourId, Colour(0xff94a3b8));
    catLabel.setJustificationType(Justification::centred);
    catLabel.setBounds(470, 38, 330, 18);
    nextBtn.setBounds(804, 16, 36, 32);
    favBtn.setBounds(860, 16, 70, 32);
    saveBtn.setBounds(938, 16, 70, 32);
    loadBtn.setBounds(1016, 16, 70, 32);
    for (auto* c : std::initializer_list<juce::Component*>{ &prevBtn, &nextBtn, &nameLabel, &catLabel, &favBtn, &saveBtn, &loadBtn })
        content.addAndMakeVisible(c);

    prevBtn.onClick = [this] { proc.presets.stepWithin(shown, -1); };
    nextBtn.onClick = [this] { proc.presets.stepWithin(shown, +1); };
    favBtn.onClick  = [this] { proc.presets.toggleFavorite(proc.presets.currentIndex()); };
    saveBtn.onClick = [this] { showSaveDialog(); };
    loadBtn.onClick = [this] { showLoadDialog(); };
}

void AuraEditor::buildBrowser()
{
    // advanced-page browser frame (the list itself is shared with the front page and re-positioned by layoutBrowser)
    browserPanel.setBounds(10, 72, 260, 716);
    advPage.addAndMakeVisible(browserPanel);

    search.setTextToShowWhenEmpty("Search sounds...", Colour(0xff64748b));
    search.setFont(mkFont(15.f));
    search.onTextChange = [this] { refreshList(); };
    content.addAndMakeVisible(search);

    catBox.addItem("All categories", 1);
    catBox.addItem("Favorites", 2);
    catBox.addItem("User presets", 3);
    const auto& cats = categoryNames();
    for (int i = 0; i < cats.size(); ++i) catBox.addItem(cats[i], 4 + i);
    catBox.setSelectedId(1, juce::dontSendNotification);
    catBox.onChange = [this] {
        const int id = catBox.getSelectedId();
        categoryFilter = id == 1 ? "" : id == 2 ? "Favorites" : id == 3 ? "User" : categoryNames()[id - 4];
        refreshList();
    };
    content.addAndMakeVisible(catBox);

    content.addAndMakeVisible(list);

    countLabel.setFont(mkFont(11.f));
    countLabel.setColour(juce::Label::textColourId, Colour(0xff64748b));
    content.addAndMakeVisible(countLabel);
}

void AuraEditor::layoutBrowser()
{
    if (advanced)
    {
        search.setBounds(18, 96, 244, 28);
        catBox.setBounds(18, 130, 244, 26);
        list.setRowHeight(26);
        list.setBounds(18, 162, 244, 590);
        countLabel.setBounds(18, 758, 244, 22);
        countLabel.setJustificationType(Justification::centredLeft);
    }
    else
    {
        search.setBounds(10, 196, 500, 34);
        list.setRowHeight(38);
        list.setBounds(10, 238, 630, 546);
        countLabel.setBounds(516, 196, 124, 34);
        countLabel.setJustificationType(Justification::centredRight);
    }
    catBox.setVisible(advanced);
}

void AuraEditor::buildSimplePage()
{
    simplePage.setBounds(0, 0, kDesignW, kDesignH);
    simplePage.setInterceptsMouseClicks(false, true);
    content.addAndMakeVisible(simplePage);

    // --- genre buttons ---
    const auto& gn = genreNames();
    const int gw = (1260 - 5 * 10) / 6;
    for (int g = 0; g < kNumGenres; ++g)
    {
        auto& b = genreBtns[(size_t) g];
        b.setButtonText(gn[g]);
        b.setBounds(10 + g * (gw + 10), 76, gw, 56);
        b.setColour(juce::TextButton::buttonColourId, Colour(0xff222737));
        b.setColour(juce::TextButton::buttonOnColourId, genreColour(g));
        b.setColour(juce::TextButton::textColourOffId, genreColour(g).brighter(0.3f));
        b.setColour(juce::TextButton::textColourOnId, Colour(0xff0b1015));
        b.onClick = [this, g] { selectGenre(genreSel == g ? -1 : g); };
        simplePage.addAndMakeVisible(b);
    }

    // --- sound-type buttons ---
    const auto& grp = groupNames();
    juce::StringArray labels;
    labels.add("ALL");
    for (int i = 0; i < kNumGroups; ++i) labels.add(grp[i]);
    labels.add(juce::String::fromUTF8("\xE2\x98\x85 Favorites"));
    labels.add("My sounds");
    const int tw = (1260 - (kNumTypeButtons - 1) * 8) / kNumTypeButtons;
    for (int t = 0; t < kNumTypeButtons; ++t)
    {
        auto& b = typeBtns[(size_t) t];
        b.setButtonText(labels[t]);
        b.setBounds(10 + t * (tw + 8), 142, tw, 40);
        b.setColour(juce::TextButton::buttonColourId, Colour(0xff222737));
        b.setColour(juce::TextButton::buttonOnColourId, Colour(0xffe2e8f0));
        b.setColour(juce::TextButton::textColourOffId, Colour(0xffcbd5e1));
        b.setColour(juce::TextButton::textColourOnId, Colour(0xff0b1015));
        b.onClick = [this, t] { selectType(t); };
        simplePage.addAndMakeVisible(b);
    }

    // --- now playing + the six simple controls ---
    nowPanel.setBounds(660, 196, 610, 588);
    simplePage.addAndMakeVisible(nowPanel);

    bigName.setFont(mkFont(32.f, true));
    bigName.setBounds(680, 218, 570, 42);
    bigTags.setFont(mkFont(13.f));
    bigTags.setColour(juce::Label::textColourId, Colour(0xff94a3b8));
    bigTags.setBounds(680, 260, 570, 22);
    resetBtn.setBounds(680, 294, 300, 42);
    resetBtn.setColour(juce::TextButton::buttonColourId, AuraLookAndFeel::accent2().darker(0.55f));
    resetBtn.setColour(juce::TextButton::textColourOffId, Colour(0xfffff4e0));
    resetBtn.onClick = [this] { proc.presets.resetToPreset(); };
    editedLabel.setFont(mkFont(13.f, true));
    editedLabel.setBounds(992, 294, 270, 42);
    editedLabel.setJustificationType(Justification::centredLeft);
    for (auto* c : std::initializer_list<juce::Component*>{ &bigName, &bigTags, &resetBtn, &editedLabel })
        simplePage.addAndMakeVisible(c);

    struct K { const char* id; const char* cap; const char* hint; };
    static const K ks[6] = {
        { "volume",   "VOLUME",     "your level - never changes with presets" },
        { "rvMix",    "REVERB",     "room and space" },
        { "dlMix",    "DELAY",      "echoes in time with your tempo" },
        { "bright",   "BRIGHTNESS", "darker  <->  brighter" },
        { "warmth",   "WARMTH",     "thin  <->  full and analog" },
        { "movement", "MOVEMENT",   "still  <->  slowly evolving" } };
    for (int i = 0; i < 6; ++i)
    {
        auto k = std::make_unique<Knob>(proc.apvts, ks[i].id, ks[i].cap, Fmt::Percent);
        k->setBig();
        k->setBounds(680 + (i % 3) * 196, 354 + (i / 3) * 214, 186, 176);
        simplePage.addAndMakeVisible(k.get());
        auto h = std::make_unique<juce::Label>();
        h->setText(ks[i].hint, juce::dontSendNotification);
        h->setFont(mkFont(11.f));
        h->setColour(juce::Label::textColourId, Colour(0xff64748b));
        h->setJustificationType(Justification::centred);
        h->setBounds(680 + (i % 3) * 196 - 4, 354 + (i / 3) * 214 + 178, 194, 18);
        simplePage.addAndMakeVisible(h.get());
        simpleKnobs.push_back(std::move(k));
        simpleHints.push_back(std::move(h));
    }
}

AuraEditor::AuraEditor(AuraProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
      keyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&lnf);
    content.setBounds(0, 0, kDesignW, kDesignH);
    addAndMakeVisible(content);
    advPage.setBounds(0, 0, kDesignW, kDesignH);
    advPage.setInterceptsMouseClicks(false, true);
    content.addAndMakeVisible(advPage);

    buildHeader();
    buildBrowser();
    buildSimplePage();

    osc1.setBounds(280, 72, 330, 222);  osc2.setBounds(620, 72, 330, 222);  misc.setBounds(960, 72, 310, 222);
    filter.setBounds(280, 300, 390, 134); fenv.setBounds(680, 300, 290, 134); aenv.setBounds(980, 300, 290, 134);
    for (auto* pnl : { &osc1, &osc2, &misc, &filter, &fenv, &aenv }) advPage.addAndMakeVisible(pnl);
    buildOscPanel(osc1, true);
    buildOscPanel(osc2, false);
    buildMiscPanel(misc);
    buildFilterPanel(filter);
    buildEnvPanel(fenv, false);
    buildEnvPanel(aenv, true);

    modTab.setBounds(0, 0, 990, 316); fxTab.setBounds(0, 0, 990, 316); voiceTab.setBounds(0, 0, 990, 316); layerTab.setBounds(0, 0, 990, 316);
    buildModTab(modTab);
    buildFxTab(fxTab);
    buildVoiceTab(voiceTab);
    buildLayerTab(layerTab);
    tabs.setTabBarDepth(28);
    tabs.addTab("Modulation", Colour(0xff12141a), &modTab, false);
    tabs.addTab("Effects", Colour(0xff12141a), &fxTab, false);
    tabs.addTab("Voice & Arp", Colour(0xff12141a), &voiceTab, false);
    tabs.addTab("Layer & Macros", Colour(0xff12141a), &layerTab, false);
    tabs.setOutline(0);
    tabs.setBounds(280, 440, 990, 350);
    advPage.addAndMakeVisible(tabs);

    advVolume = std::make_unique<Knob>(proc.apvts, "volume", "Volume", Fmt::Percent);
    advVolume->setBounds(1176, 3, 84, 63);
    content.addChildComponent(*advVolume);     // sits in the header, so it lives outside advPage (shown only on the advanced page)

    keyboard.setBounds(10, 796, 1260, 58);
    keyboard.setAvailableRange(24, 108);
    keyboard.setLowestVisibleKey(36);
    keyboard.setKeyWidth(26.f);
    keyboard.setScrollButtonsVisible(false);
    keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, AuraLookAndFeel::accent().withAlpha(0.7f));
    content.addAndMakeVisible(keyboard);

    proc.presets.addChangeListener(this);
    showPage(false);
    selectType(0);
    updateHeader();
    updateNowPlaying();

    setResizable(true, true);
    getConstrainer()->setFixedAspectRatio((double) kDesignW / (double) kDesignH);
    setResizeLimits(640, (int) (640.0 * kDesignH / kDesignW), 2560, (int) (2560.0 * kDesignH / kDesignW));
    setSize(1120, (int) (1120.0 * kDesignH / kDesignW));
    startTimerHz(30);
}

AuraEditor::~AuraEditor()
{
    stopTimer();
    proc.presets.removeChangeListener(this);
    setLookAndFeel(nullptr);
}

void AuraEditor::paint(Graphics& g) { g.fillAll(Colour(0xff12141a)); }

void AuraEditor::resized()
{
    const float s = juce::jmin((float) getWidth() / (float) kDesignW, (float) getHeight() / (float) kDesignH);
    content.setTransform(juce::AffineTransform::scale(s));
}

void AuraEditor::showPage(bool advancedPage)
{
    advanced = advancedPage;
    simplePage.setVisible(!advanced);
    advPage.setVisible(advanced);
    simpleBtn.setToggleState(!advanced, juce::dontSendNotification);
    advBtn.setToggleState(advanced, juce::dontSendNotification);
    if (advVolume) { advVolume->setVisible(advanced); advVolume->toFront(false); }
    layoutBrowser();
    refreshList();
}

void AuraEditor::selectGenre(int g)
{
    genreSel = g;
    typeSel = 0;                                   // a new genre starts from "all sound types"
    if (g >= 0)
    {
        const int idx = proc.presets.indexOfName(genreStarterPreset(g));
        if (idx >= 0) proc.presets.load(idx);      // genre mode: load a suitable starting sound
    }
    updateButtons();
    refreshList();
}

void AuraEditor::selectType(int t)
{
    typeSel = juce::jlimit(0, kNumTypeButtons - 1, t);
    updateButtons();
    refreshList();
}

void AuraEditor::updateButtons()
{
    for (int g = 0; g < kNumGenres; ++g) genreBtns[(size_t) g].setToggleState(genreSel == g, juce::dontSendNotification);
    for (int t = 0; t < kNumTypeButtons; ++t) typeBtns[(size_t) t].setToggleState(typeSel == t, juce::dontSendNotification);
}

void AuraEditor::updateNowPlaying()
{
    const auto& pm = proc.presets;
    bigName.setText(pm.currentName(), juce::dontSendNotification);
    juce::String tags;
    if (pm.currentIndex() >= 0)
    {
        const auto& pr = pm.preset(pm.currentIndex());
        tags = groupNames()[juce::jlimit(0, kNumGroups - 1, pr.group)];
        if ((pr.genres & kAllGenres) != kAllGenres)
            for (int g = 0; g < kNumGenres; ++g)
                if (pr.genres & (1u << g)) tags += juce::String::fromUTF8("   \xC2\xB7   ") + genreNames()[g];
        if (!pr.factory) tags += juce::String::fromUTF8("   \xC2\xB7   my sound");
    }
    bigTags.setText(tags, juce::dontSendNotification);

    const bool mod = pm.isModified();
    editedLabel.setText(mod ? juce::String::fromUTF8("\xE2\x97\x8F  changed - click to go back") : juce::String::fromUTF8("\xE2\x9C\x93  original settings"),
                        juce::dontSendNotification);
    editedLabel.setColour(juce::Label::textColourId, mod ? AuraLookAndFeel::accent2() : Colour(0xff64748b));
}

void AuraEditor::timerCallback()
{
    for (int i = 0; i < 3; ++i)
        if (scopes[i]) scopes[i]->push(proc.engine.vizLfo[i].load(std::memory_order_relaxed));
    if (ampMeter) ampMeter->set(proc.engine.vizAmpEnv.load(std::memory_order_relaxed));
    if (fltMeter) fltMeter->set(proc.engine.vizFiltEnv.load(std::memory_order_relaxed));
    const int v = proc.engine.vizVoices.load(std::memory_order_relaxed);
    const auto txt = "Active voices: " + juce::String(v);
    if (voicesLabel.getText() != txt) voicesLabel.setText(txt, juce::dontSendNotification);
    if ((tick++ % 6) == 0) updateNowPlaying();      // "changed / original" indicator, ~5 Hz
}

// ------------------------------------------------------------------ preset browser
void AuraEditor::refreshList()
{
    juce::String cat;
    int genre = -1, group = -1;
    if (advanced) cat = categoryFilter;
    else
    {
        genre = genreSel;
        if (typeSel >= 1 && typeSel <= kNumGroups) group = typeSel - 1;
        else if (typeSel == kNumGroups + 1) cat = "Favorites";
        else if (typeSel == kNumGroups + 2) cat = "User";
    }
    shown = proc.presets.filter(search.getText(), cat, genre, group);
    updatingList = true;
    list.updateContent();
    list.deselectAllRows();
    const int cur = proc.presets.currentIndex();
    for (size_t i = 0; i < shown.size(); ++i)
        if (shown[i] == cur) { list.selectRow((int) i, true, true); list.scrollToEnsureRowIsOnscreen((int) i); break; }
    list.repaint();
    updatingList = false;
    countLabel.setText(advanced ? juce::String((int) shown.size()) + " presets"
                                  + (proc.presets.numFavorites() > 0 ? "   |   " + juce::String(proc.presets.numFavorites()) + " favorites" : juce::String())
                                : juce::String((int) shown.size()) + " sounds",
                       juce::dontSendNotification);
}

void AuraEditor::updateHeader()
{
    nameLabel.setText(proc.presets.currentName(), juce::dontSendNotification);
    catLabel.setText(proc.presets.currentCategory().isEmpty() ? "No category" : proc.presets.currentCategory(), juce::dontSendNotification);
    const bool fav = proc.presets.isFavorite(proc.presets.currentIndex());
    favBtn.setButtonText(fav ? juce::String::fromUTF8("\xE2\x98\x85 Fav") : juce::String::fromUTF8("\xE2\x98\x86 Fav"));
    favBtn.setColour(juce::TextButton::buttonColourId, fav ? AuraLookAndFeel::accent2().darker(0.4f) : Colour(0xff2a3040));
}

void AuraEditor::paintListBoxItem(int row, Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) shown.size()) return;
    const int idx = shown[(size_t) row];
    const auto& pr = proc.presets.preset(idx);
    if (selected) { g.setColour(AuraLookAndFeel::accent().withAlpha(0.25f)); g.fillRoundedRectangle(2.f, 1.f, (float) w - 4.f, (float) h - 2.f, 4.f); }
    const bool fav = proc.presets.isFavorite(idx);
    const bool big = !advanced;
    g.setColour(fav ? AuraLookAndFeel::accent2() : Colour(0xff4b5468));
    g.setFont(mkFont(big ? 18.f : 14.f));
    g.drawText(fav ? juce::String::fromUTF8("\xE2\x98\x85") : juce::String::fromUTF8("\xE2\x98\x86"), 4, 0, big ? 30 : 20, h, Justification::centred);
    const int x0 = big ? 38 : 28;
    int right = 4;
    if (big)
    {
        g.setColour(Colour(0xff64748b));
        g.setFont(mkFont(12.f));
        g.drawText(groupNames()[juce::jlimit(0, kNumGroups - 1, pr.group)] + (pr.factory ? "" : "  (mine)"), w - 150, 0, 144, h, Justification::centredRight);
        right = 154;
    }
    g.setColour(selected ? Colour(0xffffffff) : Colour(0xffe2e8f0));
    g.setFont(mkFont(big ? 17.f : 13.f, big && selected));
    g.drawText(pr.name, x0, 0, w - x0 - right, h, Justification::centredLeft, true);
}

void AuraEditor::activateRow(int row)
{
    if (row >= 0 && row < (int) shown.size()) proc.presets.load(shown[(size_t) row]);
}

void AuraEditor::listBoxItemClicked(int row, const juce::MouseEvent& e)
{
    if (row < 0 || row >= (int) shown.size()) return;
    // the content is scaled, so e.x is already in the list's own coordinate space
    if (e.x < (advanced ? 26 : 36)) proc.presets.toggleFavorite(shown[(size_t) row]);
    else activateRow(row);
}

void AuraEditor::showSaveDialog()
{
    auto* w = new juce::AlertWindow("Save my sound", "Give your sound a name. It keeps every setting and shows up under \"My sounds\".", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor("name", proc.presets.currentName() + " (mine)", "Name");
    auto cats = categoryNames();
    cats.add("User");
    w->addComboBox("cat", cats, "Category");
    if (auto* cb = w->getComboBoxComponent("cat"))
    {
        const int idx = cats.indexOf(proc.presets.currentCategory());
        cb->setSelectedItemIndex(idx >= 0 ? idx : cats.size() - 1);
    }
    w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->setLookAndFeel(&lnf);
    w->enterModalState(true, juce::ModalCallbackFunction::create([this, w, safe = juce::Component::SafePointer<AuraEditor>(this)](int r) {
        if (r == 1 && safe != nullptr)
        {
            const auto name = w->getTextEditorContents("name");
            const auto cat = w->getComboBoxComponent("cat")->getText();
            proc.presets.saveUserPreset(name, cat);
        }
        w->setLookAndFeel(nullptr);
    }), true);
}

void AuraEditor::showLoadDialog()
{
    chooser = std::make_unique<juce::FileChooser>("Load an AuraSynth preset", proc.presets.userPresetDirectory(), "*.aurapreset");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [this, safe = juce::Component::SafePointer<AuraEditor>(this)](const juce::FileChooser& fc) {
                             if (safe == nullptr) return;
                             const auto f = fc.getResult();
                             if (f.existsAsFile()) proc.presets.loadPresetFile(f);
                         });
}
} // namespace aura
