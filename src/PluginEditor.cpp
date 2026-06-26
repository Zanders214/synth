#include "PluginEditor.h"
#include "ui/Theme.h"
#include "ui/Displays.h"
#include "ui/ModMatrixPanel.h"
#include "ui/PresetBrowser.h"
#include "Parameters.h"
#include <array>

using APVTS = juce::AudioProcessorValueTreeState;

namespace zw
{

//==============================================================================
// A labelled rotary/linear control bound to an APVTS parameter.
class LabeledKnob : public juce::Component
{
public:
    LabeledKnob (APVTS& s, const juce::String& id, const juce::String& name,
                 ZWLookAndFeel& lf, bool rotary = true, juce::Colour arc = {})
    {
        slider.setSliderStyle (rotary ? juce::Slider::RotaryVerticalDrag : juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 14);
        slider.setNumDecimalPlacesToDisplay (2);
        slider.setColour (juce::Slider::textBoxTextColourId, theme::t2);
        // Show the parameter's own formatted text (with units) in the value box.
        if (const auto* rp = s.getParameter (id))
        {
            slider.textFromValueFunction = [rp] (double v)
            { return rp->getText (rp->getNormalisableRange().convertTo0to1 ((float) v), 0); };
            slider.valueFromTextFunction = [rp] (const juce::String& t)
            { return (double) rp->getNormalisableRange().convertFrom0to1 (rp->getValueForText (t)); };
        }
        if (arc != juce::Colour()) slider.setColour (juce::Slider::rotarySliderFillColourId, arc);
        slider.setLookAndFeel (&lf);
        slider.setTooltip (name);
        if (const auto* p = s.getParameter (id))
            slider.setDoubleClickReturnValue (true, p->getNormalisableRange().convertFrom0to1 (p->getDefaultValue()));
        addAndMakeVisible (slider);

        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (lf.displayFont (8.8f, true).withExtraKerningFactor (0.12f));
        label.setColour (juce::Label::textColourId, theme::tLabel);
        addAndMakeVisible (label);

        att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
    }
    void resized() override { auto r = getLocalBounds(); label.setBounds (r.removeFromTop (13)); slider.setBounds (r); }
    juce::Slider slider;
private:
    juce::Label label;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

//==============================================================================
// Section container: titled module with a hairline border.
class Module : public juce::Component
{
public:
    Module (const juce::String& t, const ZWLookAndFeel& lf)
        : title (t), titleFont (lf.displayFont (10.5f, true).withExtraKerningFactor (0.16f))
    {}
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::wa (0.018f));
        g.fillRoundedRectangle (r, (float) theme::rWell);
        g.setColour (theme::wa (0.08f));
        g.drawRoundedRectangle (r, (float) theme::rWell, 1.0f);
        g.setColour (theme::tLabel);
        g.setFont (titleFont);
        g.drawText (title.toUpperCase(), getLocalBounds().reduced (14, 10).removeFromTop (16),
                    juce::Justification::topLeft, false);
    }
    // Body rect in the PARENT's coordinate space — controls are children of the
    // panel (not of this module), so they must be laid out in panel coordinates.
    juce::Rectangle<int> body() const { return getBounds().reduced (14).withTrimmedTop (20); }
private:
    juce::String title; juce::Font titleFont;
};

} // namespace zw

// FxRackPanel reuses LabeledKnob (defined just above), so it is included here —
// after that definition — rather than with the other headers at the top. The
// namespace is closed/reopened so the panel lands in zw (not zw::zw).
#include "ui/FxRackPanel.h"

namespace zw
{

//==============================================================================
// The fixed 1320x900 panel that holds the whole UI.
class ZWPanel : public juce::Component, private juce::Timer
{
public:
    ZWPanel (ZandersWaveAudioProcessor& p, ZWLookAndFeel& lf)
        : proc (p), lnf (lf),
          envMod ("ENV1 · AMP", lf), lfoMod ("LFO1", lf), oscMod ("OSC A", lf),
          mixMod ("SOURCES", lf), filtMod ("FILTER", lf), outMod ("OUTPUT", lf),
          macroMod ("MACROS", lf), lowerMod ("WORKSPACE", lf),
          wtDisplay (p.apvts, id::osc ('A', "wtpos"), id::osc ('A', "warp")),
          filtResp (p.apvts), adsr (p.apvts, 1), scope (p), meter (p),
          keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        auto& s = proc.apvts;

