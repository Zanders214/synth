#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

namespace zw
{

//==============================================================================
// Modulation routing: a list of {source, destination, amount(-1..1)} routes.
// Per block/voice the modulator values are gathered into a source array and
// summed per destination; the voice then applies the sum on top of each
// destination's normalised base value. Source/dest names match the prototype
// for state compatibility.
//
// Thread-safety: the audio thread reads routes every block via computeDestSums
// while the message thread can rewrite them (preset/state load). Routes live in
// a lock-free double buffer — writers fill the inactive buffer and publish it
// with a single atomic store; the audio thread reads the published buffer.
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
    static constexpr int kMaxRoutes = 256;

    ModMatrix() { seedDefaults(); }

    void clear()                                  { publish ({}); }
    void seedDefaults();
    void addRoute (ModSource s, ModDest d, float amount);
    void removeRoute (int index);

    std::vector<ModRoute> getRoutes() const;      // snapshot (UI/serialisation)
    int  size() const noexcept { return buffers[(size_t) activeIdx.load (std::memory_order_acquire)].count; }

    // Audio-thread read: lock-free snapshot of the published routes.
    void computeDestSums (const float* srcValues, float* destSums) const noexcept
    {
        for (int d = 0; d < kNumModDests; ++d) destSums[d] = 0.0f;
        const auto& buf = buffers[(size_t) activeIdx.load (std::memory_order_acquire)];
        for (int i = 0; i < buf.count; ++i)
        {
            const auto& r = buf.routes[(size_t) i];
            destSums[(int) r.dest] += srcValues[(int) r.source] * r.amount;
        }
    }

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& v);

    static juce::String sourceId (ModSource s);
    static juce::String destId   (ModDest d);
    static ModSource    sourceFromId (const juce::String& s, bool& ok);
    static ModDest      destFromId   (const juce::String& s, bool& ok);

private:
    struct Buffer { std::array<ModRoute, kMaxRoutes> routes {}; int count = 0; };

    // Build the inactive buffer from `newRoutes` and publish it atomically.
    void publish (const std::vector<ModRoute>& newRoutes);

    std::array<Buffer, 2> buffers;
    std::atomic<int> activeIdx { 0 };
};

} // namespace zw
