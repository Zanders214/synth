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

//==============================================================================
void ModMatrix::publish (const std::vector<ModRoute>& newRoutes)
{
    const int next = 1 - activeIdx.load (std::memory_order_relaxed);
    auto& buf = buffers[(size_t) next];
    buf.count = juce::jmin (kMaxRoutes, (int) newRoutes.size());
    for (int i = 0; i < buf.count; ++i)
        buf.routes[(size_t) i] = newRoutes[(size_t) i];
    activeIdx.store (next, std::memory_order_release);   // publish
}

std::vector<ModRoute> ModMatrix::getRoutes() const
{
    const auto& buf = buffers[(size_t) activeIdx.load (std::memory_order_acquire)];
    return std::vector<ModRoute> (buf.routes.begin(), buf.routes.begin() + buf.count);
}

void ModMatrix::seedDefaults()
{
    publish ({
        { ModSource::Env1,   ModDest::OscALevel, 1.00f },
        { ModSource::Lfo1,   ModDest::Cutoff,    0.42f },
        { ModSource::Env2,   ModDest::OscAWt,    0.26f },
        { ModSource::Macro1, ModDest::OscBWt,   -0.50f },
    });
}

void ModMatrix::addRoute (ModSource s, ModDest d, float amount)
{
    auto routes = getRoutes();
    for (auto& r : routes)
        if (r.source == s && r.dest == d) { r.amount = juce::jlimit (-1.0f, 1.0f, amount); publish (routes); return; }
    if ((int) routes.size() < kMaxRoutes)
        routes.push_back ({ s, d, juce::jlimit (-1.0f, 1.0f, amount) });
    publish (routes);
}

void ModMatrix::removeRoute (int index)
{
    auto routes = getRoutes();
    if (index >= 0 && index < (int) routes.size())
    {
        routes.erase (routes.begin() + index);
        publish (routes);
    }
}

juce::ValueTree ModMatrix::toValueTree() const
{
    juce::ValueTree tree ("MODMATRIX");
    for (const auto& r : getRoutes())
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

    std::vector<ModRoute> routes;
    for (const auto& node : v)
    {
        if (! node.hasType ("ROUTE")) continue;
        bool okS = false, okD = false;
        const auto s = sourceFromId (node.getProperty ("src").toString(), okS);
        const auto d = destFromId   (node.getProperty ("dest").toString(), okD);
        if (okS && okD)
            routes.push_back ({ s, d, (float) node.getProperty ("amount") });
    }
    publish (routes);
}

} // namespace zw