        addAndMakeVisible (envMod); addAndMakeVisible (lfoMod); addAndMakeVisible (oscMod);
        addAndMakeVisible (mixMod); addAndMakeVisible (filtMod); addAndMakeVisible (outMod);
        addAndMakeVisible (macroMod); addAndMakeVisible (lowerMod);
        addAndMakeVisible (wtDisplay); addAndMakeVisible (filtResp);
        addAndMakeVisible (adsr); addAndMakeVisible (scope); addAndMakeVisible (meter);

        auto knob = [&] (const juce::String& id_, const juce::String& nm, juce::Colour arc = {})
        { auto* k = knobs.add (std::make_unique<LabeledKnob> (s, id_, nm, lf, true, arc)); addAndMakeVisible (k); return k; };
        auto hslider = [&] (const juce::String& id_, const juce::String& nm)
        { auto* k = knobs.add (std::make_unique<LabeledKnob> (s, id_, nm, lf, false)); addAndMakeVisible (k); return k; };

        // ENV1 ADSR
        envA = knob (id::env (1, "attack"), "ATK", theme::cyan);
        envD = knob (id::env (1, "decay"),  "DEC", theme::cyan);
        envS = knob (id::env (1, "sustain"),"SUS", theme::cyan);
        envR = knob (id::env (1, "release"),"REL", theme::cyan);

        // LFO1
        lfoRate  = knob (id::lfo (1, "ratehz"), "RATE", theme::pink);
        lfoDepth = knob (id::lfo (1, "depth"),  "DEPTH", theme::pink);
        lfoRise  = knob (id::lfo (1, "rise"),   "RISE", theme::pink);

        // OSC A
        oscWt   = knob (id::osc ('A', "wtpos"),  "WT POS");
        oscWarp = knob (id::osc ('A', "warp"),   "WARP");
        oscUni  = knob (id::osc ('A', "unison"), "UNISON");
        oscDet  = knob (id::osc ('A', "detune"), "DETUNE");
        oscLvl  = hslider (id::osc ('A', "level"), "LEVEL");
        oscPan  = hslider (id::osc ('A', "pan"),   "PAN");

        // Source mixer (level sliders + enables)
        std::array<const char*, 4> srcIds  { "oscA_level", "oscB_level", "sub_level", "noise_level" };
        std::array<const char*, 4> srcEn   { "oscA_enable", "oscB_enable", "sub_enable", "noise_enable" };
        std::array<const char*, 4> srcName { "OSC A", "OSC B", "SUB", "NOISE" };
        for (int i = 0; i < 4; ++i)
        {
            mixLvl[i] = knobs.add (std::make_unique<LabeledKnob> (s, srcIds[i], srcName[i], lf, false));
            addAndMakeVisible (mixLvl[i]);
            mixEn[i] = makeToggle (srcEn[i], "ON");
        }

        // Filter
        cutoff = knob (id::filterCutoff, "CUTOFF", theme::accent);
        reso   = knob (id::filterReso,   "RESO", theme::accent);
        drive  = knob (id::filterDrive,  "DRIVE", theme::accent);
        fmix   = knob (id::filterMix,    "MIX", theme::accent);
        filterType = makeCombo (id::filterType, choices::filterType());
        filterEnable = makeToggle (id::filterEnable, "FILTER");
        std::array<const char*, 4> rId { "filter_routeA", "filter_routeB", "filter_routeS", "filter_routeN" };
        std::array<const char*, 4> rNm { "A", "B", "S", "N" };
        for (int i = 0; i < 4; ++i) route[i] = makeToggle (rId[i], rNm[i]);

