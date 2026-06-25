#pragma once

#include <JuceHeader.h>
#include <cmath>

// Cheap, audio-grade approximations for the transcendental functions that run on
// the per-sample hot path (filter drive, sub oscillator, FX). They trade a tiny,
// inaudible amount of accuracy for speed. Per-block math (coefficient setup, etc.)
// should keep using the exact std:: functions.
namespace zw::fastmath
{
    // tanh saturator. juce::dsp::FastMathApproximations::tanh is a Pade approximant
    // that is only well-behaved on a limited domain, so clamp first — tanh is within
    // ~1e-3 of +/-1 once |x| > 4, so clamping there is inaudible and keeps it stable.
    inline float tanh (float x) noexcept
    {
        x = juce::jlimit (-4.0f, 4.0f, x);
        return juce::dsp::FastMathApproximations::tanh (x);
    }

    // sin (2*pi*turns) for a phase expressed in turns. Wraps the argument into the
    // approximation's valid [-pi, pi] range first, so it is correct for any input.
    inline float sinTurns (float turns) noexcept
    {
        const float t = turns - std::floor (turns + 0.5f);   // -> [-0.5, 0.5)
        return juce::dsp::FastMathApproximations::sin (t * juce::MathConstants<float>::twoPi);
    }
}
