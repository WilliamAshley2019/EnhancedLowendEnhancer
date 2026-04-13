#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

namespace ParamID
{
    static const juce::String warmth   = "warmth";
    static const juce::String hpf      = "hpf";
    static const juce::String sub      = "sub";
    static const juce::String drive    = "drive";
    static const juce::String punch    = "punch";
    static const juce::String mix      = "mix";
    static const juce::String satmode  = "satmode";
    static const juce::String monobass = "monobass";
}

// ============================================================================
LowEnhancerProcessor::LowEnhancerProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LowEnhancerState", createParameterLayout())
{
    apvts.addParameterListener (ParamID::warmth,   this);
    apvts.addParameterListener (ParamID::hpf,      this);
    apvts.addParameterListener (ParamID::sub,      this);
    apvts.addParameterListener (ParamID::drive,    this);
    apvts.addParameterListener (ParamID::punch,    this);
    apvts.addParameterListener (ParamID::mix,      this);
    apvts.addParameterListener (ParamID::satmode,  this);
    apvts.addParameterListener (ParamID::monobass, this);
}

LowEnhancerProcessor::~LowEnhancerProcessor()
{
    apvts.removeParameterListener (ParamID::warmth,   this);
    apvts.removeParameterListener (ParamID::hpf,      this);
    apvts.removeParameterListener (ParamID::sub,      this);
    apvts.removeParameterListener (ParamID::drive,    this);
    apvts.removeParameterListener (ParamID::punch,    this);
    apvts.removeParameterListener (ParamID::mix,      this);
    apvts.removeParameterListener (ParamID::satmode,  this);
    apvts.removeParameterListener (ParamID::monobass, this);
}

// ============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
LowEnhancerProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParamID::warmth, "Warmth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("amount")));

    // HPF: 0 = off, otherwise 20–100 Hz. Skewed so the knob feels logarithmic.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParamID::hpf, "HPF",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f, 0.4f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParamID::sub, "Sub",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("amount")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParamID::drive, "Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f,
        juce::AudioParameterFloatAttributes().withLabel ("amount")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParamID::punch, "Punch",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("amount")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParamID::mix, "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dry/wet")));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParamID::satmode, "Sat Mode", false));   // false=TUBE, true=TIGHT

    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParamID::monobass, "Mono Bass", false));

    return layout;
}

void LowEnhancerProcessor::parameterChanged (const juce::String& paramID, float newValue)
{
    if      (paramID == ParamID::warmth)   { pWarmth.store (newValue);          filtersNeedUpdate.store (true); }
    else if (paramID == ParamID::hpf)      { pHpf.store (newValue);             filtersNeedUpdate.store (true); }
    else if (paramID == ParamID::sub)        pSub.store (newValue);
    else if (paramID == ParamID::drive)      pDrive.store (newValue);
    else if (paramID == ParamID::punch)      pPunch.store (newValue);
    else if (paramID == ParamID::mix)        pMix.store (newValue);
    else if (paramID == ParamID::satmode)    pSatMode.store (newValue > 0.5f);
    else if (paramID == ParamID::monobass)   pMonoBass.store (newValue > 0.5f);
}

