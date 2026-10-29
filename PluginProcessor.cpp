#include "PluginProcessor.h"

namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    // Anti-aliasing d'une onde en dents de scie
    inline float polyBlep (float t, float dt)
    {
        if (t < dt)         { t /= dt;               return t + t - t * t - 1.f; }
        if (t > 1.f - dt)   { t = (t - 1.f) / dt;    return t * t + t + t + 1.f; }
        return 0.f;
    }
}

//==============================  VOIX  ==============================
void PadVoice::startNote (int midiNote, float velocity, juce::SynthesiserSound*, int)
{
    auto val = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const double sr = getSampleRate();

    type  = (int) val ("type");
    nOsc  = (type == 0) ? 5 : (type == 1 ? 4 : 3);
    freq  = (float) juce::MidiMessage::getMidiNoteInHertz (midiNote);
    vel   = velocity;

    const float detCents = val ("detune") * 30.f * (type == 1 ? 1.4f : 1.f);
    for (int k = 0; k < nOsc; ++k)
    {
        const float x     = nOsc > 1 ? (k - (nOsc - 1) * 0.5f) / ((nOsc - 1) * 0.5f) : 0.f;   // -1 .. +1
        const float ratio = std::pow (2.f, detCents * x / 1200.f);
        inc[k] = freq * ratio / (float) sr;
        ph[k]  = rng.nextFloat();
        const float angle = (x * 0.85f + 1.f) * juce::MathConstants<float>::pi * 0.25f;        // largeur stéréo
        gl[k] = std::cos (angle);
        gr[k] = std::sin (angle);
    }

    subPh  = 0.f;
    lfoPh  = rng.nextFloat() * twoPi;
    lfoInc = twoPi * (0.15f + 0.2f * rng.nextFloat()) / (float) sr;   // mouvement lent, différent par note
    fmEnv  = 1.f;
    fmCoef = (float) std::exp (-1.0 / (sr * 2.5));
    for (auto& c : lp) for (auto& v : c) v = 0.f;

    adsr.setSampleRate (sr);
    adsr.setParameters ({ val ("attack"), val ("decay"), val ("sustain"), val ("release") });
    adsr.noteOn();
}

void PadVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff) adsr.noteOff();
    else { adsr.reset(); clearCurrentNote(); }
}

void PadVoice::renderNextBlock (juce::AudioBuffer<float>& out, int start, int num)
{
    if (! isVoiceActive()) return;

    auto val = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const float sr      = (float) getSampleRate();
    const float cutBase = val ("cutoff") * (type == 0 ? 1.f : (type == 1 ? 1.8f : 3.f));
    const float swell   = val ("swell"), move = val ("movement");
    const float subL    = val ("sub"),   air  = val ("air");
    const float norm    = 1.f / std::sqrt ((float) nOsc);

    for (int i = 0; i < num; ++i)
    {
        if (! adsr.isActive()) { clearCurrentNote(); break; }

        const float env = adsr.getNextSample();
        const float lfo = std::sin (lfoPh);
        lfoPh += lfoInc; if (lfoPh > twoPi) lfoPh -= twoPi;

        float L = 0.f, R = 0.f;

        if (type < 2)
        {
            for (int k = 0; k < nOsc; ++k)
            {
                ph[k] += inc[k]; if (ph[k] >= 1.f) ph[k] -= 1.f;
                const float saw = 2.f * ph[k] - 1.f - polyBlep (ph[k], inc[k]);
                const float s   = (type == 0) ? saw : 0.5f * saw + 0.5f * (2.f * std::abs (2.f * ph[k] - 1.f) - 1.f);
                L += s * gl[k];
                R += s * gr[k];
            }
            L *= norm; R *= norm;

            subPh += freq * 0.5f / sr; if (subPh >= 1.f) subPh -= 1.f;
            const float sub = std::sin (twoPi * subPh) * subL * (type == 0 ? 0.7f : 0.4f);
            L += sub; R += sub;

            if (type == 1)   // souffle d'air
            {
                const float nl = rng.nextFloat() * 2.f - 1.f, nr = rng.nextFloat() * 2.f - 1.f;
                nLp[0] += 0.15f * (nl - nLp[0]);
                nLp[1] += 0.15f * (nr - nLp[1]);
                L += air * 0.35f * (nl - nLp[0]);
                R += air * 0.35f * (nr - nLp[1]);
            }
        }
        else
        {
            const float idx = fmIndex * (0.25f + 0.75f * fmEnv);    // l'index FM retombe : le son s'adoucit
            fmEnv *= fmCoef;
            for (int k = 0; k < nOsc; ++k)
            {
                ph[k] += inc[k]; if (ph[k] >= 1.f) ph[k] -= 1.f;
                const float s = std::sin (twoPi * (ph[k] + idx * std::sin (2.f * twoPi * ph[k])));
                L += s * gl[k];
                R += s * gr[k];
            }
            L *= norm * 0.8f; R *= norm * 0.8f;

            subPh += freq * 4.f / sr; if (subPh >= 1.f) subPh -= 1.f;     // brillance 2 octaves au-dessus
            const float sh = std::sin (twoPi * subPh) * 0.06f * (0.5f + 0.5f * lfo);
            L += sh; R += sh;
        }

        // Filtre passe-bas 12 dB qui s'ouvre avec l'enveloppe (le pad « respire »)
        float fc = cutBase * (1.f + swell * 4.f * env) * (1.f + 0.3f * move * lfo);
        fc = juce::jlimit (60.f, 0.45f * sr, fc);
        const float g = 1.f - std::exp (-twoPi * fc / sr);
        float chans[2] = { L, R };
        for (int ch = 0; ch < 2; ++ch)
        {
            lp[ch][0] += g * (chans[ch] - lp[ch][0]);
            lp[ch][1] += g * (lp[ch][0] - lp[ch][1]);
            chans[ch] = lp[ch][1];
        }

        const float amp = env * (0.5f + 0.5f * vel) * 0.25f;
        out.addSample (0, start + i, chans[0] * amp);
        out.addSample (1, start + i, chans[1] * amp);
    }
}

