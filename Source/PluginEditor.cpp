#include "PluginEditor.h"
using namespace juce;

// ============================================================================
// Colour palette
const Colour LowEnhancerLookAndFeel::accent { 0xFFFF6D00 };  // neon orange
const Colour LowEnhancerLookAndFeel::dim    { 0xFF2A1500 };
const Colour LowEnhancerLookAndFeel::bg     { 0xFF0F0700 };

// ============================================================================
// Look and Feel
// ============================================================================
LowEnhancerLookAndFeel::LowEnhancerLookAndFeel()
{
    setColour (Slider::rotarySliderFillColourId,    accent);
    setColour (Slider::rotarySliderOutlineColourId, dim);
    setColour (Slider::thumbColourId,               accent);
    setColour (Slider::textBoxTextColourId,         accent.withAlpha (0.75f));
    setColour (Slider::textBoxBackgroundColourId,   Colour (0x00000000));
    setColour (Slider::textBoxOutlineColourId,      Colour (0x00000000));
    setColour (Label::textColourId,                 accent.withAlpha (0.85f));
    setColour (TextButton::buttonColourId,          dim);
    setColour (TextButton::buttonOnColourId,        accent.withAlpha (0.25f));
    setColour (TextButton::textColourOffId,         accent.withAlpha (0.5f));
    setColour (TextButton::textColourOnId,          accent);
    setColour (ToggleButton::textColourId,          accent.withAlpha (0.85f));
    setColour (ToggleButton::tickColourId,          accent);
    setColour (ToggleButton::tickDisabledColourId,  dim);
}

void LowEnhancerLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h,
                                                float sliderPos, float startAngle,
                                                float endAngle, Slider&)
{
    const float radius = jmin (w, h) * 0.44f;
    const float cx = x + w * 0.5f, cy = y + h * 0.5f;
    const float rx = cx - radius, ry = cy - radius, rw = radius * 2.0f;
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    // Track
    { Path p; p.addArc (rx, ry, rw, rw, startAngle, endAngle, true);
      g.setColour (dim); g.strokePath (p, PathStrokeType (3.5f)); }

    // Filled arc
    { Path p; p.addArc (rx, ry, rw, rw, startAngle, angle, true);
      g.setColour (accent.withAlpha (0.85f)); g.strokePath (p, PathStrokeType (3.5f)); }

    // Outer glow
    g.setColour (accent.withAlpha (0.1f));
    g.drawEllipse (rx - 2, ry - 2, rw + 4, rw + 4, 1.0f);

    // Pointer
    const float lx = cx + std::sin (angle) * radius * 0.55f;
    const float ly = cy - std::cos (angle) * radius * 0.55f;
    g.setColour (accent); g.drawLine (cx, cy, lx, ly, 2.0f);

    // Centre dot
    const float dr = radius * 0.12f;
    g.fillEllipse (cx - dr, cy - dr, dr * 2, dr * 2);
}

void LowEnhancerLookAndFeel::drawButtonBackground (Graphics& g, Button& btn,
                                                    const Colour&,
                                                    bool highlighted, bool /*down*/)
{
    auto b = btn.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = btn.getToggleState();
    g.setColour (on ? accent.withAlpha (0.2f) : dim.withAlpha (0.6f));
    g.fillRoundedRectangle (b, 3.0f);
    g.setColour (on ? accent : accent.withAlpha (0.35f));
    g.drawRoundedRectangle (b, 3.0f, on ? 1.5f : 1.0f);
    if (highlighted && !on)
    {
        g.setColour (accent.withAlpha (0.08f));
        g.fillRoundedRectangle (b, 3.0f);
    }
}

void LowEnhancerLookAndFeel::drawButtonText (Graphics& g, TextButton& btn,
                                              bool /*highlighted*/, bool /*down*/)
{
    const bool on = btn.getToggleState();
    g.setColour (on ? accent : accent.withAlpha (0.5f));
    g.setFont (Font (FontOptions (10.0f).withStyle ("Bold")));
    g.drawText (btn.getButtonText(), btn.getLocalBounds(), Justification::centred, false);
}

Font LowEnhancerLookAndFeel::getLabelFont (Label&)
{
    return Font (FontOptions (11.0f).withStyle ("Bold"));
}

// ============================================================================
// VU Meter
// ============================================================================
LowVUMeter::LowVUMeter (LowEnhancerProcessor& p, bool isOutput)
    : processor (p), output (isOutput)
{
    meterColour = isOutput ? LowEnhancerLookAndFeel::accent
                           : LowEnhancerLookAndFeel::accent.withAlpha (0.6f);
    startTimerHz (30);
}

