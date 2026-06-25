#pragma once

#include <cmath>
#include "FastMath.h"

namespace zw
{

//==============================================================================
// Stereo multimode filter built on a TPT state-variable core (Zavalishin).
// One core = 12 dB/oct; 24 dB types cascade two cores. Provides LP/HP/BP/Notch.
// Type indices match the "filter_type" choice order: LP24,LP12,HP24,HP12,BP12,Notch.
//==============================================================================
class MultimodeFilter
{
public:
    enum Type { LP24 = 0, LP12, HP24, HP12, BP12, Notch };

    void prepare (double sr) noexcept { sampleRate = sr; reset(); }

    void reset() noexcept
    {
        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < 2; ++s)
            {
                ic1[c][s] = 0.0f;
                ic2[c][s] = 0.0f;
            }
    }

    // cutoffHz, resonance 0..1, drive 0..1. Call once per block.
    void setParams (int type_, float cutoffHz, float resonance, float drive_) noexcept
    {
        type = type_;
        const float fc = clampf (cutoffHz, 20.0f, (float) (sampleRate * 0.49));
        const float g  = std::tan (3.14159265358979f * fc / (float) sampleRate);
        const float Q  = 0.5f + resonance * resonance * 24.0f;   // ~0.5 .. ~24.5
        k  = 1.0f / Q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;

        driveGain = 1.0f + drive_ * 3.0f;          // up to ~4x into the saturator
        driveComp = 1.0f / std::sqrt (driveGain);  // rough loudness compensation
    }

    float processSample (int ch, float x) noexcept
    {
        if (driveGain > 1.0001f)
            x = fastmath::tanh (x * driveGain) * driveComp;

        switch (type)
        {
            case LP12:  return core (ch, 0, x).lp;
            case HP12:  return core (ch, 0, x).hp;
            case BP12:  return core (ch, 0, x).bp;
            case Notch: return core (ch, 0, x).notch;
            case LP24:  return core (ch, 1, core (ch, 0, x).lp).lp;
            case HP24:  return core (ch, 1, core (ch, 0, x).hp).hp;
            default:    return core (ch, 0, x).lp;
        }
    }

private:
    struct Out { float lp; float bp; float hp; float notch; };

    Out core (int ch, int stage, float v0) noexcept
    {
        const float v3 = v0 - ic2[ch][stage];
        const float v1 = a1 * ic1[ch][stage] + a2 * v3;
        const float v2 = ic2[ch][stage] + a2 * ic1[ch][stage] + a3 * v3;
        ic1[ch][stage] = 2.0f * v1 - ic1[ch][stage];
        ic2[ch][stage] = 2.0f * v2 - ic2[ch][stage];

        Out o;
        o.lp    = v2;
        o.bp    = v1;
        o.hp    = v0 - k * v1 - v2;
        o.notch = v0 - k * v1;
        return o;
    }

    static float clampf (float v, float lo, float hi) noexcept
    {
        const float high = v > hi ? hi : v;
        return v < lo ? lo : high;
    }

    double sampleRate = 44100.0;
    int    type = LP24;
    float  a1 = 0;
    float  a2 = 0;
    float  a3 = 0;
    float  k = 1.0f;
    float  driveGain = 1.0f;
    float  driveComp = 1.0f;
    float  ic1[2][2] = { { 0, 0 }, { 0, 0 } };
    float  ic2[2][2] = { { 0, 0 }, { 0, 0 } };
};

} // namespace zw
