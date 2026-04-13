#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>

// ============================================================================
// LowEnhancerProcessor  — Professional low-end processing chain
//
// Signal chain (all per-channel except MONO BASS which is stereo-only):
//   1. HPF       — Rumble filter 20–100 Hz. Clears subsonic mud before adding sub.
//   2. WARMTH    — Low shelf at 120 Hz, up to +12 dB. Adds body and weight.
//   3. SUB       — Sub-harmonic synthesiser: LP → saturate → extract harmonics
//                  → LP ceiling → DC block → blend.
//                  DRIVE controls pre-gain. SAT MODE sets character.
//   4. PUNCH     — Dual-envelope transient shaper on the low band.
//                  Detects low-freq attack peaks and briefly boosts them.
//                  Makes kicks hit harder without lifting sustain.
//   5. MONO BASS — Linkwitz-Riley crossover at ~100 Hz. Sums the low band
//                  to mono, keeps highs stereo. Essential for master bus /
//                  club PA compatibility.
//   6. MIX       — Parallel dry/wet with per-sample smoothing.
// ============================================================================

class LowEnhancerProcessor : public juce::AudioProcessor,
                              public juce::AudioProcessorValueTreeState::Listener
{
public:
    LowEnhancerProcessor();
    ~LowEnhancerProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()  override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    void parameterChanged (const juce::String& paramID, float newValue) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

    float getInputRMS()  const { return inputRMS.load(); }
    float getOutputRMS() const { return outputRMS.load(); }

private:
    static constexpr int MAX_CHANNELS = 2;

    // =========================================================================
    // Per-channel DSP state
    struct ChannelState
    {
        // HPF: rumble removal
        juce::IIRFilter hpf;

        // WARMTH: low shelf
        juce::IIRFilter warmthShelf;

        // SUB synth: feed chain
        juce::IIRFilter subLP;         // pre-filter — isolate sub content
        juce::IIRFilter subCleanLP;    // post-clip LP ceiling
        juce::IIRFilter subDCBlock;    // DC removal after saturation

        // MONO BASS: Linkwitz-Riley crossover (two cascaded 2nd-order filters)
        juce::IIRFilter monoLP1, monoLP2;   // low band  (summed to mono)
        juce::IIRFilter monoHP1, monoHP2;   // high band (kept stereo)

        // PUNCH: dual envelope follower state
        float punchFastEnv = 0.0f;
        float punchSlowEnv = 0.0f;
    };

    std::array<ChannelState, MAX_CHANNELS> channelStates;

    // Envelope follower coefficients (recomputed in prepareToPlay)
    float punchFastAttack  = 0.0f;
    float punchFastRelease = 0.0f;
    float punchSlowAttack  = 0.0f;
    float punchSlowRelease = 0.0f;

    void rebuildFilters (double sr);

    // =========================================================================
    // Saturation — two modes:
    //   TUBE  (satmode=false): tanh — smooth, even+odd harmonics, classic warm
    //   TIGHT (satmode=true):  hard clip scaled back — aggressive odd harmonics,
    //                          punchy transistor character
    static inline float saturate (float x, float drive, bool tight)
    {
        if (tight)
        {
            // Hard clip with pre-gain, normalised back: gives crunchy odd harmonics
            const float boosted = x * drive;
            const float clipped = juce::jlimit (-1.0f, 1.0f, boosted);
            return clipped / std::max (drive, 0.001f);
        }
        // Tube: tanh, smooth and warm
        return std::tanh (x * drive) / std::max (drive, 0.001f);
    }

    // =========================================================================
    // Smoothed parameters
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothWarmth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothHpf;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothSub;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothDrive;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothPunch;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothMix;

    std::atomic<float> pWarmth  { 0.0f };
    std::atomic<float> pHpf     { 0.0f };   // Hz value, 0 = bypassed
    std::atomic<float> pSub     { 0.0f };
    std::atomic<float> pDrive   { 0.3f };
    std::atomic<float> pPunch   { 0.0f };
    std::atomic<float> pMix     { 1.0f };
    std::atomic<bool>  pSatMode { false };   // false=TUBE, true=TIGHT
    std::atomic<bool>  pMonoBass{ false };

    std::atomic<float> inputRMS  { 0.0f };
    std::atomic<float> outputRMS { 0.0f };
    std::atomic<bool>  filtersNeedUpdate { false };

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LowEnhancerProcessor)
};
