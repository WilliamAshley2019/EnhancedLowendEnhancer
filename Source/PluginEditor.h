#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// ============================================================================
class LowEnhancerLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LowEnhancerLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool highlighted, bool down) override;
    juce::Font getLabelFont (juce::Label&) override;

    static const juce::Colour accent;
    static const juce::Colour dim;
    static const juce::Colour bg;
};

// ============================================================================
class LowVUMeter : public juce::Component, private juce::Timer
{
public:
    LowVUMeter (LowEnhancerProcessor& p, bool isOutput);
    void paint (juce::Graphics&) override;
    void timerCallback() override;
private:
    LowEnhancerProcessor& processor;
    bool  output;
    float displayLevel = 0.0f;
    juce::Colour meterColour;
};

// ============================================================================
class LowLabelledKnob : public juce::Component
{
public:
    LowLabelledKnob (const juce::String& labelText, LowEnhancerProcessor& p,
                     const juce::String& paramID, juce::LookAndFeel& laf);
    void resized() override;
    juce::Slider slider;
private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// ============================================================================
// Two-state toggle button (e.g. TUBE / TIGHT, MONO ON/OFF)
class LowToggleButton : public juce::Component
{
public:
    LowToggleButton (const juce::String& labelA, const juce::String& labelB,
                     LowEnhancerProcessor& p, const juce::String& paramID,
                     juce::LookAndFeel& laf);
    void resized() override;
private:
    juce::TextButton btnA, btnB;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachA, attachB;
    juce::Label headerLabel;
    LowEnhancerProcessor& processor;
    const juce::String paramID;
};

// ============================================================================
// Simple on/off illuminated toggle
class LowOnOffButton : public juce::Component
{
public:
    LowOnOffButton (const juce::String& label, LowEnhancerProcessor& p,
                    const juce::String& paramID, juce::LookAndFeel& laf);
    void resized() override;
private:
    juce::ToggleButton button;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    LowEnhancerProcessor& processor;
};

// ============================================================================
class LowEnhancerEditor : public juce::AudioProcessorEditor
{
public:
    explicit LowEnhancerEditor (LowEnhancerProcessor&);
    ~LowEnhancerEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    LowEnhancerProcessor& processorRef;
    LowEnhancerLookAndFeel laf;

    // Knob row
    LowLabelledKnob knobWarmth;
    LowLabelledKnob knobHpf;
    LowLabelledKnob knobSub;
    LowLabelledKnob knobDrive;
    LowLabelledKnob knobPunch;
    LowLabelledKnob knobMix;

    // Bottom strip controls
    LowToggleButton satModeToggle;   // TUBE / TIGHT
    LowOnOffButton  monoButton;      // MONO BASS on/off

    // Metering
    LowVUMeter vuIn;
    LowVUMeter vuOut;

    // Labels
    juce::Label titleLabel;
    juce::Label subtitleLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LowEnhancerEditor)
};