// ============================================================================
// Filter rebuild — called in prepareToPlay and when relevant params change
// ============================================================================
void LowEnhancerProcessor::rebuildFilters (double sr)
{
    // WARMTH: Low shelf at 120 Hz, +12 dB max
    const double warmthGainDb  = static_cast<double> (pWarmth.load()) * 12.0;
    const double warmthGainLin = std::pow (10.0, warmthGainDb / 20.0);

    // HPF: 0 = bypass, otherwise use parameter value as frequency
    const float hpfFreq = pHpf.load();
    const bool  hpfOn   = hpfFreq > 0.5f;

    // MONO BASS: Linkwitz-Riley 4th-order at 100 Hz
    // Implemented as two cascaded 2nd-order Butterworth (Q=0.707)
    const double monoXover = 100.0;

    for (auto& ch : channelStates)
    {
        // Warmth shelf
        ch.warmthShelf.setCoefficients (
            juce::IIRCoefficients::makeLowShelf (sr, 120.0, 0.7, warmthGainLin));

        // HPF — if bypassed we still set coefficients so the filter object
        // is initialised; we just skip calling it in processBlock
        if (hpfOn)
            ch.hpf.setCoefficients (
                juce::IIRCoefficients::makeHighPass (sr, static_cast<double> (hpfFreq), 0.7));

        // Sub synth filters
        ch.subLP.setCoefficients      (juce::IIRCoefficients::makeLowPass  (sr, 200.0, 0.7));
        ch.subCleanLP.setCoefficients (juce::IIRCoefficients::makeLowPass  (sr, 500.0, 0.7));
        ch.subDCBlock.setCoefficients (juce::IIRCoefficients::makeHighPass (sr,   5.0, 0.7));

        // Linkwitz-Riley crossover (two cascaded Butterworth sections)
        auto lpCoeffs = juce::IIRCoefficients::makeLowPass  (sr, monoXover, 0.707);
        auto hpCoeffs = juce::IIRCoefficients::makeHighPass (sr, monoXover, 0.707);
        ch.monoLP1.setCoefficients (lpCoeffs);
        ch.monoLP2.setCoefficients (lpCoeffs);
        ch.monoHP1.setCoefficients (hpCoeffs);
        ch.monoHP2.setCoefficients (hpCoeffs);
    }

    // PUNCH envelope coefficients
    // Fast follower: 1 ms attack, 30 ms release  — catches sharp transients
    // Slow follower: 40 ms attack, 150 ms release — tracks the sustained level
    auto makeCoeff = [sr] (double ms) {
        return static_cast<float> (std::exp (-1.0 / (sr * ms * 0.001)));
    };
    punchFastAttack  = makeCoeff (1.0);
    punchFastRelease = makeCoeff (30.0);
    punchSlowAttack  = makeCoeff (40.0);
    punchSlowRelease = makeCoeff (150.0);
}

// ============================================================================
void LowEnhancerProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    // Sync atomics from APVTS
    pWarmth.store   (*apvts.getRawParameterValue (ParamID::warmth));
    pHpf.store      (*apvts.getRawParameterValue (ParamID::hpf));
    pSub.store      (*apvts.getRawParameterValue (ParamID::sub));
    pDrive.store    (*apvts.getRawParameterValue (ParamID::drive));
    pPunch.store    (*apvts.getRawParameterValue (ParamID::punch));
    pMix.store      (*apvts.getRawParameterValue (ParamID::mix));
    pSatMode.store  (*apvts.getRawParameterValue (ParamID::satmode)  > 0.5f);
    pMonoBass.store (*apvts.getRawParameterValue (ParamID::monobass) > 0.5f);

    // Smoothers — 50 ms ramp
    const double ramp = 0.05;
    smoothWarmth.reset (sampleRate, ramp); smoothWarmth.setCurrentAndTargetValue (pWarmth.load());
    smoothHpf.reset    (sampleRate, ramp); smoothHpf.setCurrentAndTargetValue    (pHpf.load());
    smoothSub.reset    (sampleRate, ramp); smoothSub.setCurrentAndTargetValue    (pSub.load());
    smoothDrive.reset  (sampleRate, ramp); smoothDrive.setCurrentAndTargetValue  (pDrive.load());
    smoothPunch.reset  (sampleRate, ramp); smoothPunch.setCurrentAndTargetValue  (pPunch.load());
    smoothMix.reset    (sampleRate, ramp); smoothMix.setCurrentAndTargetValue    (pMix.load());

    // Reset punch envelope state
    for (auto& ch : channelStates)
    {
        ch.punchFastEnv = 0.0f;
        ch.punchSlowEnv = 0.0f;
    }

    rebuildFilters (sampleRate);
    filtersNeedUpdate.store (false);
}

