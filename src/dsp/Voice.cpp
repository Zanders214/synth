#include "Voice.h"
#include <array>

namespace zw
{

ZWVoice::ZWVoice (const ParamRefs& refs, const WavetableLibrary& lib,
                  const ModMatrix& mm, const std::atomic<double>& bpm,
                  std::atomic<double>& lastNoteFreq)
    : p (refs), library (lib), matrix (mm), bpmRef (bpm), lastNoteFreqRef (lastNoteFreq)
{
    // Default each oscillator to its selected factory table; the per-block update
    // re-points them whenever the wtselect parameter changes (a pointer swap).
    oscA.setWavetable (library.getByIndex (0));
    oscB.setWavetable (library.getByIndex (0));
}

bool ZWVoice::canPlaySound (juce::SynthesiserSound* s)
{
    return dynamic_cast<ZWSound*> (s) != nullptr;
}

void ZWVoice::setCurrentPlaybackSampleRate (double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
    if (newRate > 0.0)
    {
        oscA.prepare (newRate);
        oscB.prepare (newRate);
        sub.prepare (newRate);
        noise.prepare (newRate);
        filter.prepare (newRate);
        ampEnv.setSampleRate (newRate);
        env2.setSampleRate (newRate);
        env3.setSampleRate (newRate);
        for (auto& l : lfo) l.prepare (newRate);
    }
}

float ZWVoice::lfoFreqHz (int i) const
{
    if (ParamRefs::on (p.lfo[i].sync))
    {
        static constexpr std::array<double, 6> beats { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };
        const int div = juce::jlimit (0, 5, (int) p.lfo[i].ratediv->load());
        double bpm = bpmRef.load();
        if (bpm <= 0.0) bpm = 120.0;
        return (float) ((bpm / 60.0) / beats[div]);
    }
    return p.lfo[i].ratehz->load();
}

void ZWVoice::startNote (int midiNoteNumber, float vel, juce::SynthesiserSound*, int pitchWheel)
{
    midiNote    = midiNoteNumber;
    rawVelocity = vel;
    velocity    = 0.2f + 0.8f * vel;
    noteNorm    = juce::jlimit (-1.0f, 1.0f, static_cast<float> (midiNote - 60) / 48.0f);
    pitchWheelMoved (pitchWheel);

    // Glide: start from the previous note's pitch (poly portamento) when enabled.
    targetFreq = 440.0 * std::pow (2.0, (static_cast<float> (midiNote - 69) + pitchBendSemis) / 12.0);
    const float glide = (p.glideTime != nullptr) ? p.glideTime->load() : 0.0f;
    noteFreq = (glide > 0.0001f) ? lastNoteFreqRef.load() : targetFreq;
    lastNoteFreqRef.store (targetFreq);

    const bool randPhase = ParamRefs::on (p.a.phaserand) || ParamRefs::on (p.b.phaserand);
    const float startPhaseA = (p.a.phase != nullptr) ? p.a.phase->load() / 360.0f : 0.0f;
    const float startPhaseB = (p.b.phase != nullptr) ? p.b.phase->load() / 360.0f : 0.0f;
    oscA.noteOn (randPhase, startPhaseA);
    oscB.noteOn (randPhase, startPhaseB);
    sub.noteOn();
    noise.reset();
    filter.reset();

    env2.setParameters (p.env2A->load(), p.env2D->load(), p.env2S->load(), p.env2R->load());
    env3.setParameters (p.env3A->load(), p.env3D->load(), p.env3S->load(), p.env3R->load());
    env2.noteOn();
    env3.noteOn();

    for (int i = 0; i < 4; ++i)
    {
        lfo[i].setParams ((int) p.lfo[i].shape->load(), lfoFreqHz (i), p.lfo[i].depth->load(),
                          p.lfo[i].rise->load(), p.lfo[i].phase->load() / 360.0f,
                          (int) p.lfo[i].mode->load());
        lfo[i].noteOn();
    }

    updateBlockParams (0);
    ampEnv.noteOn();
}

void ZWVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        ampEnv.noteOff();
        env2.noteOff();
        env3.noteOff();
    }
    else
    {
        ampEnv.reset();
        clearCurrentNote();
    }
}

void ZWVoice::pitchWheelMoved (int newValue)
{
    const float norm  = (float) (newValue - 8192) / 8192.0f;
    const float range = (p.bendRange != nullptr) ? p.bendRange->load() : 2.0f;
    pitchBendSemis = norm * range;
}