void LowVUMeter::paint (Graphics& g)
{
    const auto accent = LowEnhancerLookAndFeel::accent;
    auto b = getLocalBounds().toFloat().reduced (2);
    g.setColour (Colour (0xFF1A0800)); g.fillRoundedRectangle (b, 3);
    g.setColour (LowEnhancerLookAndFeel::dim); g.drawRoundedRectangle (b, 3, 1);
    const float level = jlimit (0.0f, 1.0f, displayLevel * 3.0f);
    const float barH  = b.getHeight() * level;
    if (barH > 0)
    {
        ColourGradient grad (meterColour, b.getX(), b.getBottom() - barH,
                             meterColour.withAlpha (0.25f), b.getX(), b.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (b.getX(), b.getBottom() - barH, b.getWidth(), barH, 2);
    }
    g.setColour (accent.withAlpha (0.55f));
    g.setFont (Font (FontOptions (9.0f).withStyle ("Bold")));
    g.drawText (output ? "OUT" : "IN", getLocalBounds(), Justification::centredTop, false);
}

void LowVUMeter::timerCallback()
{
    const float target = output ? processor.getOutputRMS() : processor.getInputRMS();
    displayLevel = target > displayLevel ? target : displayLevel * 0.92f;
    repaint();
}

// ============================================================================
// Labelled Knob
// ============================================================================
LowLabelledKnob::LowLabelledKnob (const String& labelText, LowEnhancerProcessor& p,
                                    const String& paramID, LookAndFeel& laf)
{
    slider.setSliderStyle  (Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 60, 16);
    slider.setLookAndFeel (&laf);
    addAndMakeVisible (slider);

    label.setText (labelText, dontSendNotification);
    label.setJustificationType (Justification::centred);
    label.setColour (Label::textColourId, LowEnhancerLookAndFeel::accent.withAlpha (0.75f));
    label.setFont   (Font (FontOptions (10.0f).withStyle ("Bold")));
    addAndMakeVisible (label);

    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (
        p.apvts, paramID, slider);
}

void LowLabelledKnob::resized()
{
    auto b = getLocalBounds();
    label.setBounds  (b.removeFromBottom (18));
    slider.setBounds (b);
}

// ============================================================================
// Two-state toggle (e.g. TUBE / TIGHT)
// ============================================================================
LowToggleButton::LowToggleButton (const String& labelA, const String& labelB,
                                   LowEnhancerProcessor& p, const String& pid,
                                   LookAndFeel& laf)
    : processor (p), paramID (pid)
{
    btnA.setButtonText (labelA);
    btnB.setButtonText (labelB);
    btnA.setClickingTogglesState (true);
    btnB.setClickingTogglesState (true);
    btnA.setRadioGroupId (42);
    btnB.setRadioGroupId (42);
    btnA.setLookAndFeel (&laf);
    btnB.setLookAndFeel (&laf);

    // btnA = off (false), btnB = on (true)
    const bool currentVal = *p.apvts.getRawParameterValue (pid) > 0.5f;
    btnA.setToggleState (!currentVal, dontSendNotification);
    btnB.setToggleState  (currentVal, dontSendNotification);

    // Wire btnB to the parameter (false=TUBE/off, true=TIGHT/on)
    attachB = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, pid, btnB);

    // When btnA is clicked, flip btnB (which drives the parameter)
    btnA.onClick = [this]()
    {
        if (btnA.getToggleState())
            if (auto* param = processor.apvts.getParameter (paramID))
                param->setValueNotifyingHost (0.0f);
    };

    addAndMakeVisible (btnA);
    addAndMakeVisible (btnB);
}

void LowToggleButton::resized()
{
    auto b = getLocalBounds();
    const int half = b.getWidth() / 2;
    btnA.setBounds (b.removeFromLeft (half).reduced (2));
    btnB.setBounds (b.reduced (2));
}

// ============================================================================
// On/Off toggle with LED feel
// ============================================================================
LowOnOffButton::LowOnOffButton (const String& lbl, LowEnhancerProcessor& p,
                                  const String& pid, LookAndFeel& laf)
    : processor (p)
{
    button.setButtonText (lbl);
    button.setLookAndFeel (&laf);
    addAndMakeVisible (button);
    attachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, pid, button);
}

void LowOnOffButton::resized()
{
    button.setBounds (getLocalBounds().reduced (2));
}

