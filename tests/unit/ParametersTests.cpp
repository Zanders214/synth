// Unit tests for zw parameter layout / id scheme (src/Parameters.{h,cpp}).
#include <juce_core/juce_core.h>
#include "Parameters.h"
#include "TestHelpers.h"

#include <array>
#include <cmath>

namespace
{
struct ParametersTests : juce::UnitTest
{
    ParametersTests() : juce::UnitTest ("Parameters", "DSP") {}

    void runTest() override
    {
        //======================================================================
        beginTest ("id helpers produce the documented strings");
        {
            // osc(ab,name): "osc" + AB + "_" + p
            expectEquals (zw::id::osc ('A', "enable"), juce::String ("oscA_enable"));
            expectEquals (zw::id::osc ('B', "wtpos"),  juce::String ("oscB_wtpos"));
            expectEquals (zw::id::osc ('A', "fine"),   juce::String ("oscA_fine"));

            // env(n,name): "env" + n + "_" + p
            expectEquals (zw::id::env (1, "attack"),  juce::String ("env1_attack"));
            expectEquals (zw::id::env (3, "release"), juce::String ("env3_release"));

            // lfo(n,name): "lfo" + n + "_" + p
            expectEquals (zw::id::lfo (1, "shape"),  juce::String ("lfo1_shape"));
            expectEquals (zw::id::lfo (4, "ratehz"), juce::String ("lfo4_ratehz"));

            // macro(n): "macro" + n
            expectEquals (zw::id::macro (1), juce::String ("macro1"));
            expectEquals (zw::id::macro (4), juce::String ("macro4"));

            // fx(slot,p): "fx_" + slot + "_" + p
            expectEquals (zw::id::fx ("hyper", "mix"),  juce::String ("fx_hyper_mix"));
            expectEquals (zw::id::fx ("delay", "time"), juce::String ("fx_delay_time"));

            // arpStep(n): "arp_step" + n
            expectEquals (zw::id::arpStep (1),  juce::String ("arp_step1"));
            expectEquals (zw::id::arpStep (16), juce::String ("arp_step16"));
        }

        //======================================================================
        beginTest ("singleton id constants match their literal values");
        {
            expectEquals (juce::String (zw::id::masterOut),    juce::String ("masterOut"));
            expectEquals (juce::String (zw::id::subEnable),    juce::String ("sub_enable"));
            expectEquals (juce::String (zw::id::filterCutoff), juce::String ("filter_cutoff"));
            expectEquals (juce::String (zw::id::arpEnable),    juce::String ("arp_enable"));
            expectEquals (juce::String (zw::id::glidePoly),    juce::String ("global_polyphony"));
            expectEquals (juce::String (zw::id::mpeEnable),    juce::String ("global_mpe"));
        }

        //======================================================================
        beginTest ("choice option lists have the expected order and size");
        {
            const auto ft = zw::choices::filterType();
            expectEquals (ft.size(), 6);
            expectEquals (ft[0], juce::String ("LP24"));
            expectEquals (ft[5], juce::String ("Notch"));

            expectEquals (zw::choices::subWave().size(), 3);
            expectEquals (zw::choices::noiseType().size(), 3);
            expectEquals (zw::choices::lfoShape().size(), 6);
            expectEquals (zw::choices::lfoMode().size(), 3);
            expectEquals (zw::choices::syncDiv().size(), 6);
            expectEquals (zw::choices::arpRate().size(), 6);
            expectEquals (zw::choices::arpMode().size(), 6);
            expectEquals (zw::choices::monoMode().size(), 3);
            expectEquals (zw::choices::distortMode().size(), 3);
            expectEquals (zw::choices::fxFilterType().size(), 3);

            expectEquals (zw::choices::arpMode()[0], juce::String ("Up"));
            expectEquals (zw::choices::lfoShape()[0], juce::String ("Sine"));
            expectEquals (zw::choices::fxFilterType()[2], juce::String ("BP"));
        }

        //======================================================================
        beginTest ("makeLogRange maps endpoints and midpoint as Hz = lo*(hi/lo)^x");
        {
            auto r = zw::makeLogRange (20.0f, 20000.0f);
            // convertFrom0to1 == lo * (hi/lo)^x
            expectWithinAbsoluteError (r.convertFrom0to1 (0.0f), 20.0f,    1.0e-3f);
            expectWithinAbsoluteError (r.convertFrom0to1 (1.0f), 20000.0f, 1.0e-1f);
            // midpoint: 20 * 1000^0.5 = 20 * sqrt(1000)
            const float mid = 20.0f * std::sqrt (1000.0f);
            expectWithinAbsoluteError (r.convertFrom0to1 (0.5f), mid, 1.0e-1f);

            // Round trip: norm -> value -> norm.
            for (float x : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
            {
                const float hz = r.convertFrom0to1 (x);
                expect (std::isfinite (hz));
                expectWithinAbsoluteError (r.convertTo0to1 (hz), x, 1.0e-4f);
            }

            // Monotonic increasing.
            expectGreaterThan (r.convertFrom0to1 (1.0f), r.convertFrom0to1 (0.0f));
            expectGreaterThan (r.convertFrom0to1 (0.5f), r.convertFrom0to1 (0.25f));

            // A second range with different bounds also honours the endpoints.
            auto r2 = zw::makeLogRange (0.01f, 40.0f);
            expectWithinAbsoluteError (r2.convertFrom0to1 (0.0f), 0.01f, 1.0e-4f);
            expectWithinAbsoluteError (r2.convertFrom0to1 (1.0f), 40.0f, 1.0e-3f);
        }

        //======================================================================
        beginTest ("layout builds and a representative set of parameters exist");
        {
            zwtest::DummyProcessor proc;
            auto& s = proc.apvts;

            const char* expectedIds[] = {
                "oscA_enable", "oscA_wtpos", "oscA_warp", "oscA_warpmode",
                "oscA_unison", "oscA_detune", "oscA_level", "oscA_pan",
                "oscA_phase", "oscA_octave", "oscA_coarse", "oscA_fine",
                "oscB_enable", "oscB_wtpos",
                "sub_enable", "sub_wave", "sub_octave", "sub_saturate", "sub_level",
                "noise_enable", "noise_type", "noise_color", "noise_level",
                "filter_enable", "filter_type", "filter_cutoff", "filter_reso",
                "filter_drive", "filter_mix",
                "filter_routeA", "filter_routeB", "filter_routeS", "filter_routeN",
                "env1_attack", "env1_decay", "env1_sustain", "env1_release",
                "env3_release",
                "lfo1_shape", "lfo1_sync", "lfo1_ratehz", "lfo1_ratediv",
                "lfo1_depth", "lfo1_rise", "lfo1_phase", "lfo1_mode",
                "lfo4_shape",
                "macro1", "macro2", "macro3", "macro4",
                "masterOut",
                "arp_enable", "arp_rate", "arp_mode", "arp_octaves",
                "arp_gate", "arp_swing", "arp_step1", "arp_step16",
                "fx_hyper_enable", "fx_hyper_detune", "fx_hyper_voices",
                "fx_distort_mode", "fx_flanger_rate", "fx_phaser_stages",
                "fx_chorus_voices", "fx_delay_time", "fx_reverb_size",
                "fx_comp_threshold", "fx_eq_low", "fx_filter_type",
                "global_polyphony", "global_monomode", "global_glide",
                "global_bendrange", "global_mpe"
            };

            for (const auto* idStr : expectedIds)
                expect (s.getParameter (idStr) != nullptr,
                        juce::String ("missing parameter: ") + idStr);

            // A clearly-bogus id must NOT exist.
            expect (s.getParameter ("does_not_exist") == nullptr);
        }

        //======================================================================
        beginTest ("default values match the layout definitions");
        {
            zwtest::DummyProcessor proc;
            auto& s = proc.apvts;

            // Macro defaults: {0.50, 0.20, 0.00, 0.74}.
            const std::array<float, 4> macroDef { 0.50f, 0.20f, 0.00f, 0.74f };
            for (int i = 1; i <= 4; ++i)
                expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::macro (i)),
                                           macroDef[(size_t) (i - 1)], 1.0e-3f);

            // Arp step defaults: {1,0,1,1,0,1,1,0,1,0,1,1,0,1,0,1} as bool 0/1.
            const std::array<int, 16> stepDef { 1,0,1,1,0,1,1,0,1,0,1,1,0,1,0,1 };
            for (int i = 1; i <= 16; ++i)
                expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::arpStep (i)),
                                           (float) stepDef[(size_t) (i - 1)], 1.0e-3f);