void ZWVoice::updateBlockParams (int numSamples)
{
    targetFreq = 440.0 * std::pow (2.0, (static_cast<float> (midiNote - 69) + pitchBendSemis) / 12.0);

    // Portamento: glide the sounding frequency toward the target over glide seconds.
    if (const float glide = (p.glideTime != nullptr) ? p.glideTime->load() : 0.0f;
        glide > 0.0001f && numSamples > 0)
    {
        const double coef = 1.0 - std::exp (-(double) numSamples / (glide * getSampleRate()));
        noteFreq += (targetFreq - noteFreq) * coef;
    }
    else
    {
        noteFreq = targetFreq;
    }

    aOn      = ParamRefs::on (p.a.enable);
    bOn      = ParamRefs::on (p.b.enable);
    subOn    = ParamRefs::on (p.subEnable);
    noiseOn  = ParamRefs::on (p.noiseEnable);
    filterOn = ParamRefs::on (p.filterEnable);
    rA = ParamRefs::on (p.routeA); rB = ParamRefs::on (p.routeB);
    rS = ParamRefs::on (p.routeS); rN = ParamRefs::on (p.routeN);

    // Keep mod-envelope/LFO parameters current, then advance them by this block.
    env2.setParameters (p.env2A->load(), p.env2D->load(), p.env2S->load(), p.env2R->load());
    env3.setParameters (p.env3A->load(), p.env3D->load(), p.env3S->load(), p.env3R->load());
    for (int i = 0; i < 4; ++i)
        lfo[i].setParams ((int) p.lfo[i].shape->load(), lfoFreqHz (i), p.lfo[i].depth->load(),
                          p.lfo[i].rise->load(), p.lfo[i].phase->load() / 360.0f,
                          (int) p.lfo[i].mode->load());

    // ---- Gather modulation source values ----
    std::array<float, kNumModSources> src {};
    src[(int) ModSource::Env1] = ampEnv.getValue();
    src[(int) ModSource::Env2] = env2.processBlock (numSamples);
    src[(int) ModSource::Env3] = env3.processBlock (numSamples);
    src[(int) ModSource::Lfo1] = lfo[0].processBlock (numSamples);
    src[(int) ModSource::Lfo2] = lfo[1].processBlock (numSamples);
    src[(int) ModSource::Lfo3] = lfo[2].processBlock (numSamples);
    src[(int) ModSource::Lfo4] = lfo[3].processBlock (numSamples);
    for (int i = 0; i < 4; ++i)
        src[(int) ModSource::Macro1 + i] = p.macro[i]->load();
    src[(int) ModSource::Velocity] = rawVelocity;
    src[(int) ModSource::Note]     = noteNorm;

    std::array<float, kNumModDests> destSums {};
    matrix.computeDestSums (src.data(), destSums.data());

    auto modded = [&] (ModDest d)
    {
        const auto* pr = p.dest[(int) d];
        const float base01 = pr->getValue();
        const float f01    = clamp01 (base01 + destSums[(int) d]);
        return pr->getNormalisableRange().convertFrom0to1 (f01);
    };

    auto freqFor = [this] (const OscRefs& o)
    {
        const float st = semis (o.octave->load(), o.coarse->load(), o.fine->load());
        return noteFreq * std::pow (2.0, st / 12.0);
    };

    if (aOn)
    {
        // Per-osc wavetable selection: pure pointer swap into the pre-built library.
        oscA.setWavetable (library.getByIndex ((int) p.a.wtselect->load()));
        oscA.update ({ modded (ModDest::OscAWt), modded (ModDest::OscAWarp), (int) p.a.unison->load(),
                       modded (ModDest::OscADetune), modded (ModDest::OscALevel), modded (ModDest::OscAPan),
                       p.a.uniwidth->load(), freqFor (p.a), (int) p.a.warpmode->load() });
    }
    if (bOn)
    {
        oscB.setWavetable (library.getByIndex ((int) p.b.wtselect->load()));
        oscB.update ({ modded (ModDest::OscBWt), modded (ModDest::OscBWarp), (int) p.b.unison->load(),
                       modded (ModDest::OscBDetune), modded (ModDest::OscBLevel), modded (ModDest::OscBPan),
                       p.b.uniwidth->load(), freqFor (p.b), (int) p.b.warpmode->load() });
    }

    if (subOn)
        sub.update ((int) p.subWave->load(), (int) p.subOctave->load(),
                    p.subSat->load(), modded (ModDest::SubLevel), noteFreq);
    if (noiseOn)
        noise.update ((int) p.noiseType->load(), p.noiseColor->load(), modded (ModDest::NoiseLevel));

    filter.setParams ((int) p.filterType->load(), modded (ModDest::Cutoff),
                      modded (ModDest::Reso), modded (ModDest::Drive));
    filterMix = p.fmix->load();

    ampEnv.setParameters (p.env1A->load(), p.env1D->load(), p.env1S->load(), p.env1R->load());
}

void ZWVoice::renderNextBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (! ampEnv.isActive())
        return;

    updateBlockParams (numSamples);

    const int numCh = buffer.getNumChannels();
    auto* outL = buffer.getWritePointer (0, startSample);
    auto* outR = numCh > 1 ? buffer.getWritePointer (1, startSample) : nullptr;

    for (int n = 0; n < numSamples; ++n)
    {
        float fL = 0.0f;   // routed to filter
        float fR = 0.0f;   // routed to filter
        float dL = 0.0f;   // bypass (dry)
        float dR = 0.0f;   // bypass (dry)

        if (aOn)    { float l = 0; float r = 0; oscA.renderAdd (l, r); if (rA) { fL += l; fR += r; } else { dL += l; dR += r; } }
        if (bOn)    { float l = 0; float r = 0; oscB.renderAdd (l, r); if (rB) { fL += l; fR += r; } else { dL += l; dR += r; } }
        if (subOn)  { float s = sub.render();   if (rS) { fL += s; fR += s; } else { dL += s; dR += s; } }
        if (noiseOn){ float s = noise.render(); if (rN) { fL += s; fR += s; } else { dL += s; dR += s; } }

        float oL;
        float oR;
        if (filterOn)
        {
            const float wL = filter.processSample (0, fL);
            const float wR = filter.processSample (1, fR);
            oL = dL + fL + (wL - fL) * filterMix;
            oR = dR + fR + (wR - fR) * filterMix;
        }
        else
        {
            oL = dL + fL;
            oR = dR + fR;
        }

        const float amp = ampEnv.getNextSample() * velocity;
        outL[n] += oL * amp;
        if (outR != nullptr) outR[n] += oR * amp;
    }

    if (! ampEnv.isActive())
        clearCurrentNote();
}

} // namespace zw