        // Output
        master = knob (id::masterOut, "MASTER", theme::accent);

        // Macros
        for (int i = 0; i < 4; ++i)
            macro[i] = knob (id::macro (i + 1), "MACRO " + juce::String (i + 1), theme::accent);

        // Header: preset stepper + save + browser (well is clickable, see mouseDown)
        addAndMakeVisible (prevBtn); prevBtn.setButtonText ("<"); prevBtn.setLookAndFeel (&lf);
        addAndMakeVisible (nextBtn); nextBtn.setButtonText (">"); nextBtn.setLookAndFeel (&lf);
        prevBtn.onClick = [this] { stepPreset (-1); };
        nextBtn.onClick = [this] { stepPreset (1); };
        addAndMakeVisible (saveBtn); saveBtn.setButtonText ("SAVE"); saveBtn.setLookAndFeel (&lf);
        saveBtn.onClick = [this] { openSaveDialog(); };
        presetBrowser.onLoadFactory = [this] (int i)         { loadCombined (i); };
        presetBrowser.onLoadUser    = [this] (juce::String n) { loadUserByName (n); };
        presetBrowser.onSave        = [this]                  { openSaveDialog(); };
        presetTotal = proc.getPresetManager().getNumFactory()
                    + proc.getPresetManager().getUserPresetNames().size();

        // Lower workspace tabs
        buildLowerTabs();

        // Keyboard
        keyboard.setLowestVisibleKey (36);
        addAndMakeVisible (keyboard);

        startTimerHz (8);
    }

