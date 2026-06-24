#include "Voice.h"

namespace zw
{

ZWVoice::ZWVoice (const ParamRefs& refs, const Wavetable& wt)
    : p (refs), table (wt)
{
    oscA.setWavetable (&table);
    oscB.setWavetable (&table);
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
    }
}

void ZWVoice::startNote (int midiNoteNumber, float vel, juce::SynthesiserSound*, int pitchWheel)
{
    midiNote = midiNoteNumber;
    velocity = 0.2f + 0.8f * vel;
    pitchWheelMoved (pitchWheel);

    const bool randPhase = ParamRefs::on (p.a.phaserand) || ParamRefs::on (p.b.phaserand);
    const float startPhaseA = (p.a.phase != nullptr) ? p.a.phase->load() / 360.0f : 0.0f;
    const float startPhaseB = (p.b.phase != nullptr) ? p.b.phase->load() / 360.0f : 0.0f;
    oscA.noteOn (randPhase, startPhaseA);
    oscB.noteOn (randPhase, startPhaseB);
    sub.noteOn();
    noise.reset();
    filter.reset();

    updateBlockParams();
    ampEnv.noteOn();
}

void ZWVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        ampEnv.noteOff();
    }
    else
    {
        ampEnv.reset();
        clearCurrentNote();
    }
}

void ZWVoice::pitchWheelMoved (int newValue)
{
    const float norm = (float) (newValue - 8192) / 8192.0f;     // -1..+1
    const float range = (p.bendRange != nullptr) ? p.bendRange->load() : 2.0f;
    pitchBendSemis = norm * range;
}

void ZWVoice::updateBlockParams()
{
    noteFreq = 440.0 * std::pow (2.0, (midiNote - 69 + pitchBendSemis) / 12.0);

    aOn      = ParamRefs::on (p.a.enable);
    bOn      = ParamRefs::on (p.b.enable);
    subOn    = ParamRefs::on (p.subEnable);
    noiseOn  = ParamRefs::on (p.noiseEnable);
    filterOn = ParamRefs::on (p.filterEnable);
    rA = ParamRefs::on (p.routeA); rB = ParamRefs::on (p.routeB);
    rS = ParamRefs::on (p.routeS); rN = ParamRefs::on (p.routeN);

    auto freqFor = [this] (const OscRefs& o)
    {
        const float st = semis (o.octave->load(), o.coarse->load(), o.fine->load());
        return noteFreq * std::pow (2.0, st / 12.0);
    };

    if (aOn)
        oscA.update (p.a.wtpos->load(), p.a.warp->load(), (int) p.a.unison->load(),
                     p.a.detune->load(), p.a.level->load(), p.a.pan->load(),
                     p.a.uniwidth->load(), freqFor (p.a));
    if (bOn)
        oscB.update (p.b.wtpos->load(), p.b.warp->load(), (int) p.b.unison->load(),
                     p.b.detune->load(), p.b.level->load(), p.b.pan->load(),
                     p.b.uniwidth->load(), freqFor (p.b));

    if (subOn)
        sub.update ((int) p.subWave->load(), (int) p.subOctave->load(),
                    p.subSat->load(), p.subLevel->load(), noteFreq);
    if (noiseOn)
        noise.update ((int) p.noiseType->load(), p.noiseColor->load(), p.noiseLevel->load());

    filter.setParams ((int) p.filterType->load(), p.cutoff->load(), p.reso->load(), p.drive->load());
    filterMix = p.fmix->load();

    ampEnv.setParameters (p.env1A->load(), p.env1D->load(), p.env1S->load(), p.env1R->load());
}

void ZWVoice::renderNextBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (! ampEnv.isActive())
        return;

    updateBlockParams();

    const int numCh = buffer.getNumChannels();
    auto* outL = buffer.getWritePointer (0, startSample);
    auto* outR = numCh > 1 ? buffer.getWritePointer (1, startSample) : nullptr;

    for (int n = 0; n < numSamples; ++n)
    {
        float fL = 0.0f, fR = 0.0f;   // routed to filter
        float dL = 0.0f, dR = 0.0f;   // bypass (dry)

        if (aOn)    { float l = 0, r = 0; oscA.renderAdd (l, r); if (rA) { fL += l; fR += r; } else { dL += l; dR += r; } }
        if (bOn)    { float l = 0, r = 0; oscB.renderAdd (l, r); if (rB) { fL += l; fR += r; } else { dL += l; dR += r; } }
        if (subOn)  { float s = sub.render();   if (rS) { fL += s; fR += s; } else { dL += s; dR += s; } }
        if (noiseOn){ float s = noise.render(); if (rN) { fL += s; fR += s; } else { dL += s; dR += s; } }

        float oL, oR;
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