//============================  PROCESSEUR  ===========================
juce::AudioProcessorValueTreeState::ParameterLayout TaLaViePadsProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using R = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("type", "Pad",
                 juce::StringArray { "Chaud", "Aerien", "Cristal" }, 0));
    p.push_back (std::make_unique<P> ("attack",     "Attaque (s)",     R (0.01f, 6.f, 0.01f, 0.5f), 1.2f));
    p.push_back (std::make_unique<P> ("decay",      "Decay (s)",       R (0.1f, 6.f, 0.01f, 0.5f), 2.f));
    p.push_back (std::make_unique<P> ("sustain",    "Sustain",         R (0.f, 1.f, 0.01f), 0.85f));
    p.push_back (std::make_unique<P> ("release",    "Release (s)",     R (0.1f, 10.f, 0.01f, 0.5f), 3.f));
    p.push_back (std::make_unique<P> ("cutoff",     "Filtre (Hz)",     R (100.f, 12000.f, 1.f, 0.4f), 1800.f));
    p.push_back (std::make_unique<P> ("swell",      "Ouverture",       R (0.f, 1.f, 0.01f), 0.5f));
    p.push_back (std::make_unique<P> ("detune",     "Largeur (detune)",R (0.f, 1.f, 0.01f), 0.5f));
    p.push_back (std::make_unique<P> ("movement",   "Mouvement",       R (0.f, 1.f, 0.01f), 0.5f));
    p.push_back (std::make_unique<P> ("sub",        "Sub (graves)",    R (0.f, 1.f, 0.01f), 0.3f));
    p.push_back (std::make_unique<P> ("air",        "Souffle (Aerien)",R (0.f, 1.f, 0.01f), 0.25f));
    p.push_back (std::make_unique<P> ("chorus",     "Chorus",          R (0.f, 1.f, 0.01f), 0.5f));
    p.push_back (std::make_unique<P> ("reverbSize", "Reverb taille",   R (0.f, 1.f, 0.01f), 0.85f));
    p.push_back (std::make_unique<P> ("reverbMix",  "Reverb mix",      R (0.f, 1.f, 0.01f), 0.45f));
    p.push_back (std::make_unique<P> ("volume",     "Volume",          R (0.f, 1.5f, 0.01f), 0.9f));
    return { p.begin(), p.end() };
}

TaLaViePadsProcessor::TaLaViePadsProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    synth.addSound (new PadSound());
    for (int i = 0; i < 8; ++i)
        synth.addVoice (new PadVoice (apvts));
}

void TaLaViePadsProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sr);
    reverb.setSampleRate (sr);
    reverb.reset();

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) samplesPerBlock, 2 };
    chorusFx.prepare (spec);
    chorusFx.setRate (0.35f);
    chorusFx.setDepth (0.35f);
    chorusFx.setCentreDelay (15.f);
    chorusFx.setFeedback (0.1f);
}

void TaLaViePadsProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buf.clear();
    synth.renderNextBlock (buf, midi, 0, buf.getNumSamples());

    auto val = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    chorusFx.setMix (0.5f * val ("chorus"));
    {
        juce::dsp::AudioBlock<float> block (buf);
        chorusFx.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    const float mix = val ("reverbMix");
    juce::Reverb::Parameters rp;
    rp.roomSize = val ("reverbSize");
    rp.damping  = 0.6f;
    rp.wetLevel = mix * 0.6f;
    rp.dryLevel = 1.f - 0.3f * mix;
    rp.width    = 1.f;
    reverb.setParameters (rp);
    reverb.processStereo (buf.getWritePointer (0), buf.getWritePointer (1), buf.getNumSamples());

    buf.applyGain (val ("volume"));
}

juce::AudioProcessorEditor* TaLaViePadsProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void TaLaViePadsProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void TaLaViePadsProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TaLaViePadsProcessor(); }
