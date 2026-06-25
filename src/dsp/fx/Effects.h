#pragma once

#include <JuceHeader.h>
#include "../MultimodeFilter.h"
#include "../FastMath.h"
#include <cmath>

namespace zw::fx
{

//==============================================================================
// Shared helper: blend a processed (wet) block with a stored dry copy.
inline void blend (juce::dsp::AudioBlock<float>& block, const juce::AudioBuffer<float>& dry, float mix)
{
    const int nc = (int) block.getNumChannels();
    const int ns = (int) block.getNumSamples();
    for (int c = 0; c < nc; ++c)
    {
        auto* w = block.getChannelPointer ((size_t) c);
        const auto* d = dry.getReadPointer (juce::jmin (c, dry.getNumChannels() - 1));
        for (int i = 0; i < ns; ++i)
            w[i] = d[i] * (1.0f - mix) + w[i] * mix;
    }
}

// A dry-copy scratch used by effects that blend internally.
struct DryStore
{
    juce::AudioBuffer<float> buf;
    void prepare (const juce::dsp::ProcessSpec& s) { buf.setSize ((int) s.numChannels, (int) s.maximumBlockSize, false, false, true); }
    void copyFrom (const juce::dsp::AudioBlock<float>& block)
    {
        const int nc = (int) block.getNumChannels();
        const int ns = (int) block.getNumSamples();
        for (int c = 0; c < nc; ++c)
            buf.copyFrom (c, 0, block.getChannelPointer ((size_t) c), ns);
    }
};

//==============================================================================
class Hyper   // unison/dimension widener: detuned modulated delay ensemble
{
public:
    void prepare (const juce::dsp::ProcessSpec& s)
    {
        sr = s.sampleRate; dry.prepare (s);
        for (auto& d : line) { d.setMaximumDelayInSamples ((int) (sr * 0.05) + 4); d.prepare (s); d.reset(); }
        for (auto& ph : lfoPhase) ph = 0.0f;
    }
    void reset() { for (auto& d : line) d.reset(); }
    void setParams (float detune, int voices_, float width_, float mix_)
    { det = detune; voices = juce::jlimit (1, 8, voices_); width = width_; mix = mix_; }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        const int ns = (int) block.getNumSamples();
        auto* L = block.getChannelPointer (0);
        auto* R = block.getNumChannels() > 1 ? block.getChannelPointer (1) : L;
        dry.copyFrom (block);

        for (int i = 0; i < ns; ++i)
        {
            const float in = 0.5f * (L[i] + R[i]);
            float wl = 0.0f;
            float wr = 0.0f;
            for (int v = 0; v < voices; ++v)
            {
                lfoPhase[v] += (0.15f + 0.05f * static_cast<float>(v) + det * 0.4f) / (float) sr;
                if (lfoPhase[v] >= 1.0f) lfoPhase[v] -= 1.0f;
                const float mod = fastmath::sinTurns (lfoPhase[v]);
                const float dms = 8.0f + det * 12.0f + mod * (2.0f + det * 6.0f);   // ms
                line[v].setDelay (juce::jmax (1.0f, (float) (dms * 0.001 * sr)));
                line[v].pushSample (0, in);
                const float s = line[v].popSample (0);
                const float panv = (voices > 1) ? ((float) v / static_cast<float>(voices - 1) * 2.0f - 1.0f) * width : 0.0f;
                wl += s * (0.5f - 0.5f * panv);
                wr += s * (0.5f + 0.5f * panv);
            }
            const float g = 1.0f / std::sqrt ((float) voices);
            L[i] = wl * g; R[i] = wr * g;
        }
        blend (block, dry.buf, mix);
    }

private:
    double sr = 44100.0; int voices = 4;
    float det = 0.3f;
    float width = 0.5f;
    float mix = 1.0f;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> line[8];
    float lfoPhase[8] = { 0,0,0,0,0,0,0,0 };
    DryStore dry;
};

//==============================================================================
class Distort
{
public:
    Distort() : os (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR) {}

