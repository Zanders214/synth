// Entry point for the ZandersWave DSP unit-test suite.
// Runs every registered juce::UnitTest and returns non-zero on any failure so
// CI (and the coverage run) can gate on it.

#include <juce_events/juce_events.h>
#include <juce_core/juce_core.h>
#include <cstdio>

int main()
{
    // Some DSP types build an APVTS (needs an AudioProcessor + message thread).
    juce::ScopedJuceInitialiser_GUI init;

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int totalPasses = 0, totalFailures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        const auto* r = runner.getResult (i);
        totalPasses   += r->passes;
        totalFailures += r->failures;

        if (r->failures > 0)
        {
            std::printf ("\n[FAIL] %s / %s  (%d passed, %d failed)\n",
                         r->unitTestName.toRawUTF8(), r->subcategoryName.toRawUTF8(),
                         r->passes, r->failures);
            for (const auto& m : r->messages)
                std::printf ("    %s\n", m.toRawUTF8());
        }
    }

    std::printf ("\nZandersWave unit tests: %d checks passed, %d failed\n",
                 totalPasses, totalFailures);
    std::printf ("%s\n", totalFailures == 0 ? "PASS" : "FAIL");
    return totalFailures == 0 ? 0 : 1;
}
