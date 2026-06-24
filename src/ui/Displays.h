#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "ZWLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../Parameters.h"
#include <cmath>

namespace zw
{

//==============================================================================
struct AnimatedComponent : public juce::Component, private juce::Timer
{
    AnimatedComponent() { startTimerHz (30); }
    ~AnimatedComponent() override { stopTimer(); }
    void timerCallback() override { phase += 0.02f; if (phase > 1000.0f) phase = 0.0f; repaint(); }
    float phase = 0.0f;
};

inline void wellBackground (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (theme::well);
    g.fillRoundedRectangle (r, (float) theme::rWell);
    g.setColour (theme::wa (0.06f));
    g.drawRoundedRectangle (r, (float) theme::rWell, 1.0f);
}

//==============================================================================
// 3D-style stack of wavetable frames; current frame highlighted white.
class WavetableDisplay : public AnimatedComponent
{
public:
    WavetableDisplay (juce::AudioProcessorValueTreeState& s, juce::String wtId, juce::String warpId)
    {
        wt   = s.getRawParameterValue (wtId);
        warp = s.getRawParameterValue (warpId);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        wellBackground (g, r);
        auto area = r.reduced (14.0f);

        const float wtPos = wt != nullptr ? wt->load() : 0.3f;
        const float wp    = warp != nullptr ? warp->load() : 0.0f;
        const int   K = 15;
        const int   cur = juce::roundToInt (wtPos * (K - 1));
        const int   N = 64;

        for (int k = 0; k < K; ++k)
        {
            const float d = (float) k / (K - 1);
            const float ox = (k - cur) * 7.0f;
            const float oy = (cur - k) * 8.2f + std::sin (phase + (float) k * 0.3f) * 1.5f;
            const float opacity = juce::jmax (0.10f, 0.55f - std::abs (k - cur) * 0.045f);
            const bool  isCur = (k == cur);

            juce::Path path;
            for (int i = 0; i <= N; ++i)
            {
                const float x = (float) i / N;
                float ph = x;                              // warp phase-bend
                if (wp > 0.001f) { const float piv = 0.5f + 0.49f * wp;
                    ph = (x < piv) ? (0.5f * x / piv) : (0.5f + 0.5f * (x - piv) / (1.0f - piv)); }
                const int H = 1 + (int) (d * 16);
                float y = 0.0f;
                for (int n = 1; n <= H; ++n) y += (1.0f / n) * std::sin (juce::MathConstants<float>::twoPi * n * ph);
                y *= 0.5f;
                const float px = area.getX() + ox + x * area.getWidth() * 0.82f;
                const float py = area.getCentreY() + oy - y * area.getHeight() * 0.22f;
                if (i == 0) path.startNewSubPath (px, py); else path.lineTo (px, py);
            }
            const auto col = isCur ? juce::Colours::white : theme::spectrum (d).withAlpha (opacity);
            if (isCur) ZWLookAndFeel::glowPath (g, path, col, 2.2f, 2.0f);
            else { g.setColour (col); g.strokePath (path, juce::PathStrokeType (1.4f)); }
        }
    }

private:
    std::atomic<float> *wt = nullptr, *warp = nullptr;
};

//==============================================================================
class FilterResponse : public AnimatedComponent
{
public:
    explicit FilterResponse (juce::AudioProcessorValueTreeState& s)
    {
        cut  = s.getRawParameterValue (id::filterCutoff);
        res  = s.getRawParameterValue (id::filterReso);
        type = s.getRawParameterValue (id::filterType);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        wellBackground (g, r);
        auto a = r.reduced (10.0f);

        g.setColour (theme::wa (0.05f));
        for (int i = 1; i < 6; ++i) { const float x = a.getX() + a.getWidth() * i / 6.0f; g.drawVerticalLine ((int) x, a.getY(), a.getBottom()); }

        const float fc   = cut != nullptr ? cut->load() : 2000.0f;
        const float reso = res != nullptr ? res->load() : 0.26f;
        const int   t    = type != nullptr ? (int) type->load() : 0;
        const float order = (t == 0 || t == 2) ? 4.0f : 2.0f;     // 24 vs 12 dB
        const float fcNorm = std::log (fc / 20.0f) / std::log (1000.0f);   // 0..1

        juce::Path curve;
        const int N = 90;
        for (int i = 0; i <= N; ++i)
        {
            const float xn = (float) i / N;
            const float f  = 20.0f * std::pow (1000.0f, xn);
            const float ratio = f / fc;
            float mag = 1.0f;
            if (t == 5)        mag = std::abs (1.0f - 1.0f / (1.0f + std::pow ((ratio - 1.0f) * 8.0f, 2.0f)));   // notch
            else if (t == 4)   mag = 1.0f / std::sqrt (1.0f + std::pow ((std::log (ratio + 1e-6f)) * 2.5f, 2.0f)); // bandpass
            else if (t == 1 || t == 0) mag = 1.0f / std::sqrt (1.0f + std::pow (ratio, 2.0f * order));            // lowpass
            else               mag = 1.0f / std::sqrt (1.0f + std::pow (1.0f / juce::jmax (1e-4f, ratio), 2.0f * order)); // highpass
            const float bump = 1.0f + reso * 3.5f * std::exp (-std::pow ((xn - fcNorm) * 14.0f, 2.0f));
            mag *= bump;
            const float db = juce::jlimit (-1.0f, 1.2f, std::log10 (juce::jmax (1e-3f, mag)));
            const float y  = a.getCentreY() - db * a.getHeight() * 0.42f;
            const float x  = a.getX() + xn * a.getWidth();
            if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }
        ZWLookAndFeel::glowPath (g, curve, theme::accent, 2.0f, 2.0f);

        const float cx = a.getX() + fcNorm * a.getWidth();
        g.setColour (theme::accent.withAlpha (0.5f));
        g.drawVerticalLine ((int) cx, a.getY(), a.getBottom());
    }

private:
    std::atomic<float> *cut = nullptr, *res = nullptr, *type = nullptr;
};

//==============================================================================
class AdsrDisplay : public AnimatedComponent
{
public:
    AdsrDisplay (juce::AudioProcessorValueTreeState& s, int envIndex)
    {
        a = s.getRawParameterValue (id::env (envIndex, "attack"));
        d = s.getRawParameterValue (id::env (envIndex, "decay"));
        sus = s.getRawParameterValue (id::env (envIndex, "sustain"));
        rel = s.getRawParameterValue (id::env (envIndex, "release"));
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        wellBackground (g, r);
        auto bx = r.reduced (10.0f);

        auto norm = [] (std::atomic<float>* p, float dflt) { return p ? juce::jlimit (0.0f, 1.0f, p->load() / 8.0f) : dflt; };
        const float an = norm (a, 0.02f), dn = norm (d, 0.05f), rn = norm (rel, 0.08f);
        const float sn = sus ? sus->load() : 0.7f;

        const float yTop = bx.getY(), yBot = bx.getBottom();
        const float x0 = bx.getX();
        const float ax = x0 + an * bx.getWidth() * 0.3f;
        const float dx = ax + dn * bx.getWidth() * 0.3f;
        const float sy = yBot - sn * (yBot - yTop);
        const float sx = bx.getX() + bx.getWidth() * 0.72f;
        const float ex = sx + rn * bx.getWidth() * 0.28f;

        juce::Path p;
        p.startNewSubPath (x0, yBot);
        p.lineTo (ax, yTop);
        p.lineTo (dx, sy);
        p.lineTo (sx, sy);
        p.lineTo (juce::jmin (ex, bx.getRight()), yBot);
        ZWLookAndFeel::glowPath (g, p, theme::cyan, 2.0f, 1.6f);

        for (auto pt : { juce::Point<float> (ax, yTop), { dx, sy }, { sx, sy }, { juce::jmin (ex, bx.getRight()), yBot } })
        {
            g.setColour (juce::Colours::white);
            g.fillEllipse (pt.x - 3.0f, pt.y - 3.0f, 6.0f, 6.0f);
        }
    }

private:
    std::atomic<float> *a = nullptr, *d = nullptr, *sus = nullptr, *rel = nullptr;
};

//==============================================================================
class Scope : public AnimatedComponent
{
public:
    explicit Scope (ZandersWaveAudioProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        wellBackground (g, r);
        auto a = r.reduced (6.0f);

        const float* ring = proc.getScopeRing();
        const int    wp   = proc.getScopeWritePos();
        const int    N    = juce::jmin (512, ZandersWaveAudioProcessor::kScopeSize);

        juce::Path path;
        for (int i = 0; i < N; ++i)
        {
            const int idx = (wp - N + i) & (ZandersWaveAudioProcessor::kScopeSize - 1);
            const float s = ring[(size_t) ((idx + ZandersWaveAudioProcessor::kScopeSize) & (ZandersWaveAudioProcessor::kScopeSize - 1))];
            const float x = a.getX() + a.getWidth() * (float) i / (N - 1);
            const float y = a.getCentreY() - juce::jlimit (-1.0f, 1.0f, s) * a.getHeight() * 0.46f;
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
        ZWLookAndFeel::glowPath (g, path, theme::spectrum (0.5f), 1.6f, 1.4f);
    }
private:
    ZandersWaveAudioProcessor& proc;
};

//==============================================================================
class LevelMeter : public AnimatedComponent
{
public:
    explicit LevelMeter (ZandersWaveAudioProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override
    {
        auto a = getLocalBounds().toFloat();
        g.setColour (theme::well);
        g.fillRoundedRectangle (a, 4.0f);
        const float peak = juce::jlimit (0.0f, 1.0f, proc.getOutputPeak());
        auto fill = a.reduced (2.0f).withWidth ((a.getWidth() - 4.0f) * peak);
        juce::ColourGradient grad (theme::cyan, a.getX(), 0, theme::amber, a.getRight(), 0, false);
        grad.addColour (0.6, theme::pink);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, 3.0f);
    }
private:
    ZandersWaveAudioProcessor& proc;
};

} // namespace zw