    void prepare (const juce::dsp::ProcessSpec& s)
    {
        sr = s.sampleRate; dry.prepare (s);
        os.initProcessing (s.maximumBlockSize);
        juce::dsp::ProcessSpec ms { s.sampleRate, s.maximumBlockSize, s.numChannels };
        tone.prepare (ms); tone.reset();
        lastTone = 0.5f; applyTone (lastTone);   // prime coefficients off the audio thread
    }
    void reset() { os.reset(); tone.reset(); }
    void setParams (float drive_, float toneAmt, float mix_, float out_, int mode_)
    { drive = drive_; mix = mix_; outGain = out_; mode = mode_;
      // Recompute the tone shelf only when it actually changes: each make*() call
      // heap-allocates, which is unsafe on the audio thread (caught by RTSan).
      if (toneAmt != lastTone) { lastTone = toneAmt; applyTone (toneAmt); } }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        dry.copyFrom (block);
        const float pre = 1.0f + drive * 24.0f;

        auto up = os.processSamplesUp (block);
        const int nc = (int) up.getNumChannels();
        const int ns = (int) up.getNumSamples();
        for (int c = 0; c < nc; ++c)
        {
            auto* d = up.getChannelPointer ((size_t) c);
            for (int i = 0; i < ns; ++i) d[i] = shape (d[i] * pre);
        }
        os.processSamplesDown (block);

        juce::dsp::ProcessContextReplacing<float> ctx (block);
        tone.process (ctx);

        block.multiplyBy (outGain);
        blend (block, dry.buf, mix);
    }

private:
    float shape (float x) const
    {
        switch (mode)
        {
            case 1: return x > 0.0f ? fastmath::tanh (x) : 0.6f * fastmath::tanh (x);   // diode (asym)
            case 2: { float y = x; while (y > 1.0f) y = 2.0f - y; while (y < -1.0f) y = -2.0f - y; return y; } // fold
            default: return fastmath::tanh (x);                                // tube
        }
    }
    void applyTone (float toneAmt)
    { *tone.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 2500.0f, 0.707f, juce::Decibels::decibelsToGain ((toneAmt - 0.5f) * 18.0f)); }

    double sr = 44100.0; int mode = 0;
    float drive = 0.3f;
    float mix = 1.0f;
    float outGain = 0.8f;
    float lastTone = 0.5f;
    juce::dsp::Oversampling<float> os;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> tone;
    DryStore dry;
};

//==============================================================================
class Flanger
{
public:
    void prepare (const juce::dsp::ProcessSpec& s)
    {
        sr = s.sampleRate; dry.prepare (s);
        line.setMaximumDelayInSamples ((int) (sr * 0.02) + 4);
        line.prepare (s); line.reset();
        phase = 0.0f;
    }
    void reset() { line.reset(); phase = 0.0f; }
    void setParams (float rateHz, float depth_, float fb_, float mix_)
    { rate = rateHz; depth = depth_; fb = fb_ * 0.95f; mix = mix_; }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        const int ns = (int) block.getNumSamples();
        const int nc = (int) block.getNumChannels();
        dry.copyFrom (block);
        for (int i = 0; i < ns; ++i)
        {
            phase += rate / (float) sr; if (phase >= 1.0f) phase -= 1.0f;
            const float mod = 0.5f + 0.5f * fastmath::sinTurns (phase);
            const float dly = juce::jmax (1.0f, (float) ((0.5 + (0.5 + depth * 6.5) * mod) * 0.001 * sr));
            line.setDelay (dly);
            for (int c = 0; c < nc; ++c)
            {
                auto* x = block.getChannelPointer ((size_t) c);
                const float dl = line.popSample (c);
                line.pushSample (c, x[i] + dl * fb);
                x[i] = x[i] + dl;
            }
        }
        blend (block, dry.buf, mix);
    }

private:
    double sr = 44100.0;
    float rate = 0.5f;
    float depth = 0.5f;
    float fb = 0.3f;
    float mix = 0.5f;
    float phase = 0.0f;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> line;
    DryStore dry;
};

