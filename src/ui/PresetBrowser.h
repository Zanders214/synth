#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "ZWLookAndFeel.h"
#include "../PresetManager.h"

namespace zw
{

//==============================================================================
// Header preset browser: builds a themed PopupMenu listing the factory bank and
// the user presets (two sections) from a PresetManager, plus a "Save current"
// entry. Owns the list/selection logic; the editor wires the callbacks to load
// or save. The menu is rebuilt from disk on every show(), so a freshly saved
// preset appears immediately with no extra refresh wiring.
//
// PopupMenu is used rather than a custom overlay because its colours are already
// themed in ZWLookAndFeel and it handles outside-click dismissal, scrolling and
// section headers natively.
//==============================================================================
class PresetBrowser
{
public:
    std::function<void (int)>          onLoadFactory;   // factory program index
    std::function<void (juce::String)> onLoadUser;      // user preset name
    std::function<void ()>             onSave;          // open the save dialog

    PresetBrowser (PresetManager& pmRef, ZWLookAndFeel& lf) : pm (pmRef), lnf (lf) {}

    // Show the browser anchored to a target area (e.g. the preset well).
    // currentCombinedIndex ticks the active entry: [0, numFactory) is factory,
    // numFactory.. is the matching user preset.
    void show (juce::Component* anchor, juce::Rectangle<int> targetArea, int currentCombinedIndex)
    {
        const int numFactory = pm.getNumFactory();
        userNames = pm.getUserPresetNames();

        juce::PopupMenu menu;
        menu.setLookAndFeel (&lnf);

        menu.addSectionHeader ("FACTORY");
        for (int i = 0; i < numFactory; ++i)
            menu.addItem (kFactoryBase + i, pm.factoryName (i), true, currentCombinedIndex == i);

        menu.addSectionHeader ("USER");
        if (userNames.isEmpty())
        {
            menu.addItem (kNoUserId, "(no saved presets)", false, false);
        }
        else
        {
            for (int i = 0; i < userNames.size(); ++i)
                menu.addItem (kUserBase + i, userNames[i], true,
                              currentCombinedIndex == numFactory + i);
        }

        menu.addSeparator();
        menu.addItem (kSaveId, "Save current…");

        auto options = juce::PopupMenu::Options()
                           .withTargetComponent (anchor)
                           .withTargetScreenArea (targetArea)
                           .withMinimumWidth (220);

        menu.showMenuAsync (options, [this, numFactory] (int result)
        {
            if (result == 0)
                return;                                  // dismissed

            if (result == kSaveId)
            {
                if (onSave) onSave();
            }
            else if (result >= kUserBase)
            {
                const int idx = result - kUserBase;
                if (juce::isPositiveAndBelow (idx, userNames.size()) && onLoadUser)
                    onLoadUser (userNames[idx]);
            }
            else if (result >= kFactoryBase)
            {
                const int idx = result - kFactoryBase;
                if (juce::isPositiveAndBelow (idx, numFactory) && onLoadFactory)
                    onLoadFactory (idx);
            }
        });
    }

private:
    static constexpr int kFactoryBase = 1;
    static constexpr int kUserBase    = 1000;
    static constexpr int kSaveId      = 9000;
    static constexpr int kNoUserId    = 9001;

    PresetManager&  pm;
    ZWLookAndFeel&  lnf;
    juce::StringArray userNames;   // maps chosen user id -> name across the async callback
};

} // namespace zw
