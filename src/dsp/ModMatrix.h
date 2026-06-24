#pragma once

#include <JuceHeader.h>
#include <vector>

namespace zw
{

//==============================================================================
// Modulation routing: a list of {source, destination, amount(-1..1)} routes.
// Per block/voice the modulator values are gathered into a source array and
// summed per destination; the voice then applies the sum on top of each
// destination's normalised base value. Source/dest names match the prototype
// for state compatibility. (Live editing + lock-free swap arrives with M7.)
//==============================================================================
enum class ModSource
{
    Env1 = 0, Env2, Env3, Lfo1, Lfo2, Lfo3, Lfo4,
    Macro1, Macro2, Macro3, Macro4, Velocity, Note, Count
};

enum class ModDest
{
    OscAWt = 0, OscAWarp, OscALevel, OscAPan, OscADetune,
    OscBWt, OscBWarp, OscBLevel, OscBPan, OscBDetune,
    SubLevel, NoiseLevel, Cutoff, Reso, Drive, Count
};

inline constexpr int kNumModSources = (int) ModSource::Count;
inline constexpr int kNumModDests   = (int) ModDest::Count;

struct ModRoute
{
    ModSource source;
    ModDest   dest;
    float     amount;   // -1..1
};

class ModMatrix
{
public:
    ModMatrix() { seedDefaults(); }

    void clear() { routes.clear(); }
    void seedDefaults();

    void addRoute (ModSource s, ModDest d, float amount);
    void removeRoute (int index);
    const std::vector<ModRoute>& getRoutes() const noexcept { return routes; }
    int  size() const noexcept { return (int) routes.size(); }

    // destSums must point to at least kNumModDests floats; srcValues to kNumModSources.
    void computeDestSums (const float* srcValues, float* destSums) const noexcept
    {
        for (int d = 0; d < kNumModDests; ++d) destSums[d] = 0.0f;
        for (const auto& r : routes)
            destSums[(int) r.dest] += srcValues[(int) r.source] * r.amount;
    }

    // State (serialised under its own tree, separate from the APVTS).
    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& v);

    // Name <-> enum mapping (prototype-compatible ids).
    static juce::String sourceId (ModSource s);
    static juce::String destId   (ModDest d);
    static ModSource    sourceFromId (const juce::String& s, bool& ok);
    static ModDest      destFromId   (const juce::String& s, bool& ok);

private:
    std::vector<ModRoute> routes;
};

} // namespace zw