//==============================================================================
class Phaser
{
public:
    void prepare (const juce::dsp::ProcessSpec& s) { ph.prepare (s); }
    void reset() { ph.reset(); }
    void setParams (float rateHz, float depth_, float mix_)
    { ph.setRate (rateHz); ph.setDepth (depth_); ph.setCentreFrequency (600.0f); ph.setFeedback (0.5f); ph.setMix (mix_); }
    void process (juce::dsp::AudioBlock<float>& block) { juce::dsp::ProcessContextReplacing<float> c (block); ph.process (c); }
private:
    juce::dsp::Phaser<float> ph;
};

//==============================================================================
class Chorus
{
public:
    void prepare (const juce::dsp::ProcessSpec& s) { ch.prepare (s); }
    void reset() { ch.reset(); }
    void setParams (float rateHz, float depth_, float mix_)
    { ch.setRate (rateHz); ch.setDepth (depth_); ch.setCentreDelay (12.0f); ch.setFeedback (0.2f); ch.setMix (mix_); }
    void process (juce::dsp::AudioBlock<float>& block) { juce::dsp::ProcessContextReplacing<float> c (block); ch.process (c); }
private:
    juce::dsp::Chorus<float> ch;
};

//==============================================================================
class Delay
{
public:
    void prepare (const juce::dsp::ProcessSpec& s)
    {
        sr = s.sampleRate; dry.prepare (s);
        line.setMaximumDelayInSamples ((int) (sr * 2.1) + 8);
        line.prepare (s); line.reset();
    }
    void reset() { line.reset(); }
    void setParams (float timeMs, float fb_, float width_, float mix_)
    { timeSamps = juce::jmax (1.0f, (float) (timeMs * 0.001 * sr)); fb = fb_ * 0.95f; width = width_; mix = mix_; }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        const int ns = (int) block.getNumSamples();
        const int nc = (int) block.getNumChannels();
        dry.copyFrom (block);
        auto* L = block.getChannelPointer (0);
        auto* R = nc > 1 ? block.getChannelPointer (1) : L;
        line.setDelay (timeSamps);
        for (int i = 0; i < ns; ++i)
        {
            const float dL = line.popSample (0);
            const float dR = nc > 1 ? line.popSample (1) : dL;
            // ping-pong cross feedback; width controls the cross amount
            line.pushSample (0, L[i] + dR * fb);
            if (nc > 1) line.pushSample (1, R[i] + dL * fb);
            L[i] = L[i] + dL * (1.0f - 0.5f * width) + dR * (0.5f * width);
            if (nc > 1) R[i] = R[i] + dR * (1.0f - 0.5f * width) + dL * (0.5f * width);
        }
        blend (block, dry.buf, mix);
    }

private:
    double sr = 44100.0;
    float timeSamps = 14400.0f;
    float fb = 0.35f;
    float width = 0.5f;
    float mix = 0.3f;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> line;
    DryStore dry;
};

//==============================================================================
class Reverb
{
public:
    void prepare (const juce::dsp::ProcessSpec& s) { rv.prepare (s); }
    void reset() { rv.reset(); }
    void setParams (float size, float decay, float damp, float mix)
    {
        juce::dsp::Reverb::Parameters pr;
        pr.roomSize   = juce::jlimit (0.0f, 1.0f, size * 0.6f + decay * 0.4f);
        pr.damping    = damp;
        pr.wetLevel   = mix;
        pr.dryLevel   = 1.0f - mix;
        pr.width      = 1.0f;
        pr.freezeMode = 0.0f;
        rv.setParameters (pr);
    }
    void process (juce::dsp::AudioBlock<float>& block) { juce::dsp::ProcessContextReplacing<float> c (block); rv.process (c); }
private:
    juce::dsp::Reverb rv;
};

//==============================================================================
class Comp
{
public:
    void prepare (const juce::dsp::ProcessSpec& s) { comp.prepare (s); makeup.prepare (s); makeup.setRampDurationSeconds (0.02); }
    void reset() { comp.reset(); }
    void setParams (float threshDb, float ratio, float attackMs, float makeupDb)
    { comp.setThreshold (threshDb); comp.setRatio (juce::jmax (1.0f, ratio)); comp.setAttack (attackMs); comp.setRelease (120.0f);
      makeup.setGainDecibels (makeupDb); }
    void process (juce::dsp::AudioBlock<float>& block)
    { juce::dsp::ProcessContextReplacing<float> c (block); comp.process (c); makeup.process (c); }
private:
    juce::dsp::Compressor<float> comp;
    juce::dsp::Gain<float> makeup;
};