    ~ZWPanel() override
    {
        stopTimer();
        for (auto* b : toggles) b->setLookAndFeel (nullptr);
        for (auto* c : combos) c->setLookAndFeel (nullptr);
        prevBtn.setLookAndFeel (nullptr); nextBtn.setLookAndFeel (nullptr);
        saveBtn.setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        auto full = getLocalBounds().toFloat();
        g.setGradientFill (theme::panelGradient (full));
        g.fillRoundedRectangle (full, (float) theme::rPanel);
        g.setColour (theme::wa (0.06f));
        g.drawRoundedRectangle (full.reduced (0.5f), (float) theme::rPanel, 1.0f);

        // Header wordmark
        g.setFont (lnf.displayFont (22.0f, true));
        g.setColour (theme::t1);
        g.drawText ("Zanders", 20, 14, 110, 28, juce::Justification::centredLeft, false);
        g.setColour (theme::cyan);
        g.drawText ("Wave", 118, 14, 80, 28, juce::Justification::centredLeft, false);
        // Badge
        auto badge = juce::Rectangle<float> (196, 18, 86, 20);
        g.setColour (theme::accent.withAlpha (0.18f)); g.fillRoundedRectangle (badge, 10.0f);
        g.setColour (theme::accent); g.setFont (lnf.displayFont (9.0f, true).withExtraKerningFactor (0.16f));
        g.drawText ("WAVETABLE", badge, juce::Justification::centred, false);

        // Preset well + name
        auto well = juce::Rectangle<float> (470, 14, 300, 28);
        g.setColour (theme::well); g.fillRoundedRectangle (well, 8.0f);
        g.setColour (theme::wa (0.08f)); g.drawRoundedRectangle (well, 8.0f, 1.0f);
        g.setColour (theme::cyan); g.fillEllipse (well.getX() + 12, well.getCentreY() - 3, 6, 6);
        g.setColour (theme::t1); g.setFont (lnf.displayFont (13.0f, true));
        g.drawText (presetName, well.reduced (28, 0), juce::Justification::centredLeft, false);
        g.setColour (theme::tMuted); g.setFont (lnf.monoFont (10.0f));
        g.drawText (juce::String (presetIndex + 1) + " / " + juce::String (juce::jmax (1, presetTotal)),
                    well.reduced (10, 0), juce::Justification::centredRight, false);

        // Voice readout
        g.setColour (theme::tMuted); g.setFont (lnf.monoFont (10.0f));
        g.drawText (juce::String (proc.getActiveVoiceCount()) + " VOICES   MPE · 16",
                    getWidth() - 320, 14, 200, 28, juce::Justification::centredRight, false);

        // Mod-sources bar label
        g.setColour (theme::tLabel); g.setFont (lnf.displayFont (9.5f, true).withExtraKerningFactor (0.16f));
        g.drawText ("MOD SOURCES", 20, modBarY, 110, 18, juce::Justification::centredLeft, false);
        std::array<const char*, 7> chips { "ENV1", "ENV2", "ENV3", "LFO1", "LFO2", "VEL", "NOTE" };
        int cx = 130;
        for (auto* c : chips)
        {
            auto chip = juce::Rectangle<float> ((float) cx, (float) modBarY - 1, 54, 20);
            g.setColour (theme::wa (0.05f)); g.fillRoundedRectangle (chip, 8.0f);
            g.setColour (theme::srcColour (c)); g.fillEllipse (chip.getX() + 7, chip.getCentreY() - 3, 6, 6);
            g.setColour (theme::t3); g.setFont (lnf.displayFont (9.0f, true));
            g.drawText (c, chip.withTrimmedLeft (16), juce::Justification::centredLeft, false);
            cx += 62;
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop (52);                         // header
        // preset stepper buttons + save in header
        prevBtn.setBounds (444, 17, 22, 22);
        nextBtn.setBounds (774, 17, 22, 22);
        saveBtn.setBounds (806, 16, 60, 24);
        meter.setBounds (getWidth() - 150, 22, 120, 12);

        auto main = r.removeFromTop (560).reduced (16, 8);
        auto left = main.removeFromLeft (304);
        auto right = main.removeFromRight (344);
        main.removeFromLeft (12); main.removeFromRight (12);
        auto centre = main;

        // Left rail: ENV + LFO
        auto envArea = left.removeFromTop (300); envMod.setBounds (envArea);
        { auto b = envMod.body(); adsr.setBounds (b.removeFromTop (96)); b.removeFromTop (8);
          auto row = b.removeFromTop (90); const int kw = row.getWidth() / 4;
          envA->setBounds (row.removeFromLeft (kw)); envD->setBounds (row.removeFromLeft (kw));
          envS->setBounds (row.removeFromLeft (kw)); envR->setBounds (row); }
        left.removeFromTop (12);
        lfoMod.setBounds (left);
        { auto b = lfoMod.body(); auto row = b.removeFromTop (90); const int kw = row.getWidth() / 3;
          lfoRate->setBounds (row.removeFromLeft (kw)); lfoDepth->setBounds (row.removeFromLeft (kw)); lfoRise->setBounds (row); }

        // Centre: OSC editor + source mixer
        auto oscArea = centre.removeFromTop (380); oscMod.setBounds (oscArea);
        { auto b = oscMod.body(); wtDisplay.setBounds (b.removeFromTop (190)); b.removeFromTop (10);
          auto row = b.removeFromTop (90); const int kw = row.getWidth() / 4;
          oscWt->setBounds (row.removeFromLeft (kw)); oscWarp->setBounds (row.removeFromLeft (kw));
          oscUni->setBounds (row.removeFromLeft (kw)); oscDet->setBounds (row);
          b.removeFromTop (6); auto srow = b.removeFromTop (44);
          oscLvl->setBounds (srow.removeFromLeft (srow.getWidth() / 2).reduced (4, 0));
          oscPan->setBounds (srow.reduced (4, 0)); }
        centre.removeFromTop (12);
        mixMod.setBounds (centre);
        { auto b = mixMod.body(); const int cw = b.getWidth() / 4;
          for (int i = 0; i < 4; ++i) { auto col = b.removeFromLeft (cw).reduced (4);
            mixEn[i]->setBounds (col.removeFromTop (20).reduced (12, 0));
            mixLvl[i]->setBounds (col); } }

        // Right rail: Filter + Output
        auto filtArea = right.removeFromTop (340); filtMod.setBounds (filtArea);
        { auto b = filtMod.body();
          { auto tr = b.removeFromTop (22); filterEnable->setBounds (tr.removeFromLeft (90));
            filterType->setBounds (tr.removeFromRight (140)); }
          b.removeFromTop (4); filtResp.setBounds (b.removeFromTop (110)); b.removeFromTop (6);
          auto row = b.removeFromTop (88); const int kw = row.getWidth() / 4;
          cutoff->setBounds (row.removeFromLeft (kw)); reso->setBounds (row.removeFromLeft (kw));
          drive->setBounds (row.removeFromLeft (kw)); fmix->setBounds (row);
          b.removeFromTop (4); auto rr = b.removeFromTop (22); const int rw = rr.getWidth() / 4;
          for (auto* r2 : route) r2->setBounds (rr.removeFromLeft (rw).reduced (2, 0)); }
        right.removeFromTop (12);
        outMod.setBounds (right);
        { auto b = outMod.body(); scope.setBounds (b.removeFromTop (70)); b.removeFromTop (6);
          master->setBounds (b.removeFromLeft (b.getWidth() / 2)); }

        // Mod-sources bar
        modBarY = r.getY() + 6;
        r.removeFromTop (34);

        // Lower workspace
        auto lower = r.removeFromTop (200).reduced (16, 0);
        lowerMod.setBounds (lower);
        { auto b = lowerMod.body();
          auto tabRow = b.removeFromTop (24);
          const int tw = tabRow.getWidth() / 4;
          for (int i = 0; i < 4; ++i) tabBtn[i]->setBounds (tabRow.removeFromLeft (tw).reduced (2, 0));
          b.removeFromTop (6);
          for (auto* t : tabPages) t->setBounds (b);
          layoutPages(); }

        // Footer: macros + keyboard
        auto footer = r.reduced (16, 6);
        macroMod.setBounds (footer.removeFromLeft (300));
        { auto b = macroMod.body(); const int kw = b.getWidth() / 4;
          for (auto* m : macro) m->setBounds (b.removeFromLeft (kw)); }
        footer.removeFromLeft (12);
        keyboard.setBounds (footer);
    }

private:
    void timerCallback() override
    {
        // Mirror the host program name only while a factory program is selected;
        // user presets aren't host programs, so keep the name set at load time.
        if (presetIndex < proc.getPresetManager().getNumFactory())
        {
            presetIndex = proc.getCurrentProgram();
            presetName = proc.getProgramName (presetIndex);
            if (presetName.isEmpty()) presetName = "Init";
        }
        repaint (0, 0, getWidth(), 52);
    }

    // ---- Preset selection (combined factory + user list) ----
    void loadCombined (int idx)
    {
        auto& pm = proc.getPresetManager();
        const int nf = pm.getNumFactory();
        const int total = nf + pm.getUserPresetNames().size();
        if (total <= 0) return;

        presetTotal = total;
        idx = ((idx % total) + total) % total;          // wrap
        presetIndex = idx;

        if (idx < nf)
        {
            proc.setCurrentProgram (idx);               // host-sync + factory apply
            presetName = pm.factoryName (idx);
        }
        else
        {
            auto names = pm.getUserPresetNames();
            const auto name = names[idx - nf];
            pm.loadUserPreset (name);
            presetName = name;
        }
        repaint();
    }

    void loadUserByName (const juce::String& name)
    {
        auto& pm = proc.getPresetManager();
        const int pos = pm.getUserPresetNames().indexOf (name);
        if (pos >= 0)
            loadCombined (pm.getNumFactory() + pos);
    }

    void stepPreset (int dir) { loadCombined (presetIndex + dir); }

    void openBrowser()
    {
        auto& pm = proc.getPresetManager();
        presetTotal = pm.getNumFactory() + pm.getUserPresetNames().size();
        const auto well = juce::Rectangle<int> (470, 14, 300, 28);
        presetBrowser.show (this, localAreaToGlobal (well), presetIndex);
    }

    void openSaveDialog()
    {
        saveDialog = std::make_unique<juce::AlertWindow> (
            "Save Preset", "Enter a name for this preset:", juce::MessageBoxIconType::NoIcon);
        saveDialog->addTextEditor ("name", presetName, "Name:");
        saveDialog->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
        saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        saveDialog->setLookAndFeel (&lnf);
        saveDialog->enterModalState (true, juce::ModalCallbackFunction::create (
            [this] (int result)
            {
                if (result == 1)
                {
                    const auto name = saveDialog->getTextEditorContents ("name").trim();
                    if (name.isNotEmpty() && proc.getPresetManager().saveUserPreset (name))
                    {
                        // New preset lives under the user section now; reflect it and
                        // point the combined index at it (browser re-scans disk on open).
                        loadUserByName (name);
                    }
                }
                saveDialog->setLookAndFeel (nullptr);
                saveDialog.reset();
            }), false);
    }

    // Open the browser when the preset well is clicked.
    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto well = juce::Rectangle<int> (470, 14, 300, 28);
        if (well.contains (e.getPosition()))
            openBrowser();
    }

