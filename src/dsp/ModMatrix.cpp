#include "ModMatrix.h"

namespace zw
{

static const char* kSourceIds[kNumModSources] = {
    "ENV1", "ENV2", "ENV3", "LFO1", "LFO2", "LFO3", "LFO4",
    "MACRO1", "MACRO2", "MACRO3", "MACRO4", "VEL", "NOTE"
};

static const char* kDestIds[kNumModDests] = {
    "aWt", "aWarp", "aLvl", "aPan", "aDet",
    "bWt", "bWarp", "bLvl", "bPan", "bDet",
    "subLvl", "noiLvl", "cut", "res", "drive"
};

juce::String ModMatrix::sourceId (ModSource s) { return kSourceIds[(int) s]; }
juce::String ModMatrix::destId   (ModDest d)   { return kDestIds[(int) d]; }

ModSource ModMatrix::sourceFromId (const juce::String& s, bool& ok)
{
    for (int i = 0; i < kNumModSources; ++i)
        if (s == kSourceIds[i]) { ok = true; return (ModSource) i; }
    ok = false; return ModSource::Env1;
}

ModDest ModMatrix::destFromId (const juce::String& s, bool& ok)
{
    for (int i = 0; i < kNumModDests; ++i)
        if (s == kDestIds[i]) { ok = true; return (ModDest) i; }
    ok = false; return ModDest::OscAWt;
}

void ModMatrix::seedDefaults()
{
    routes.clear();
    routes.push_back ({ ModSource::Env1,   ModDest::OscALevel, 1.00f });
    routes.push_back ({ ModSource::Lfo1,   ModDest::Cutoff,    0.42f });
    routes.push_back ({ ModSource::Env2,   ModDest::OscAWt,    0.26f });
    routes.push_back ({ ModSource::Macro1, ModDest::OscBWt,   -0.50f });
}

void ModMatrix::addRoute (ModSource s, ModDest d, float amount)
{
    for (auto& r : routes)
        if (r.source == s && r.dest == d) { r.amount = amount; return; }
    routes.push_back ({ s, d, juce::jlimit (-1.0f, 1.0f, amount) });
}

void ModMatrix::removeRoute (int index)
{
    if (index >= 0 && index < (int) routes.size())
        routes.erase (routes.begin() + index);
}

juce::ValueTree ModMatrix::toValueTree() const
{
    juce::ValueTree tree ("MODMATRIX");
    for (const auto& r : routes)
    {
        juce::ValueTree node ("ROUTE");
        node.setProperty ("src",    sourceId (r.source), nullptr);
        node.setProperty ("dest",   destId (r.dest),     nullptr);
        node.setProperty ("amount", r.amount,            nullptr);
        tree.appendChild (node, nullptr);
    }
    return tree;
}

void ModMatrix::fromValueTree (const juce::ValueTree& v)
{
    if (! v.hasType ("MODMATRIX"))
        return;

    routes.clear();
    for (const auto& node : v)
    {
        if (! node.hasType ("ROUTE")) continue;
        bool okS = false, okD = false;
        const auto s = sourceFromId (node.getProperty ("src").toString(), okS);
        const auto d = destFromId   (node.getProperty ("dest").toString(), okD);
        if (okS && okD)
            routes.push_back ({ s, d, (float) node.getProperty ("amount") });
    }
}

} // namespace zw
