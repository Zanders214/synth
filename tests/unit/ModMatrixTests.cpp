// Unit tests for zw::ModMatrix (lock-free modulation routing matrix).
#include <juce_core/juce_core.h>
#include "dsp/ModMatrix.h"

#include <array>
#include <cmath>
#include <vector>

namespace
{
// Helper: build a source array where source s holds value v (all others 0).
std::array<float, (size_t) zw::kNumModSources> oneHotSource (zw::ModSource s, float v)
{
    std::array<float, (size_t) zw::kNumModSources> src {};
    src.fill (0.0f);
    src[(size_t) s] = v;
    return src;
}

struct ModMatrixTests : juce::UnitTest
{
    ModMatrixTests() : juce::UnitTest ("ModMatrix", "DSP") {}

    void runTest() override
    {
        beginTest ("enum counts match id-table sizes");
        {
            expectEquals (zw::kNumModSources, 13);
            expectEquals (zw::kNumModDests,   15);
            expectEquals ((int) zw::ModSource::Count, zw::kNumModSources);
            expectEquals ((int) zw::ModDest::Count,   zw::kNumModDests);
        }

        beginTest ("freshly constructed matrix seeds the four default routes");
        {
            zw::ModMatrix mm;
            expectEquals (mm.size(), 4);

            const auto routes = mm.getRoutes();
            expectEquals ((int) routes.size(), 4);

            // Default route 0: Env1 -> OscALevel, amount 1.0
            expect (routes[0].source == zw::ModSource::Env1);
            expect (routes[0].dest   == zw::ModDest::OscALevel);
            expectWithinAbsoluteError (routes[0].amount, 1.0f, 1.0e-6f);

            // Default route 1: Lfo1 -> Cutoff, amount 0.42
            expect (routes[1].source == zw::ModSource::Lfo1);
            expect (routes[1].dest   == zw::ModDest::Cutoff);
            expectWithinAbsoluteError (routes[1].amount, 0.42f, 1.0e-6f);

            // Default route 2: Env2 -> OscAWt, amount 0.26
            expect (routes[2].source == zw::ModSource::Env2);
            expect (routes[2].dest   == zw::ModDest::OscAWt);
            expectWithinAbsoluteError (routes[2].amount, 0.26f, 1.0e-6f);

            // Default route 3: Macro1 -> OscBWt, amount -0.50
            expect (routes[3].source == zw::ModSource::Macro1);
            expect (routes[3].dest   == zw::ModDest::OscBWt);
            expectWithinAbsoluteError (routes[3].amount, -0.50f, 1.0e-6f);
        }

        beginTest ("clear() removes all routes; all dest sums become zero");
        {
            zw::ModMatrix mm;
            mm.clear();
            expectEquals (mm.size(), 0);
            expect (mm.getRoutes().empty());

            // Every source fully on -> with no routes, every dest must be 0.
            std::array<float, (size_t) zw::kNumModSources> src {};
            src.fill (1.0f);
            std::array<float, (size_t) zw::kNumModDests> sums {};
            sums.fill (123.0f); // poison: computeDestSums must overwrite
            mm.computeDestSums (src.data(), sums.data());
            for (int d = 0; d < zw::kNumModDests; ++d)
                expectEquals (sums[(size_t) d], 0.0f);
        }

        beginTest ("seedDefaults() re-publishes defaults after a clear");
        {
            zw::ModMatrix mm;
            mm.clear();
            expectEquals (mm.size(), 0);
            mm.seedDefaults();
            expectEquals (mm.size(), 4);
        }

        beginTest ("computeDestSums: a source at depth d feeds its destination");
        {
            zw::ModMatrix mm;
            mm.clear();
            mm.addRoute (zw::ModSource::Lfo2, zw::ModDest::Cutoff, 0.75f);

            // Source value 0.5, amount 0.75 -> expected contribution 0.375.
            auto src = oneHotSource (zw::ModSource::Lfo2, 0.5f);
            std::array<float, (size_t) zw::kNumModDests> sums {};
            mm.computeDestSums (src.data(), sums.data());

            expectWithinAbsoluteError (sums[(size_t) zw::ModDest::Cutoff], 0.375f, 1.0e-6f);

            // All other destinations untouched.
            for (int d = 0; d < zw::kNumModDests; ++d)
                if (d != (int) zw::ModDest::Cutoff)
                    expectEquals (sums[(size_t) d], 0.0f);
        }

        beginTest ("computeDestSums: linear in source value and amount sign");
        {
            zw::ModMatrix mm;
            mm.clear();
            mm.addRoute (zw::ModSource::Env3, zw::ModDest::Drive, -0.5f);

            std::array<float, (size_t) zw::kNumModDests> sums {};

            auto pos = oneHotSource (zw::ModSource::Env3, 1.0f);
            mm.computeDestSums (pos.data(), sums.data());
            expectWithinAbsoluteError (sums[(size_t) zw::ModDest::Drive], -0.5f, 1.0e-6f);

            // Negative source with negative amount -> positive contribution.
            auto neg = oneHotSource (zw::ModSource::Env3, -2.0f);
            mm.computeDestSums (neg.data(), sums.data());
            expectWithinAbsoluteError (sums[(size_t) zw::ModDest::Drive], 1.0f, 1.0e-6f);
        }

        beginTest ("computeDestSums: multiple sources accumulate into one dest");
        {
            zw::ModMatrix mm;
            mm.clear();
            mm.addRoute (zw::ModSource::Lfo1, zw::ModDest::Reso, 0.3f);
            mm.addRoute (zw::ModSource::Lfo2, zw::ModDest::Reso, 0.6f);
            expectEquals (mm.size(), 2);

            std::array<float, (size_t) zw::kNumModSources> src {};
            src.fill (0.0f);
            src[(size_t) zw::ModSource::Lfo1] = 1.0f;  // 1.0 * 0.3 = 0.3
            src[(size_t) zw::ModSource::Lfo2] = 0.5f;  // 0.5 * 0.6 = 0.3
            std::array<float, (size_t) zw::kNumModDests> sums {};
            mm.computeDestSums (src.data(), sums.data());

            expectWithinAbsoluteError (sums[(size_t) zw::ModDest::Reso], 0.6f, 1.0e-6f);
        }

        beginTest ("addRoute appends new routes and reports finite, in-range sums");
        {
            zw::ModMatrix mm;
            mm.clear();
            mm.addRoute (zw::ModSource::Macro2, zw::ModDest::OscAPan, 0.2f);
            mm.addRoute (zw::ModSource::Macro3, zw::ModDest::OscBLevel, 0.4f);
            expectEquals (mm.size(), 2);

            const auto routes = mm.getRoutes();
            expect (routes[0].source == zw::ModSource::Macro2);
            expect (routes[0].dest   == zw::ModDest::OscAPan);
            expect (routes[1].source == zw::ModSource::Macro3);
            expect (routes[1].dest   == zw::ModDest::OscBLevel);

            std::array<float, (size_t) zw::kNumModSources> src {};
            src.fill (1.0f);
            std::array<float, (size_t) zw::kNumModDests> sums {};
            mm.computeDestSums (src.data(), sums.data());
            for (int d = 0; d < zw::kNumModDests; ++d)
                expect (std::isfinite (sums[(size_t) d]));
        }

        beginTest ("addRoute on an existing (source,dest) updates amount, no duplicate");
        {
            zw::ModMatrix mm;
            mm.clear();
            mm.addRoute (zw::ModSource::Lfo3, zw::ModDest::OscADetune, 0.1f);
            mm.addRoute (zw::ModSource::Lfo3, zw::ModDest::OscADetune, 0.9f);

            expectEquals (mm.size(), 1, "same source+dest must overwrite, not duplicate");
            const auto routes = mm.getRoutes();
            expectWithinAbsoluteError (routes[0].amount, 0.9f, 1.0e-6f);
        }

        beginTest ("addRoute clamps amount to [-1, 1] on both insert and update");
        {
            zw::ModMatrix mm;
            mm.clear();

            // Out-of-range on initial insert.
            mm.addRoute (zw::ModSource::Velocity, zw::ModDest::SubLevel, 5.0f);
            expectWithinAbsoluteError (mm.getRoutes()[0].amount, 1.0f, 1.0e-6f);

            // Out-of-range on update of the same route.
            mm.addRoute (zw::ModSource::Velocity, zw::ModDest::SubLevel, -3.0f);
            expectWithinAbsoluteError (mm.getRoutes()[0].amount, -1.0f, 1.0e-6f);
            expectEquals (mm.size(), 1);
        }

        beginTest ("removeRoute erases the indexed route");
        {
            zw::ModMatrix mm;
            mm.clear();
            mm.addRoute (zw::ModSource::Lfo1, zw::ModDest::Cutoff, 0.5f);   // idx 0
            mm.addRoute (zw::ModSource::Lfo2, zw::ModDest::Reso,   0.5f);   // idx 1
            mm.addRoute (zw::ModSource::Lfo3, zw::ModDest::Drive,  0.5f);   // idx 2
            expectEquals (mm.size(), 3);

            mm.removeRoute (1); // remove the middle one
            expectEquals (mm.size(), 2);
            const auto routes = mm.getRoutes();
            expect (routes[0].source == zw::ModSource::Lfo1);
            expect (routes[1].source == zw::ModSource::Lfo3,
                    "remaining routes shift down after erase");
        }

        beginTest ("removeRoute ignores out-of-range indices");
        {
            zw::ModMatrix mm;
            mm.clear();
            mm.addRoute (zw::ModSource::Note, zw::ModDest::OscAWarp, 0.5f);
            expectEquals (mm.size(), 1);

            mm.removeRoute (-1);   // negative: no-op
            mm.removeRoute (99);   // beyond end: no-op
            mm.removeRoute (1);    // == size: no-op
            expectEquals (mm.size(), 1);

            mm.removeRoute (0);    // valid
            expectEquals (mm.size(), 0);
            mm.removeRoute (0);    // empty: no-op
            expectEquals (mm.size(), 0);
        }

        beginTest ("sourceId returns the right tag for every source");
        {
            const char* expected[] = {
                "ENV1", "ENV2", "ENV3", "LFO1", "LFO2", "LFO3", "LFO4",
                "MACRO1", "MACRO2", "MACRO3", "MACRO4", "VEL", "NOTE"
            };
            for (int i = 0; i < zw::kNumModSources; ++i)
                expectEquals (zw::ModMatrix::sourceId ((zw::ModSource) i),
                              juce::String (expected[i]));
        }

        beginTest ("destId returns the right tag for every destination");
        {
            const char* expected[] = {
                "aWt", "aWarp", "aLvl", "aPan", "aDet",
                "bWt", "bWarp", "bLvl", "bPan", "bDet",
                "subLvl", "noiLvl", "cut", "res", "drive"
            };
            for (int i = 0; i < zw::kNumModDests; ++i)
                expectEquals (zw::ModMatrix::destId ((zw::ModDest) i),
                              juce::String (expected[i]));
        }

        beginTest ("source id round-trips through sourceFromId");
        {
            for (int i = 0; i < zw::kNumModSources; ++i)
            {
                const auto s  = (zw::ModSource) i;
                const auto id = zw::ModMatrix::sourceId (s);
                bool ok = false;
                const auto back = zw::ModMatrix::sourceFromId (id, ok);
                expect (ok, "valid source id should resolve");
                expect (back == s);
            }
        }

        beginTest ("dest id round-trips through destFromId");
        {
            for (int i = 0; i < zw::kNumModDests; ++i)
            {
                const auto d  = (zw::ModDest) i;
                const auto id = zw::ModMatrix::destId (d);
                bool ok = false;
                const auto back = zw::ModMatrix::destFromId (id, ok);
                expect (ok, "valid dest id should resolve");
                expect (back == d);
            }
        }

        beginTest ("unknown ids report ok=false and a safe default");
        {
            bool okS = true;
            const auto s = zw::ModMatrix::sourceFromId ("NOPE", okS);
            expect (! okS, "unknown source id should fail");
            expect (s == zw::ModSource::Env1, "fallback source is Env1");

            bool okD = true;
            const auto d = zw::ModMatrix::destFromId ("nope", okD);
            expect (! okD, "unknown dest id should fail");
            expect (d == zw::ModDest::OscAWt, "fallback dest is OscAWt");
        }

        beginTest ("toValueTree mirrors the live route list");
        {
            zw::ModMatrix mm;   // defaults present
            const auto tree = mm.toValueTree();
            expect (tree.hasType ("MODMATRIX"));
            expectEquals (tree.getNumChildren(), mm.size());

            const auto routes = mm.getRoutes();
            for (int i = 0; i < tree.getNumChildren(); ++i)
            {
                const auto node = tree.getChild (i);
                expect (node.hasType ("ROUTE"));
                expectEquals (node.getProperty ("src").toString(),
                              zw::ModMatrix::sourceId (routes[(size_t) i].source));
                expectEquals (node.getProperty ("dest").toString(),
                              zw::ModMatrix::destId (routes[(size_t) i].dest));
                expectWithinAbsoluteError ((float) node.getProperty ("amount"),
                                           routes[(size_t) i].amount, 1.0e-6f);
            }
        }

        beginTest ("fromValueTree restores an exact route set (round-trip)");
        {
            zw::ModMatrix a;
            a.clear();
            a.addRoute (zw::ModSource::Env2,   zw::ModDest::OscBWarp, 0.33f);
            a.addRoute (zw::ModSource::Macro4, zw::ModDest::NoiseLevel, -0.7f);

            const auto tree = a.toValueTree();

            zw::ModMatrix b;             // starts with defaults
            b.fromValueTree (tree);      // should be replaced wholesale
            expectEquals (b.size(), a.size());

            const auto ra = a.getRoutes();
            const auto rb = b.getRoutes();
            for (size_t i = 0; i < ra.size(); ++i)
            {
                expect (rb[i].source == ra[i].source);
                expect (rb[i].dest   == ra[i].dest);
                expectWithinAbsoluteError (rb[i].amount, ra[i].amount, 1.0e-6f);
            }
        }

        beginTest ("fromValueTree ignores wrong root type");
        {
            zw::ModMatrix mm;   // defaults
            const int before = mm.size();
            juce::ValueTree bogus ("NOTAMATRIX");
            mm.fromValueTree (bogus);
            expectEquals (mm.size(), before, "wrong root type must be a no-op");
        }

        beginTest ("fromValueTree skips non-ROUTE children and bad ids");
        {
            juce::ValueTree tree ("MODMATRIX");

            juce::ValueTree good ("ROUTE");
            good.setProperty ("src", "LFO4", nullptr);
            good.setProperty ("dest", "cut", nullptr);
            good.setProperty ("amount", 0.5f, nullptr);
            tree.appendChild (good, nullptr);

            juce::ValueTree notRoute ("JUNK");      // wrong type: skipped
            tree.appendChild (notRoute, nullptr);

            juce::ValueTree badSrc ("ROUTE");       // unknown source id: skipped
            badSrc.setProperty ("src", "BADSRC", nullptr);
            badSrc.setProperty ("dest", "cut", nullptr);
            badSrc.setProperty ("amount", 0.5f, nullptr);
            tree.appendChild (badSrc, nullptr);

            juce::ValueTree badDest ("ROUTE");      // unknown dest id: skipped
            badDest.setProperty ("src", "LFO1", nullptr);
            badDest.setProperty ("dest", "BADDEST", nullptr);
            badDest.setProperty ("amount", 0.5f, nullptr);
            tree.appendChild (badDest, nullptr);

            zw::ModMatrix mm;
            mm.fromValueTree (tree);
            expectEquals (mm.size(), 1, "only the one valid ROUTE survives");

            const auto routes = mm.getRoutes();
            expect (routes[0].source == zw::ModSource::Lfo4);
            expect (routes[0].dest   == zw::ModDest::Cutoff);
            expectWithinAbsoluteError (routes[0].amount, 0.5f, 1.0e-6f);
        }

        beginTest ("default routes produce expected dest sums end-to-end");
        {
            zw::ModMatrix mm;   // defaults
            // Drive every source to 1.0; compute the published dest sums.
            std::array<float, (size_t) zw::kNumModSources> src {};
            src.fill (1.0f);
            std::array<float, (size_t) zw::kNumModDests> sums {};
            mm.computeDestSums (src.data(), sums.data());

            // OscALevel <- Env1 * 1.0
            expectWithinAbsoluteError (sums[(size_t) zw::ModDest::OscALevel], 1.0f, 1.0e-6f);
            // Cutoff <- Lfo1 * 0.42
            expectWithinAbsoluteError (sums[(size_t) zw::ModDest::Cutoff], 0.42f, 1.0e-6f);
            // OscAWt <- Env2 * 0.26
            expectWithinAbsoluteError (sums[(size_t) zw::ModDest::OscAWt], 0.26f, 1.0e-6f);
            // OscBWt <- Macro1 * -0.50
            expectWithinAbsoluteError (sums[(size_t) zw::ModDest::OscBWt], -0.50f, 1.0e-6f);

            // Untouched dests stay zero.
            expectEquals (sums[(size_t) zw::ModDest::OscBPan], 0.0f);
            expectEquals (sums[(size_t) zw::ModDest::Reso],    0.0f);
        }

        beginTest ("publish/read survives many config rewrites (double-buffer swap)");
        {
            zw::ModMatrix mm;
            std::array<float, (size_t) zw::kNumModSources> src {};
            src.fill (1.0f);
            std::array<float, (size_t) zw::kNumModDests> sums {};

            for (int n = 0; n < 8; ++n)
            {
                mm.clear();
                mm.addRoute (zw::ModSource::Lfo1, zw::ModDest::Cutoff, 0.5f);
                mm.computeDestSums (src.data(), sums.data());
                expectWithinAbsoluteError (sums[(size_t) zw::ModDest::Cutoff], 0.5f, 1.0e-6f);
                expectEquals (mm.size(), 1);
            }
        }
    }
};

static ModMatrixTests modMatrixTests;
}