void LowEnhancerProcessor::releaseResources()
{
    for (auto& ch : channelStates)
    {
        ch.punchFastEnv = 0.0f;
        ch.punchSlowEnv = 0.0f;
    }
}

// ============================================================================
void LowEnhancerProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    if (filtersNeedUpdate.exchange (false))
        rebuildFilters (currentSampleRate);

    // Update smoother targets
    smoothWarmth.setTargetValue (pWarmth.load());
    smoothSub.setTargetValue    (pSub.load());
    smoothDrive.setTargetValue  (pDrive.load());
    smoothPunch.setTargetValue  (pPunch.load());
    smoothMix.setTargetValue    (pMix.load());
    // HPF is filter-rebuild-based, no per-sample value needed

    // Input RMS
    {
        float rms = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch) rms += buffer.getRMSLevel (ch, 0, numSamples);
        inputRMS.store (rms / (float)numChannels);
    }

    // Snapshot smoothers once per block
    std::vector<float> snapWarmth (numSamples), snapSub   (numSamples),
                       snapDrive  (numSamples), snapPunch  (numSamples),
                       snapMix    (numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        snapWarmth[i] = smoothWarmth.getNextValue();
        snapSub[i]    = smoothSub.getNextValue();
        snapDrive[i]  = smoothDrive.getNextValue();
        snapPunch[i]  = smoothPunch.getNextValue();
        snapMix[i]    = smoothMix.getNextValue();
    }

    const bool hpfActive  = pHpf.load() > 0.5f;
    const bool tight      = pSatMode.load();
    const bool monoBassOn = pMonoBass.load() && numChannels >= 2;

    // Dry copy for wet/dry
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf (buffer);

    // =========================================================================
    // Per-channel processing (HPF, WARMTH, SUB, PUNCH)
    // =========================================================================
    for (int ch = 0; ch < std::min (numChannels, MAX_CHANNELS); ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        auto&  cs   = channelStates[ch];

        // --- 1. HPF: remove subsonic rumble ---
        if (hpfActive)
            cs.hpf.processSamples (data, numSamples);

        // --- 2. WARMTH: low shelf blend ---
        {
            std::vector<float> shelved (numSamples);
            std::copy (data, data + numSamples, shelved.data());
            cs.warmthShelf.processSamples (shelved.data(), numSamples);
            for (int i = 0; i < numSamples; ++i)
                data[i] += (shelved[i] - data[i]) * snapWarmth[i];
        }

        // --- 3. SUB: sub-harmonic synthesiser ---
        {
            std::vector<float> subSignal (numSamples);
            std::copy (data, data + numSamples, subSignal.data());
            cs.subLP.processSamples (subSignal.data(), numSamples);

            std::vector<float> harmonics (numSamples);
            for (int i = 0; i < numSamples; ++i)
            {
                // Drive: 1.5x–8x, generates harmonics at 2f, 3f, 4f...
                const float driveGain = 1.5f + snapDrive[i] * 6.5f;
                const float sat       = saturate (subSignal[i], driveGain, tight);
                harmonics[i]          = sat - subSignal[i];
            }
            cs.subCleanLP.processSamples (harmonics.data(), numSamples);
            cs.subDCBlock.processSamples (harmonics.data(), numSamples);
            for (int i = 0; i < numSamples; ++i)
                data[i] += harmonics[i] * snapSub[i] * 0.7f;
        }

        // --- 4. PUNCH: low-end transient shaper ---
        // Dual envelope follower: fast tracks peaks, slow tracks average.
        // When fast > slow, we are in an attack — boost proportionally.
        // Applied only to frequencies below ~200 Hz so we punch the sub/kick
        // without brightening the mids.
        if (snapPunch[numSamples - 1] > 0.001f)
        {
            std::vector<float> lowBand (numSamples);
            std::copy (data, data + numSamples, lowBand.data());
            // Temporary LP to isolate low-band transients for envelope detection
            // (we don't filter the actual output, just use it for gain computation)
            juce::IIRFilter tempLP;
            tempLP.setCoefficients (juce::IIRCoefficients::makeLowPass (currentSampleRate, 200.0, 0.7));
            tempLP.processSamples (lowBand.data(), numSamples);

            for (int i = 0; i < numSamples; ++i)
            {
                const float lvl = std::abs (lowBand[i]);

                // Update fast envelope
                const float fa = punchFastAttack, fr = punchFastRelease;
                cs.punchFastEnv = (lvl > cs.punchFastEnv)
                    ? fa * cs.punchFastEnv + (1.0f - fa) * lvl
                    : fr * cs.punchFastEnv + (1.0f - fr) * lvl;

                // Update slow envelope
                const float sa = punchSlowAttack, sr2 = punchSlowRelease;
                cs.punchSlowEnv = (lvl > cs.punchSlowEnv)
                    ? sa * cs.punchSlowEnv + (1.0f - sa) * lvl
                    : sr2 * cs.punchSlowEnv + (1.0f - sr2) * lvl;

                // Transient = how much faster the fast env is rising vs slow
                // Clamped to [0,1] so it only boosts, never cuts
                const float transient = juce::jlimit (0.0f, 1.0f,
                    (cs.punchFastEnv - cs.punchSlowEnv) /
                    std::max (cs.punchSlowEnv + 1e-9f, 1e-6f));

                // Apply transient boost — up to 6 dB at punch=1 and full transient
                const float boostGain = 1.0f + transient * snapPunch[i] * 1.0f;
                data[i] *= boostGain;
            }
        }
    }

    // =========================================================================
    // MONO BASS: Linkwitz-Riley 4th-order crossover at 100 Hz
    // Low band is summed to mono; high band remains stereo.
    // Only runs when enabled and we actually have 2 channels.
    // =========================================================================
    if (monoBassOn)
    {
        float* L = buffer.getWritePointer (0);
        float* R = buffer.getWritePointer (1);
        auto&  csL = channelStates[0];
        auto&  csR = channelStates[1];

        std::vector<float> lowL (numSamples), lowR  (numSamples),
                           highL(numSamples), highR (numSamples);

        // Split each channel through cascaded LP (two passes = LR4)
        std::copy (L, L + numSamples, lowL.data());
        csL.monoLP1.processSamples (lowL.data(), numSamples);
        csL.monoLP2.processSamples (lowL.data(), numSamples);

        std::copy (R, R + numSamples, lowR.data());
        csR.monoLP1.processSamples (lowR.data(), numSamples);
        csR.monoLP2.processSamples (lowR.data(), numSamples);

        std::copy (L, L + numSamples, highL.data());
        csL.monoHP1.processSamples (highL.data(), numSamples);
        csL.monoHP2.processSamples (highL.data(), numSamples);

        std::copy (R, R + numSamples, highR.data());
        csR.monoHP1.processSamples (highR.data(), numSamples);
        csR.monoHP2.processSamples (highR.data(), numSamples);

        // Sum low bands to mono (equal power: * 0.5 avoids +3dB)
        for (int i = 0; i < numSamples; ++i)
        {
            const float monoLow = (lowL[i] + lowR[i]) * 0.5f;
            L[i] = monoLow + highL[i];
            R[i] = monoLow + highR[i];
        }
    }

    // =========================================================================
    // MIX: per-sample smoothed dry/wet
    // =========================================================================
    for (int ch = 0; ch < std::min (numChannels, MAX_CHANNELS); ++ch)
    {
        float*       data = buffer.getWritePointer (ch);
        const float* dry  = dryBuffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = dry[i] * (1.0f - snapMix[i]) + data[i] * snapMix[i];
    }

    // Output RMS
    {
        float rms = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch) rms += buffer.getRMSLevel (ch, 0, numSamples);
        outputRMS.store (rms / (float)numChannels);
    }
}

// ============================================================================
void LowEnhancerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void LowEnhancerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LowEnhancerProcessor();
}