            // Spot-check a representative spread of other declared defaults.
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::osc ('A', "level")),  0.82f,   1.0e-3f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::osc ('A', "wtpos")),  0.30f,   1.0e-3f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::osc ('B', "level")),  0.00f,   1.0e-3f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::osc ('A', "detune")), 18.0f,   1.0e-2f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::filterCutoff),        2193.0f, 1.0f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::filterMix),           1.0f,    1.0e-3f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::masterOut),           0.80f,   1.0e-3f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::subLevel),            0.46f,   1.0e-3f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::subSaturate),         0.22f,   1.0e-3f);

            // Bool defaults (raw value is 0/1).
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::osc ('A', "enable")), 1.0f, 1.0e-3f); // enabled
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::subEnable),           1.0f, 1.0e-3f); // enabled
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::noiseEnable),         0.0f, 1.0e-3f); // disabled
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::arpEnable),           0.0f, 1.0e-3f); // disabled
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::mpeEnable),           0.0f, 1.0e-3f); // disabled

            // Int defaults.
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::osc ('A', "unison")), 4.0f,  1.0e-3f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::glidePoly),           16.0f, 1.0e-3f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::bendRange),           2.0f,  1.0e-3f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::subOctave),           -1.0f, 1.0e-3f);

            // Choice defaults: filter type index 0 -> "LP24".
            if (auto* fp = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (zw::id::filterType)))
            {
                expectEquals (fp->getIndex(), 0);
                expectEquals (fp->getCurrentChoiceName(), juce::String ("LP24"));
            }
            else
            {
                expect (false, "filter type should be a choice parameter");
            }

            // LFO div default index 3 -> "1/8".
            if (auto* dv = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (zw::id::lfo (1, "ratediv"))))
                expectEquals (dv->getIndex(), 3);
            else
                expect (false, "lfo ratediv should be a choice parameter");

            // Arp rate default index 3 -> "1/16".
            if (auto* ar = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (zw::id::arpRate)))
                expectEquals (ar->getCurrentChoiceName(), juce::String ("1/16"));
            else
                expect (false, "arp rate should be a choice parameter");
        }

        //======================================================================
        beginTest ("float ranges clamp at their declared extremes");
        {
            zwtest::DummyProcessor proc;
            auto& s = proc.apvts;

            // Cutoff: makeLogRange(20, 20000). norm 0 -> 20 Hz, norm 1 -> 20000 Hz.
            zwtest::setParam (s, zw::id::filterCutoff, 0.0f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::filterCutoff), 20.0f, 1.0e-1f);
            zwtest::setParam (s, zw::id::filterCutoff, 1.0f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::filterCutoff), 20000.0f, 1.0e-1f);

            // Detune: linear range [0, 100] ct.
            zwtest::setParam (s, zw::id::osc ('A', "detune"), 0.0f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::osc ('A', "detune")), 0.0f, 1.0e-2f);
            zwtest::setParam (s, zw::id::osc ('A', "detune"), 1.0f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::osc ('A', "detune")), 100.0f, 1.0e-2f);

            // Osc pan: linear range [-1, 1].
            zwtest::setParam (s, zw::id::osc ('A', "pan"), 0.0f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::osc ('A', "pan")), -1.0f, 1.0e-2f);
            zwtest::setParam (s, zw::id::osc ('A', "pan"), 1.0f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::osc ('A', "pan")), 1.0f, 1.0e-2f);

            // Percent params clamp to [0, 1].
            zwtest::setParam (s, zw::id::masterOut, 0.0f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::masterOut), 0.0f, 1.0e-3f);
            zwtest::setParam (s, zw::id::masterOut, 1.0f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::masterOut), 1.0f, 1.0e-3f);

            // Comp threshold: linear [-60, 0] dB.
            zwtest::setParam (s, zw::id::fx ("comp", "threshold"), 0.0f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::fx ("comp", "threshold")), -60.0f, 1.0e-2f);
            zwtest::setParam (s, zw::id::fx ("comp", "threshold"), 1.0f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::fx ("comp", "threshold")), 0.0f, 1.0e-2f);

            // EQ low: linear [-15, 15] dB; midpoint should be 0.
            zwtest::setParam (s, zw::id::fx ("eq", "low"), 0.5f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::fx ("eq", "low")), 0.0f, 1.0e-1f);
        }

        //======================================================================
        beginTest ("int ranges clamp at their declared extremes");
        {
            zwtest::DummyProcessor proc;
            auto& s = proc.apvts;

            // Unison: [1, 16].
            zwtest::setParam (s, zw::id::osc ('A', "unison"), 0.0f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::osc ('A', "unison")), 1.0f, 1.0e-3f);
            zwtest::setParam (s, zw::id::osc ('A', "unison"), 1.0f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::osc ('A', "unison")), 16.0f, 1.0e-3f);

            // Octave: [-3, 3].
            zwtest::setParam (s, zw::id::osc ('A', "octave"), 0.0f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::osc ('A', "octave")), -3.0f, 1.0e-3f);
            zwtest::setParam (s, zw::id::osc ('A', "octave"), 1.0f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::osc ('A', "octave")), 3.0f, 1.0e-3f);

            // Polyphony: [1, 32].
            zwtest::setParam (s, zw::id::glidePoly, 0.0f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::glidePoly), 1.0f, 1.0e-3f);
            zwtest::setParam (s, zw::id::glidePoly, 1.0f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::glidePoly), 32.0f, 1.0e-3f);

            // Bend range: [0, 24].
            zwtest::setParam (s, zw::id::bendRange, 0.0f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::bendRange), 0.0f, 1.0e-3f);
            zwtest::setParam (s, zw::id::bendRange, 1.0f);
            expectWithinAbsoluteError (zwtest::rawParam (s, zw::id::bendRange), 24.0f, 1.0e-3f);
        }

        //======================================================================
        beginTest ("choice parameters expose all enum cases and clamp the index");
        {
            zwtest::DummyProcessor proc;
            auto& s = proc.apvts;

            auto* ft = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (zw::id::filterType));
            expect (ft != nullptr, "filter type must be a choice parameter");

            if (ft != nullptr)
            {
                const auto& names = ft->choices;
                expectEquals (names.size(), zw::choices::filterType().size());

                // Drive every enum case and read it back via the normalised setter.
                const int last = names.size() - 1;
                for (int idx = 0; idx <= last; ++idx)
                {
                    const float norm = (last > 0) ? (float) idx / (float) last : 0.0f;
                    zwtest::setParam (s, zw::id::filterType, norm);
                    expectEquals (ft->getIndex(), idx);
                }

                // Out-of-range norm clamps to the last index.
                zwtest::setParam (s, zw::id::filterType, 1.0f);
                expectEquals (ft->getIndex(), last);
                zwtest::setParam (s, zw::id::filterType, 0.0f);
                expectEquals (ft->getIndex(), 0);
            }

            // Warp mode choice has 7 options ("Off".."Quantize").
            if (auto* wm = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (zw::id::osc ('A', "warpmode"))))
            {
                expectEquals (wm->choices.size(), 7);
                expectEquals (wm->choices[0], juce::String ("Off"));
                expectEquals (wm->choices[6], juce::String ("Quantize"));
            }
            else
            {
                expect (false, "warp mode should be a choice parameter");
            }
        }

        //======================================================================
        beginTest ("every osc/env/lfo index family resolves to a real parameter");
        {
            zwtest::DummyProcessor proc;
            auto& s = proc.apvts;

            for (char ab : { 'A', 'B' })
                for (const char* p : { "enable", "wtpos", "warp", "warpmode", "unison",
                                       "detune", "uniblend", "uniwidth", "level", "pan",
                                       "phase", "phaserand", "octave", "coarse", "fine" })
                    expect (s.getParameter (zw::id::osc (ab, p)) != nullptr,
                            zw::id::osc (ab, p));

            for (int n = 1; n <= 3; ++n)
                for (const char* p : { "attack", "decay", "sustain", "release" })
                    expect (s.getParameter (zw::id::env (n, p)) != nullptr,
                            zw::id::env (n, p));

            for (int n = 1; n <= 4; ++n)
                for (const char* p : { "shape", "sync", "ratehz", "ratediv",
                                       "depth", "rise", "phase", "mode" })
                    expect (s.getParameter (zw::id::lfo (n, p)) != nullptr,
                            zw::id::lfo (n, p));
        }
    }
};

static ParametersTests parametersTests;
}
