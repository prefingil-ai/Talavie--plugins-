#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

struct PadSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

// Voix de pad : 3 familles de sons
//   0 Chaud   : 5 ondes en dents de scie désaccordées + sub
//   1 Aerien  : scie + triangle + souffle (bruit filtré), plus large
//   2 Cristal : synthèse FM douce (timbre de verre / cloche) + brillance
class PadVoice : public juce::SynthesiserVoice
{
public:
    explicit PadVoice (juce::AudioProcessorValueTreeState& s) : apvts (s) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<PadSound*> (s) != nullptr; }
    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override;
    void stopNote (float, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;

private:
    static constexpr int maxOsc = 5;
    juce::AudioProcessorValueTreeState& apvts;
    juce::ADSR adsr;
    juce::Random rng;

    int   type = 0, nOsc = 5;
    float freq = 440.f, vel = 1.f;
    float ph[maxOsc] {}, inc[maxOsc] {}, gl[maxOsc] {}, gr[maxOsc] {};
    float subPh = 0.f, lfoPh = 0.f, lfoInc = 0.f;
    float fmEnv = 1.f, fmCoef = 0.9999f, fmIndex = 1.6f;
    float lp[2][2] {};
    float nLp[2] {};
};

class TaLaViePadsProcessor : public juce::AudioProcessor
{
public:
    TaLaViePadsProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout& l) const override
    { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TaLaVie Pads"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::Synthesiser synth;
    juce::dsp::Chorus<float> chorusFx;
    juce::Reverb reverb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TaLaViePadsProcessor)
};