//==============================================================================
class Eq   // fixed-frequency 4-band: low shelf / 2 peaks / high shelf
{
public:
    void prepare (const juce::dsp::ProcessSpec& s)
    {
        sr = s.sampleRate;
        chain.prepare (s);
        // prime all bands flat,
        lastLow   = 0.0f;
        lastLoMid = 0.0f;
        lastHiMid = 0.0f;
        lastHigh  = 0.0f;
        applyLow (0.0f); applyLoMid (0.0f); applyHiMid (0.0f); applyHigh (0.0f);  // off the audio thread
    }
    void reset() { chain.reset(); }
    void setParams (float lowDb, float loMidDb, float hiMidDb, float highDb)
    {
        // Recompute a band only when its gain changes: each make*() call heap-allocates,
        // which is unsafe on the audio thread (caught by RTSan).
        if (lowDb   != lastLow)   { lastLow   = lowDb;   applyLow   (lowDb); }
        if (loMidDb != lastLoMid) { lastLoMid = loMidDb; applyLoMid (loMidDb); }
        if (hiMidDb != lastHiMid) { lastHiMid = hiMidDb; applyHiMid (hiMidDb); }
        if (highDb  != lastHigh)  { lastHigh  = highDb;  applyHigh  (highDb); }
    }
    void process (juce::dsp::AudioBlock<float>& block) { juce::dsp::ProcessContextReplacing<float> c (block); chain.process (c); }
private:
    using Coef = juce::dsp::IIR::Coefficients<float>;
    void applyLow   (float dB) { *chain.get<0>().state = *Coef::makeLowShelf   (sr, 120.0f,  0.707f, juce::Decibels::decibelsToGain (dB)); }
    void applyLoMid (float dB) { *chain.get<1>().state = *Coef::makePeakFilter (sr, 500.0f,  0.9f,   juce::Decibels::decibelsToGain (dB)); }
    void applyHiMid (float dB) { *chain.get<2>().state = *Coef::makePeakFilter (sr, 3000.0f, 0.9f,   juce::Decibels::decibelsToGain (dB)); }
    void applyHigh  (float dB) { *chain.get<3>().state = *Coef::makeHighShelf  (sr, 8000.0f, 0.707f, juce::Decibels::decibelsToGain (dB)); }

    using Dup = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    double sr = 44100.0;
    float lastLow = 0.0f;
    float lastLoMid = 0.0f;
    float lastHiMid = 0.0f;
    float lastHigh = 0.0f;
    juce::dsp::ProcessorChain<Dup, Dup, Dup, Dup> chain;
};

//==============================================================================
class FilterFx   // reuses the multimode filter as an FX
{
public:
    void prepare (const juce::dsp::ProcessSpec& s) { dry.prepare (s); filt.prepare (s.sampleRate); }
    void reset() { filt.reset(); }
    void setParams (float cutoffHz, float reso, int type /*0 LP,1 HP,2 BP*/, float mix_)
    {
        const int inner = (type == 2) ? MultimodeFilter::BP12 : MultimodeFilter::LP12;
        const int t = (type == 1) ? MultimodeFilter::HP12 : inner;
        filt.setParams (t, cutoffHz, reso, 0.0f);
        mix = mix_;
    }
    void process (juce::dsp::AudioBlock<float>& block)
    {
        const int ns = (int) block.getNumSamples();
        const int nc = (int) block.getNumChannels();
        dry.copyFrom (block);
        for (int c = 0; c < nc; ++c)
        {
            auto* x = block.getChannelPointer ((size_t) c);
            for (int i = 0; i < ns; ++i) x[i] = filt.processSample (juce::jmin (c, 1), x[i]);
        }
        blend (block, dry.buf, mix);
    }
private:
    MultimodeFilter filt; float mix = 1.0f; DryStore dry;
};

} // namespace zw::fx
