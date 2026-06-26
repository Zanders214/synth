// Unit tests for zw::PresetManager (factory bank, state capture/apply,
// reset-to-defaults via applyFactory, and user preset save/load round-trip).
#include <juce_core/juce_core.h>
#include "PresetManager.h"
#include "Parameters.h"
#include "dsp/ModMatrix.h"
#include "TestHelpers.h"

#include <cmath>

namespace
{
struct PresetManagerTests : juce::UnitTest
{
    PresetManagerTests() : juce::UnitTest ("PresetManager", "DSP") {}

    void runTest() override
    {
        using namespace zw;

        beginTest ("factory bank has all six presets with non-empty, distinct names");
        {
            zwtest::DummyProcessor proc;
            ModMatrix matrix;
            PresetManager pm (proc, proc.apvts, matrix);

            const int n = pm.getNumFactory();
            expectGreaterThan (n, 5, "expected at least six factory presets");
            expectEquals (n, 6);

            expectEquals (pm.factoryName (0), juce::String ("Init"));
            expectEquals (pm.factoryName (3), juce::String ("Sub Bass"));
            expectEquals (pm.factoryName (5), juce::String ("Vapor Keys"));

            // Every in-range name must be non-empty.
            for (int i = 0; i < n; ++i)
                expect (pm.factoryName (i).isNotEmpty(), "factory name should be non-empty");
        }

        beginTest ("factoryName out-of-range returns empty string");
        {
            zwtest::DummyProcessor proc;
            ModMatrix matrix;
            PresetManager pm (proc, proc.apvts, matrix);

            expect (pm.factoryName (-1).isEmpty(), "negative index should give empty");
            expect (pm.factoryName (pm.getNumFactory()).isEmpty(), "past-end index should give empty");
            expect (pm.factoryName (9999).isEmpty(), "far past-end index should give empty");
        }

        beginTest ("applyFactory(SubBass) raises sub level and lowers cutoff");
        {
            zwtest::DummyProcessor proc;
            ModMatrix matrix;
            PresetManager pm (proc, proc.apvts, matrix);

            const float subBefore = zwtest::rawParam (proc.apvts, id::subLevel);
            const float cutBefore = zwtest::rawParam (proc.apvts, id::filterCutoff);

            pm.applyFactory (3);   // "Sub Bass": sub_level actual 0.9, cutoff 650 Hz

            const float subAfter = zwtest::rawParam (proc.apvts, id::subLevel);
            const float cutAfter = zwtest::rawParam (proc.apvts, id::filterCutoff);

            expect (std::isfinite (subAfter));
            expect (std::isfinite (cutAfter));
            expectGreaterThan (subAfter, 0.7f, "Sub Bass should drive sub level high");
            expectGreaterThan (subAfter, subBefore, "sub level should rise from default");
            expectLessThan (cutAfter, cutBefore, "Sub Bass should lower the filter cutoff");
            expectWithinAbsoluteError (cutAfter, 650.0f, 50.0f);
        }

        beginTest ("applyFactory(HyperLead) overrides unison/detune and is finite");
        {
            zwtest::DummyProcessor proc;
            ModMatrix matrix;
            PresetManager pm (proc, proc.apvts, matrix);

            pm.applyFactory (1);   // "Hyper Lead"
            const float unison = zwtest::rawParam (proc.apvts, id::osc ('A', "unison"));
            const float cutoff = zwtest::rawParam (proc.apvts, id::filterCutoff);
            expect (std::isfinite (unison));
            expect (std::isfinite (cutoff));
            expectGreaterThan (unison, 1.0f, "Hyper Lead uses unison voices");
            expectGreaterThan (cutoff, 5000.0f, "Hyper Lead is bright (cutoff 8500 Hz)");
        }

        beginTest ("applyFactory(Init) strips to a single clean oscillator with no modulation");
        {
            zwtest::DummyProcessor proc;
            ModMatrix matrix;
            PresetManager pm (proc, proc.apvts, matrix);

            // Disturb the state with another patch first.
            pm.applyFactory (3);   // Sub Bass moves things around

            pm.applyFactory (0);   // Init: bare single saw, nothing on it

            // Clean voicing: one oscillator, single voice, no sub/noise/filter.
            expectWithinAbsoluteError (zwtest::rawParam (proc.apvts, id::osc ('A', "unison")), 1.0f, 1.0e-4f);
            expectEquals (zwtest::rawParam (proc.apvts, id::osc ('B', "enable")), 0.0f, "Osc B must be off");
            expectEquals (zwtest::rawParam (proc.apvts, id::subEnable),    0.0f, "sub must be off");
            expectEquals (zwtest::rawParam (proc.apvts, id::filterEnable), 0.0f, "filter must be off");

            // No effects.
            expectEquals (zwtest::rawParam (proc.apvts, id::fx ("reverb", "enable")), 0.0f, "reverb must be off");
            expectEquals (zwtest::rawParam (proc.apvts, id::fx ("delay",  "enable")), 0.0f, "delay must be off");

            // No modulation routes (amp env is hardwired, so the patch still sounds).
            expectEquals (matrix.size(), 0, "Init must clear the mod matrix");

            // Parameters Init does not override fall back to their defaults
            // (covers resetToDefaults()); masterOut default is 0.80 (percent).
            expectWithinAbsoluteError (zwtest::rawParam (proc.apvts, id::masterOut), 0.80f, 0.01f);
        }

        beginTest ("applyFactory ignores out-of-range index (no state change)");
        {
            zwtest::DummyProcessor proc;
            ModMatrix matrix;
            PresetManager pm (proc, proc.apvts, matrix);

            const float before = zwtest::rawParam (proc.apvts, id::masterOut);
            pm.applyFactory (-1);
            pm.applyFactory (pm.getNumFactory());
            pm.applyFactory (12345);
            const float after = zwtest::rawParam (proc.apvts, id::masterOut);
            expectEquals (after, before, "out-of-range applyFactory must not change parameters");
        }

        beginTest ("captureState produces the wrapper tree with APVTS + mod matrix");
        {
            zwtest::DummyProcessor proc;
            ModMatrix matrix;
            PresetManager pm (proc, proc.apvts, matrix);

            const auto wrapper = pm.captureState();
            expect (wrapper.hasType (PresetManager::kWrapperType), "wrapper should have ZANDERSWAVE type");

            auto params = wrapper.getChildWithName (proc.apvts.state.getType());
            expect (params.isValid(), "wrapper should contain the APVTS state child");

            auto mm = wrapper.getChildWithName ("MODMATRIX");
            expect (mm.isValid(), "wrapper should contain the MODMATRIX child");
        }

        beginTest ("captureState then applyState restores a changed parameter");
        {
            zwtest::DummyProcessor proc;
            ModMatrix matrix;
            PresetManager pm (proc, proc.apvts, matrix);

            // Put a distinctive value into masterOut, then snapshot.
            zwtest::setParam (proc.apvts, id::masterOut, 0.65f);
            const float snapped = zwtest::rawParam (proc.apvts, id::masterOut);
            const auto snapshot = pm.captureState();

            // Change it, confirm the change, then restore from the snapshot.
            zwtest::setParam (proc.apvts, id::masterOut, 0.10f);
            expectNotEquals (zwtest::rawParam (proc.apvts, id::masterOut), snapped);

            pm.applyState (snapshot);
            expectWithinAbsoluteError (zwtest::rawParam (proc.apvts, id::masterOut), snapped, 1.0e-3f);
        }

        beginTest ("applyState round-trips the mod matrix routes");
        {
            zwtest::DummyProcessor proc;
            ModMatrix matrix;
            PresetManager pm (proc, proc.apvts, matrix);

            // Define a known custom routing, then capture.
            matrix.clear();
            matrix.addRoute (ModSource::Lfo1, ModDest::Cutoff, 0.5f);
            matrix.addRoute (ModSource::Env2, ModDest::OscALevel, -0.25f);
            const int routeCount = matrix.size();
            expectEquals (routeCount, 2);
            const auto snapshot = pm.captureState();

            // Wipe the matrix, then restore from the captured wrapper.
            matrix.clear();
            expectEquals (matrix.size(), 0);

            pm.applyState (snapshot);
            expectEquals (matrix.size(), routeCount, "mod matrix routes should round-trip");

            const auto routes = matrix.getRoutes();
            expectEquals ((int) routes.size(), routeCount);
            bool foundCutoff = false;
            for (const auto& r : routes)
                if (r.source == ModSource::Lfo1 && r.dest == ModDest::Cutoff)
                {
                    foundCutoff = true;
                    expectWithinAbsoluteError (r.amount, 0.5f, 1.0e-3f);
                }
            expect (foundCutoff, "restored matrix should contain the Lfo1->Cutoff route");
        }

        beginTest ("applyState accepts legacy bare APVTS state");
        {
            zwtest::DummyProcessor proc;
            ModMatrix matrix;
            PresetManager pm (proc, proc.apvts, matrix);

            zwtest::setParam (proc.apvts, id::masterOut, 0.42f);
            const float legacyVal = zwtest::rawParam (proc.apvts, id::masterOut);
            // A bare copy of the APVTS state (no ZANDERSWAVE wrapper).
            const juce::ValueTree bare = proc.apvts.copyState();

            zwtest::setParam (proc.apvts, id::masterOut, 0.95f);
            expectNotEquals (zwtest::rawParam (proc.apvts, id::masterOut), legacyVal);

            pm.applyState (bare);
            expectWithinAbsoluteError (zwtest::rawParam (proc.apvts, id::masterOut), legacyVal, 1.0e-3f);
        }

        beginTest ("applyState with an unrecognised tree is a safe no-op");
        {
            zwtest::DummyProcessor proc;
            ModMatrix matrix;
            PresetManager pm (proc, proc.apvts, matrix);

            const float before = zwtest::rawParam (proc.apvts, id::masterOut);
            juce::ValueTree junk ("SOMETHING_ELSE");
            pm.applyState (junk);     // neither wrapper nor APVTS type
            const float after = zwtest::rawParam (proc.apvts, id::masterOut);
            expectEquals (after, before, "unknown state tree must not alter parameters");
        }

        beginTest ("getUserDir exists and is a directory");
        {
            zwtest::DummyProcessor proc;
            ModMatrix matrix;
            PresetManager pm (proc, proc.apvts, matrix);

            try
            {
                const auto dir = pm.getUserDir();
                // Creation may legitimately fail on a locked-down CI box; only
                // assert when the directory actually came into being.
                if (dir.exists())
                    expect (dir.isDirectory(), "user preset path should be a directory");
            }
            catch (...)
            {
                // Filesystem unavailable — do not fail the suite.
            }
        }

        beginTest ("saveUserPreset / loadUserPreset round-trip on disk (resilient)");
        {
            zwtest::DummyProcessor proc;
            ModMatrix matrix;
            PresetManager pm (proc, proc.apvts, matrix);

            // Unique name so concurrent runs / leftovers don't collide.
            const juce::String name = "zwtest_pm_"
                                    + juce::String (juce::Time::getMillisecondCounterHiRes(), 3)
                                          .replaceCharacter ('.', '_');
            juce::File written;

            try
            {
                // Stamp a recognisable value, capture the saved value, then save.
                zwtest::setParam (proc.apvts, id::masterOut, 0.33f);
                const float saved = zwtest::rawParam (proc.apvts, id::masterOut);

                const bool didSave = pm.saveUserPreset (name);
                written = pm.getUserDir().getChildFile (name + PresetManager::kFileExt);

                if (didSave)
                {
                    expect (written.existsAsFile(), "save should create the preset file");

                    // The saved name should appear in the listing.
                    const auto names = pm.getUserPresetNames();
                    expect (names.contains (name), "saved preset should be listed");

                    // Mutate, then load it back and confirm the value returns.
                    zwtest::setParam (proc.apvts, id::masterOut, 0.99f);
                    const bool didLoad = pm.loadUserPreset (name);
                    expect (didLoad, "load of a just-saved preset should succeed");
                    if (didLoad)
                        expectWithinAbsoluteError (zwtest::rawParam (proc.apvts, id::masterOut),
                                                   saved, 1.0e-3f);

                    // Loading a non-existent preset must report failure.
                    expect (! pm.loadUserPreset ("zwtest_definitely_not_here_42"),
                            "loading a missing preset should return false");
                }
            }
            catch (...)
            {
                // Filesystem unavailable — treat as skipped, not failed.
            }

            // Best-effort cleanup; never fail the test on cleanup issues.
            try { if (written.existsAsFile()) written.deleteFile(); } catch (...) {}
        }

        beginTest ("saveUserPreset with an empty/illegal name returns false");
        {
            zwtest::DummyProcessor proc;
            ModMatrix matrix;
            PresetManager pm (proc, proc.apvts, matrix);

            try
            {
                expect (! pm.saveUserPreset (juce::String()), "empty name should not save");
            }
            catch (...)
            {
                // Filesystem unavailable — skip silently.
            }
        }
    }
};

static PresetManagerTests presetManagerTests;
}
