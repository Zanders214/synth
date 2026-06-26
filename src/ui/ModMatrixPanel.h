#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "ZWLookAndFeel.h"
#include "../dsp/ModMatrix.h"
#include <vector>

namespace zw
{

//==============================================================================
// Mod-matrix route editor for the MOD MATRIX lower tab.
//
// A scrollable list of routes, one row each:
//     [source v] -> [destination v]  [bipolar amount slider]  [X]
// plus an "ADD ROUTE" button. All editing happens on the message thread and is
// committed to the live ModMatrix through its lock-free publish path (a single
// atomic swap per edit), so the audio thread never observes a half-written list.
//
// The combo item index maps 1:1 onto the ModSource/ModDest enum order, so reads
// and writes are index-based (no string parsing). The visible labels below are
// purely presentational; the enum order remains the source of truth.
//==============================================================================
class ModMatrixPanel : public juce::Component
{
public:
    ModMatrixPanel (ModMatrix& matrixIn, ZWLookAndFeel& lnfIn)
        : matrix (matrixIn), lnf (lnfIn)
    {
        addBtn.setButtonText ("ADD ROUTE");
        addBtn.setLookAndFeel (&lnf);
        addBtn.onClick = [this] { addRoute(); };
        addAndMakeVisible (addBtn);

        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);

        rebuildFromMatrix();
    }

    void paint (juce::Graphics& g) override
    {
        wellBackground (g, getLocalBounds().toFloat().reduced (1.0f));

        if (rows.isEmpty())
        {
            g.setColour (theme::tMuted);
            g.setFont (juce::Font (juce::FontOptions (12.0f)));
            g.drawText ("No routes - press ADD ROUTE to create one",
                        viewport.getBounds(), juce::Justification::centred, false);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (8);
        auto top = b.removeFromTop (24);
        addBtn.setBounds (top.removeFromLeft (110));
        b.removeFromTop (6);
        viewport.setBounds (b);
        layoutRows();
    }

    // Re-read the live matrix when the tab becomes visible so external changes
    // (preset / DAW state load via ModMatrix::fromValueTree) are reflected.
    void visibilityChanged() override
    {
        if (isVisible())
            rebuildFromMatrix();
    }

private:
    static constexpr int kRowHeight = 30;

    static juce::StringArray sourceLabels()
    {
        return { "Env 1", "Env 2", "Env 3", "LFO 1", "LFO 2", "LFO 3", "LFO 4",
                 "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Velocity", "Note" };
    }

    static juce::StringArray destLabels()
    {
        return { "Osc A WT", "Osc A Warp", "Osc A Level", "Osc A Pan", "Osc A Detune",
                 "Osc B WT", "Osc B Warp", "Osc B Level", "Osc B Pan", "Osc B Detune",
                 "Sub Level", "Noise Level", "Cutoff", "Reso", "Drive" };
    }

    //==========================================================================
    // One editable route row. Holds the controls; all behaviour is wired by the
    // owning panel so the row stays a dumb view.
    struct Row : public juce::Component
    {
        Row (ZWLookAndFeel& lnfIn)
        {
            source.addItemList (sourceLabels(), 1);
            dest.addItemList (destLabels(), 1);
            source.setLookAndFeel (&lnfIn);
            dest.setLookAndFeel (&lnfIn);

            amount.setSliderStyle (juce::Slider::LinearHorizontal);
            amount.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 18);
            amount.setRange (-1.0, 1.0, 0.001);
            amount.setDoubleClickReturnValue (true, 0.0);
            amount.setLookAndFeel (&lnfIn);

            del.setButtonText ("X");
            del.setLookAndFeel (&lnfIn);

            addAndMakeVisible (source);
            addAndMakeVisible (dest);
            addAndMakeVisible (amount);
            addAndMakeVisible (del);
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (theme::tMuted);
            g.setFont (juce::Font (juce::FontOptions (14.0f)));
            g.drawText (juce::String::fromUTF8 ("\xe2\x86\x92"), arrowArea,
                        juce::Justification::centred, false);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (0, 3);
            source.setBounds (r.removeFromLeft (100));
            arrowArea = r.removeFromLeft (20);
            dest.setBounds (r.removeFromLeft (110));
            r.removeFromLeft (6);
            del.setBounds (r.removeFromRight (26));
            r.removeFromRight (6);
            amount.setBounds (r);
        }

        juce::ComboBox source, dest;
        juce::Slider   amount;
        juce::TextButton del;
        juce::Rectangle<int> arrowArea;
    };

    //==========================================================================
    void addRoute()
    {
        auto routes = matrix.getRoutes();
        routes.push_back ({ ModSource::Env1, ModDest::OscAWt, 0.0f });
        matrix.setRoutes (routes);
        rebuildFromMatrix();
    }

    void deleteRoute (int index)
    {
        auto routes = matrix.getRoutes();
        if (index >= 0 && index < (int) routes.size())
        {
            routes.erase (routes.begin() + index);
            matrix.setRoutes (routes);
        }
        rebuildFromMatrix();
    }

    // Read every row's controls into a fresh route list and publish it to the
    // matrix in one atomic swap. Called on any source/dest/amount change.
    void commit()
    {
        std::vector<ModRoute> routes;
        routes.reserve ((size_t) rows.size());
        for (auto* row : rows)
        {
            ModRoute r;
            r.source = (ModSource) (row->source.getSelectedId() - 1);
            r.dest   = (ModDest)   (row->dest.getSelectedId()   - 1);
            r.amount = (float) row->amount.getValue();
            routes.push_back (r);
        }
        matrix.setRoutes (routes);
    }

    void rebuildFromMatrix()
    {
        rows.clear();

        const auto routes = matrix.getRoutes();
        for (int i = 0; i < (int) routes.size(); ++i)
        {
            auto* row = rows.add (std::make_unique<Row> (lnf));
            const auto& r = routes[(size_t) i];
            row->source.setSelectedId ((int) r.source + 1, juce::dontSendNotification);
            row->dest.setSelectedId   ((int) r.dest   + 1, juce::dontSendNotification);
            row->amount.setValue (r.amount, juce::dontSendNotification);

            row->source.onChange = [this] { commit(); };
            row->dest.onChange   = [this] { commit(); };
            row->amount.onValueChange = [this] { commit(); };
            row->del.onClick = [this, i] { deleteRoute (i); };

            content.addAndMakeVisible (row);
        }

        layoutRows();
        repaint();
    }

    void layoutRows()
    {
        const int w = juce::jmax (0, viewport.getWidth()
                                       - viewport.getScrollBarThickness());
        content.setSize (w, juce::jmax (viewport.getHeight(), rows.size() * kRowHeight));
        for (int i = 0; i < rows.size(); ++i)
            rows[i]->setBounds (0, i * kRowHeight, w, kRowHeight);
    }

    ModMatrix&     matrix;
    ZWLookAndFeel& lnf;

    juce::TextButton addBtn;
    juce::Viewport   viewport;
    juce::Component  content;
    juce::OwnedArray<Row> rows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModMatrixPanel)
};

} // namespace zw