    juce::TextButton* makeToggle (const juce::String& id, const juce::String& text)
    {
        auto* b = toggles.add (std::make_unique<juce::TextButton> (text));
        b->setClickingTogglesState (true);
        b->setLookAndFeel (&lnf);
        addAndMakeVisible (b);
        tAtt.add (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id, *b));
        return b;
    }

    juce::ComboBox* makeCombo (const juce::String& id, const juce::StringArray& items)
    {
        auto* c = combos.add (std::make_unique<juce::ComboBox>());
        c->addItemList (items, 1);
        c->setLookAndFeel (&lnf);
        addAndMakeVisible (c);
        cAtt.add (std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id, *c));
        return c;
    }

    void buildLowerTabs()
    {
        std::array<const char*, 4> names { "FX RACK", "MOD MATRIX", "ARP", "WAVETABLE" };
        for (int i = 0; i < 4; ++i)
        {
            auto* b = tabBtn.add (std::make_unique<juce::TextButton> (names[i]));
            b->setClickingTogglesState (true);
            b->setRadioGroupId (100);
            b->setLookAndFeel (&lnf);
            b->onClick = [this, i] { showTab (i); };
            addAndMakeVisible (b);
        }

        // FX page: per-effect parameter editor (picker + enable + param knobs).
        // Replaces the old bare row of 10 enable toggles; lays itself out.
        auto* fxPage = tabPages.add (std::make_unique<zw::FxRackPanel> (proc.apvts, lnf));
        fxRackPage = fxPage; addChildComponent (fxPage);

        // Matrix page: editable mod-matrix route list
        auto* mxPage = tabPages.add (std::make_unique<zw::ModMatrixPanel> (proc.getModMatrix(), lnf));
        matrixPage = mxPage; addChildComponent (mxPage);

        // Arp page: run + rate + mode + 16 steps
        auto* arpPage = tabPages.add (std::make_unique<juce::Component>());
        arpRun  = makeToggleOn (arpPage, id::arpEnable, "RUN");
        arpRate = makeComboOn (arpPage, id::arpRate, choices::arpRate());
        arpMode = makeComboOn (arpPage, id::arpMode, choices::arpMode());
        for (int i = 0; i < 16; ++i)
            arpStep[i] = makeToggleOn (arpPage, id::arpStep (i + 1), juce::String (i + 1));
        arpPageComp = arpPage; addChildComponent (arpPage);

        // Wavetable page: per-osc table selectors + .wav import + frame slider.
        auto* wtPage = tabPages.add (std::make_unique<juce::Component>());
        wtSelectA = makeComboOn (wtPage, id::osc ('A', "wtselect"), choices::wavetable());
        wtSelectB = makeComboOn (wtPage, id::osc ('B', "wtselect"), choices::wavetable());
        wtImport  = toggles.add (std::make_unique<juce::TextButton> ("IMPORT WAV..."));
        wtImport->setLookAndFeel (&lnf);
        wtPage->addAndMakeVisible (wtImport);
        wtImport->onClick = [this]
        {
            // Import on the message thread: read + band-limit the .wav into the
            // library's user slot, then point both selectors at "User Import".
            fileChooser = std::make_unique<juce::FileChooser> ("Import wavetable (.wav)",
                                                               juce::File{}, "*.wav");
            const auto flags = juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles;
            fileChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();
                if (file.existsAsFile() && proc.getWavetableLibrary().loadFromWav (file))
                {
                    const int userIdx = proc.getWavetableLibrary().getUserIndex() + 1; // 1-based combo id
                    if (wtSelectA != nullptr) wtSelectA->setSelectedId (userIdx);
                    if (wtSelectB != nullptr) wtSelectB->setSelectedId (userIdx);
                }
            });
        };
        wtFrame = knobs.add (std::make_unique<LabeledKnob> (proc.apvts, id::osc ('A', "wtpos"), "FRAME POSITION", lnf, false));
        wtPage->addAndMakeVisible (wtFrame);
        wtPageComp = wtPage; addChildComponent (wtPage);

        tabBtn[0]->setToggleState (true, juce::dontSendNotification);
        showTab (0);
    }

    juce::TextButton* makeToggleOn (juce::Component* parent, const juce::String& id, const juce::String& text)
    {
        auto* b = toggles.add (std::make_unique<juce::TextButton> (text)); b->setClickingTogglesState (true); b->setLookAndFeel (&lnf);
        parent->addAndMakeVisible (b);
        tAtt.add (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id, *b));
        return b;
    }
    juce::ComboBox* makeComboOn (juce::Component* parent, const juce::String& id, const juce::StringArray& items)
    {
        auto* c = combos.add (std::make_unique<juce::ComboBox>()); c->addItemList (items, 1); c->setLookAndFeel (&lnf);
        parent->addAndMakeVisible (c);
        cAtt.add (std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id, *c));
        return c;
    }

    void showTab (int which)
    {
        for (int i = 0; i < tabPages.size(); ++i) tabPages[i]->setVisible (i == which);
        layoutPages();
    }

    void layoutPages()
    {
        if (tabPages.isEmpty()) return;
        auto area = tabPages[0]->getLocalBounds();
        // FX page (FxRackPanel) lays itself out via its own resized().
        if (arpPageComp != nullptr)
        { auto b = arpPageComp->getLocalBounds(); auto top = b.removeFromTop (24);
          arpRun->setBounds (top.removeFromLeft (70)); top.removeFromLeft (8);
          arpRate->setBounds (top.removeFromLeft (110)); top.removeFromLeft (8);
          arpMode->setBounds (top.removeFromLeft (130));
          b.removeFromTop (10); auto grid = b.removeFromTop (60); const int sw = grid.getWidth() / 16;
          for (auto* st : arpStep) st->setBounds (grid.removeFromLeft (sw).reduced (2)); }
        if (wtPageComp != nullptr)
        {
            auto b = wtPageComp->getLocalBounds();
            auto top = b.removeFromTop (24);
            if (wtSelectA != nullptr) { wtSelectA->setBounds (top.removeFromLeft (160)); top.removeFromLeft (8); }
            if (wtSelectB != nullptr) { wtSelectB->setBounds (top.removeFromLeft (160)); top.removeFromLeft (8); }
            if (wtImport  != nullptr) { wtImport->setBounds  (top.removeFromLeft (130)); }
            b.removeFromTop (10);
            wtFrame->setBounds (b.removeFromTop (60));
        }
        juce::ignoreUnused (area);
    }

    ZandersWaveAudioProcessor& proc;
    ZWLookAndFeel& lnf;

    Module envMod;
    Module lfoMod;
    Module oscMod;
    Module mixMod;
    Module filtMod;
    Module outMod;
    Module macroMod;
    Module lowerMod;
    WavetableDisplay wtDisplay;
    FilterResponse filtResp;
    AdsrDisplay adsr;
    Scope scope;
    LevelMeter meter;
    juce::MidiKeyboardComponent keyboard;

    juce::OwnedArray<LabeledKnob> knobs;
    juce::OwnedArray<juce::TextButton> toggles;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<APVTS::ButtonAttachment> tAtt;
    juce::OwnedArray<APVTS::ComboBoxAttachment> cAtt;
    juce::OwnedArray<juce::Component> tabPages;
    juce::OwnedArray<juce::TextButton> tabBtn;

    LabeledKnob* envA{};
    LabeledKnob* envD{};
    LabeledKnob* envS{};
    LabeledKnob* envR{};
    LabeledKnob* lfoRate{};
    LabeledKnob* lfoDepth{};
    LabeledKnob* lfoRise{};
    LabeledKnob* oscWt{};
    LabeledKnob* oscWarp{};
    LabeledKnob* oscUni{};
    LabeledKnob* oscDet{};
    LabeledKnob* oscLvl{};
    LabeledKnob* oscPan{};
    std::array<LabeledKnob*, 4> mixLvl{};
    LabeledKnob* cutoff{};
    LabeledKnob* reso{};
    LabeledKnob* drive{};
    LabeledKnob* fmix{};
    LabeledKnob* master{};
    std::array<LabeledKnob*, 4> macro{};
    LabeledKnob* wtFrame{};
    std::array<juce::TextButton*, 4> mixEn{};
    juce::TextButton* filterEnable{};
    std::array<juce::TextButton*, 4> route{};
    juce::TextButton* arpRun{};
    std::array<juce::TextButton*, 16> arpStep{};
    juce::ComboBox* filterType{};
    juce::ComboBox* arpRate{};
    juce::ComboBox* arpMode{};
    juce::Component* arpPageComp{};
    juce::Component* wtPageComp{};
    juce::Component* matrixPage{};

    juce::TextButton prevBtn;
    juce::TextButton nextBtn;
    juce::TooltipWindow tooltip { this, 600 };
    juce::String presetName { "Init" };
    int modBarY = 620;

    //==========================================================================
    // Wavetable library controls (WAVETABLE lower tab) — see buildLowerTabs()
    // wtPage block and layoutPages(). Per-osc table selectors + .wav importer.
    juce::ComboBox*  wtSelectA{};
    juce::ComboBox*  wtSelectB{};
    juce::TextButton* wtImport{};
    std::unique_ptr<juce::FileChooser> fileChooser;
    //==========================================================================

    // ---- Preset browser (header) ----
    juce::TextButton saveBtn;
    zw::PresetBrowser presetBrowser { proc.getPresetManager(), lnf };
    std::unique_ptr<juce::AlertWindow> saveDialog;
    int presetIndex = 0;   // combined factory+user selection index
    int presetTotal = 0;   // cached factory+user count for the well counter

    //==========================================================================
    // FX rack editor (FX RACK lower tab) — see buildLowerTabs() fxPage block.
    // Owned by tabPages; this is a non-owning back-pointer kept for symmetry
    // with the other tab pages (matrixPage/arpPageComp/wtPageComp).
    juce::Component* fxRackPage{};
    //==========================================================================

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZWPanel)
};

} // namespace zw

//==============================================================================
ZandersWaveAudioProcessorEditor::ZandersWaveAudioProcessorEditor (ZandersWaveAudioProcessor& p)
    : AudioProcessorEditor (&p)
{
    setLookAndFeel (&lnf);
    panel = std::make_unique<zw::ZWPanel> (p, lnf);
    addAndMakeVisible (*panel);
    panel->setSize (kDesignW, kDesignH);

    setResizable (true, true);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) kDesignW / (double) kDesignH);
    setResizeLimits (kDesignW / 2, kDesignH / 2, kDesignW * 2, kDesignH * 2);
    setSize (kDesignW, kDesignH);
}

ZandersWaveAudioProcessorEditor::~ZandersWaveAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void ZandersWaveAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (zw::theme::pageBg);
}

void ZandersWaveAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) kDesignW;
    if (panel != nullptr)
    {
        panel->setTransform (juce::AffineTransform::scale (scale));
        panel->setBounds (0, 0, kDesignW, kDesignH);
    }
}