// ============================================================================
// Editor
// ============================================================================
LowEnhancerEditor::LowEnhancerEditor (LowEnhancerProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p),
      knobWarmth ("WARMTH", p, "warmth", laf),
      knobHpf    ("HPF Hz", p, "hpf",   laf),
      knobSub    ("SUB",    p, "sub",   laf),
      knobDrive  ("DRIVE",  p, "drive", laf),
      knobPunch  ("PUNCH",  p, "punch", laf),
      knobMix    ("MIX",    p, "mix",   laf),
      satModeToggle ("TUBE", "TIGHT", p, "satmode",  laf),
      monoButton    ("MONO BASS",    p, "monobass", laf),
      vuIn (p, false), vuOut (p, true)
{
    setLookAndFeel (&laf);
    // Wider to fit 6 knobs + VU
    setSize (582, 270);

    titleLabel.setText ("LOW ENHANCER", dontSendNotification);
    titleLabel.setFont (Font (FontOptions (18.0f).withStyle ("Bold")));
    titleLabel.setColour (Label::textColourId, LowEnhancerLookAndFeel::accent);
    titleLabel.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText (
        "HPF  //  LOW SHELF  //  SUB HARMONIC  //  PUNCH  //  MONO BASS",
        dontSendNotification);
    subtitleLabel.setFont (Font (FontOptions (9.0f)));
    subtitleLabel.setColour (Label::textColourId,
                              LowEnhancerLookAndFeel::accent.withAlpha (0.4f));
    subtitleLabel.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (subtitleLabel);

    addAndMakeVisible (knobWarmth);
    addAndMakeVisible (knobHpf);
    addAndMakeVisible (knobSub);
    addAndMakeVisible (knobDrive);
    addAndMakeVisible (knobPunch);
    addAndMakeVisible (knobMix);
    addAndMakeVisible (satModeToggle);
    addAndMakeVisible (monoButton);
    addAndMakeVisible (vuIn);
    addAndMakeVisible (vuOut);
}

LowEnhancerEditor::~LowEnhancerEditor() { setLookAndFeel (nullptr); }

void LowEnhancerEditor::paint (Graphics& g)
{
    const auto accent = LowEnhancerLookAndFeel::accent;

    g.fillAll (LowEnhancerLookAndFeel::bg);

    // Subtle warm grid
    g.setColour (accent.withAlpha (0.035f));
    for (int y = 0; y < getHeight(); y += 20) g.drawHorizontalLine (y, 0, (float)getWidth());
    for (int x = 0; x < getWidth();  x += 20) g.drawVerticalLine   (x, 0, (float)getHeight());

    // Header bar
    g.setColour (Colour (0xFF1A0800));
    g.fillRect (0, 0, getWidth(), 48);

    // Header underline
    g.setColour (accent.withAlpha (0.3f));
    g.drawLine (0.0f, 48.0f, (float)getWidth(), 48.0f, 1.0f);

    // Bottom strip background
    g.setColour (Colour (0xFF140900));
    g.fillRect (0, getHeight() - 44, getWidth(), 44);

    // Bottom strip top line
    g.setColour (accent.withAlpha (0.15f));
    g.drawLine (0.0f, (float)(getHeight() - 44), (float)getWidth(), (float)(getHeight() - 44), 1.0f);

    // VU divider
    g.setColour (accent.withAlpha (0.1f));
    g.drawLine ((float)(getWidth() - 72), 56, (float)(getWidth() - 72), (float)(getHeight() - 44), 1.0f);

    // Version
    g.setColour (accent.withAlpha (0.2f));
    g.setFont (Font (FontOptions (8.0f)));
    g.drawText ("v2.0  //  JUCE 8",
                getLocalBounds().removeFromBottom (12),
                Justification::centredRight, false);
}

void LowEnhancerEditor::resized()
{
    const int W = getWidth(), H = getHeight();

    titleLabel.setBounds    (12, 6,  W - 24, 22);
    subtitleLabel.setBounds (12, 28, W - 24, 14);

    // 6 knobs filling the main area, leaving 72px for VU on the right
    const int knobAreaW = W - 76;
    const int knobW     = knobAreaW / 6;
    const int knobY     = 52;
    const int knobH     = H - 52 - 46;   // header + bottom strip

    knobWarmth.setBounds (0,          knobY, knobW, knobH);
    knobHpf.setBounds    (knobW,      knobY, knobW, knobH);
    knobSub.setBounds    (knobW * 2,  knobY, knobW, knobH);
    knobDrive.setBounds  (knobW * 3,  knobY, knobW, knobH);
    knobPunch.setBounds  (knobW * 4,  knobY, knobW, knobH);
    knobMix.setBounds    (knobW * 5,  knobY, knobW, knobH);

    // VU meters
    const int vuW = 28, vuH = knobH - 8, vuY = knobY + 4;
    vuIn.setBounds  (W - 68, vuY, vuW, vuH);
    vuOut.setBounds (W - 36, vuY, vuW, vuH);

    // Bottom strip — SAT MODE toggle + MONO BASS button
    const int stripY = H - 42;
    const int stripH = 38;
    satModeToggle.setBounds (8,          stripY, 110, stripH);
    monoButton.setBounds    (128,        stripY, 120, stripH);
}

// ============================================================================
juce::AudioProcessorEditor* LowEnhancerProcessor::createEditor()
{
    return new LowEnhancerEditor (*this);
}
