#include "PluginEditor.h"
#include "ui/Theme.h"
#include "ui/Displays.h"
#include "Parameters.h"

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
        if (auto* rp = s.getParameter (id))
        {
            slider.textFromValueFunction = [rp] (double v)
            { return rp->getText (rp->getNormalisableRange().convertTo0to1 ((float) v), 0); };
            slider.valueFromTextFunction = [rp] (const juce::String& t)
            { return (double) rp->getNormalisableRange().convertFrom0to1 (rp->getValueForText (t)); };
        }
        if (arc != juce::Colour()) slider.setColour (juce::Slider::rotarySliderFillColourId, arc);
        slider.setLookAndFeel (&lf);
        slider.setTooltip (name);
        if (auto* p = s.getParameter (id))
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
    Module (const juce::String& t, ZWLookAndFeel& lf) : title (t)
    {
        titleFont = lf.displayFont (10.5f, true).withExtraKerningFactor (0.16f);
    }
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
        { auto* k = new LabeledKnob (s, id_, nm, lf, true, arc); knobs.add (k); addAndMakeVisible (k); return k; };
        auto hslider = [&] (const juce::String& id_, const juce::String& nm)
        { auto* k = new LabeledKnob (s, id_, nm, lf, false); knobs.add (k); addAndMakeVisible (k); return k; };

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
        const char* srcIds[4]  = { "oscA_level", "oscB_level", "sub_level", "noise_level" };
        const char* srcEn[4]   = { "oscA_enable", "oscB_enable", "sub_enable", "noise_enable" };
        const char* srcName[4] = { "OSC A", "OSC B", "SUB", "NOISE" };
        for (int i = 0; i < 4; ++i)
        {
            mixLvl[i] = new LabeledKnob (s, srcIds[i], srcName[i], lf, false);
            knobs.add (mixLvl[i]); addAndMakeVisible (mixLvl[i]);
            mixEn[i] = makeToggle (srcEn[i], "ON");
        }

        // Filter
        cutoff = knob (id::filterCutoff, "CUTOFF", theme::accent);
        reso   = knob (id::filterReso,   "RESO", theme::accent);
        drive  = knob (id::filterDrive,  "DRIVE", theme::accent);
        fmix   = knob (id::filterMix,    "MIX", theme::accent);
        filterType = makeCombo (id::filterType, choices::filterType());
        filterEnable = makeToggle (id::filterEnable, "FILTER");
        const char* rId[4] = { "filter_routeA", "filter_routeB", "filter_routeS", "filter_routeN" };
        const char* rNm[4] = { "A", "B", "S", "N" };
        for (int i = 0; i < 4; ++i) route[i] = makeToggle (rId[i], rNm[i]);

        // Output
        master = knob (id::masterOut, "MASTER", theme::accent);

        // Macros
        for (int i = 0; i < 4; ++i)
            macro[i] = knob (id::macro (i + 1), "MACRO " + juce::String (i + 1), theme::accent);

        // Header: preset stepper
        addAndMakeVisible (prevBtn); prevBtn.setButtonText ("<"); prevBtn.setLookAndFeel (&lf);
        addAndMakeVisible (nextBtn); nextBtn.setButtonText (">"); nextBtn.setLookAndFeel (&lf);
        prevBtn.onClick = [this] { stepPreset (-1); };
        nextBtn.onClick = [this] { stepPreset (1); };

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
        g.drawText (juce::String (proc.getCurrentProgram() + 1) + " / " + juce::String (proc.getNumPrograms()),
                    well.reduced (10, 0), juce::Justification::centredRight, false);

        // Voice readout
        g.setColour (theme::tMuted); g.setFont (lnf.monoFont (10.0f));
        g.drawText (juce::String (proc.getActiveVoiceCount()) + " VOICES   MPE · 16",
                    getWidth() - 320, 14, 200, 28, juce::Justification::centredRight, false);

        // Mod-sources bar label
        g.setColour (theme::tLabel); g.setFont (lnf.displayFont (9.5f, true).withExtraKerningFactor (0.16f));
        g.drawText ("MOD SOURCES", 20, modBarY, 110, 18, juce::Justification::centredLeft, false);
        const char* chips[] = { "ENV1", "ENV2", "ENV3", "LFO1", "LFO2", "VEL", "NOTE" };
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
        // preset stepper buttons in header
        prevBtn.setBounds (444, 17, 22, 22);
        nextBtn.setBounds (774, 17, 22, 22);
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
          for (int i = 0; i < 4; ++i) route[i]->setBounds (rr.removeFromLeft (rw).reduced (2, 0)); }
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
          for (int i = 0; i < 4; ++i) macro[i]->setBounds (b.removeFromLeft (kw)); }
        footer.removeFromLeft (12);
        keyboard.setBounds (footer);
    }

private:
    void timerCallback() override
    {
        presetName = proc.getProgramName (proc.getCurrentProgram());
        if (presetName.isEmpty()) presetName = "Init";
        repaint (0, 0, getWidth(), 52);
    }

    void stepPreset (int dir)
    {
        const int n = proc.getNumPrograms();
        int idx = (proc.getCurrentProgram() + dir + n) % n;
        proc.setCurrentProgram (idx);
        repaint();
    }

    juce::TextButton* makeToggle (const juce::String& id, const juce::String& text)
    {
        auto* b = new juce::TextButton (text);
        b->setClickingTogglesState (true);
        b->setLookAndFeel (&lnf);
        toggles.add (b); addAndMakeVisible (b);
        tAtt.add (new APVTS::ButtonAttachment (proc.apvts, id, *b));
        return b;
    }

    juce::ComboBox* makeCombo (const juce::String& id, const juce::StringArray& items)
    {
        auto* c = new juce::ComboBox();
        c->addItemList (items, 1);
        c->setLookAndFeel (&lnf);
        combos.add (c); addAndMakeVisible (c);
        cAtt.add (new APVTS::ComboBoxAttachment (proc.apvts, id, *c));
        return c;
    }

    void buildLowerTabs()
    {
        const char* names[4] = { "FX RACK", "MOD MATRIX", "ARP", "WAVETABLE" };
        for (int i = 0; i < 4; ++i)
        {
            auto* b = new juce::TextButton (names[i]);
            b->setClickingTogglesState (true);
            b->setRadioGroupId (100);
            b->setLookAndFeel (&lnf);
            b->onClick = [this, i] { showTab (i); };
            tabBtn.add (b); addAndMakeVisible (b);
        }

        // FX page: 10 enable toggles
        auto* fxPage = new juce::Component();
        const char* fxSlots[10] = { "hyper","distort","flanger","phaser","chorus","delay","reverb","comp","eq","filter" };
        const char* fxNames[10] = { "Hyper","Distort","Flanger","Phaser","Chorus","Delay","Reverb","Comp","EQ","Filter" };
        for (int i = 0; i < 10; ++i)
        {
            auto* b = new juce::TextButton (fxNames[i]);
            b->setClickingTogglesState (true); b->setLookAndFeel (&lnf);
            toggles.add (b); fxPage->addChildComponent (b); b->setVisible (true);
            tAtt.add (new APVTS::ButtonAttachment (proc.apvts, id::fx (fxSlots[i], "enable"), *b));
            fxToggles[i] = b;
        }
        fxPage->setInterceptsMouseClicks (false, true);
        fxPageComp = fxPage; tabPages.add (fxPage); addChildComponent (fxPage);

        // Matrix page: route list (read-only label)
        auto* mxPage = new RouteList (proc.getPresetManager()); // placeholder using mod matrix via processor
        matrixPage = mxPage; tabPages.add (mxPage); addChildComponent (mxPage);

        // Arp page: run + rate + mode + 16 steps
        auto* arpPage = new juce::Component();
        arpRun  = makeToggleOn (arpPage, id::arpEnable, "RUN");
        arpRate = makeComboOn (arpPage, id::arpRate, choices::arpRate());
        arpMode = makeComboOn (arpPage, id::arpMode, choices::arpMode());
        for (int i = 0; i < 16; ++i)
            arpStep[i] = makeToggleOn (arpPage, id::arpStep (i + 1), juce::String (i + 1));
        arpPageComp = arpPage; tabPages.add (arpPage); addChildComponent (arpPage);

        // Wavetable page: frame slider (OSC A WT)
        auto* wtPage = new juce::Component();
        wtFrame = new LabeledKnob (proc.apvts, id::osc ('A', "wtpos"), "FRAME POSITION", lnf, false);
        knobs.add (wtFrame); wtPage->addAndMakeVisible (wtFrame);
        wtPageComp = wtPage; tabPages.add (wtPage); addChildComponent (wtPage);

        tabBtn[0]->setToggleState (true, juce::dontSendNotification);
        showTab (0);
    }

    juce::TextButton* makeToggleOn (juce::Component* parent, const juce::String& id, const juce::String& text)
    {
        auto* b = new juce::TextButton (text); b->setClickingTogglesState (true); b->setLookAndFeel (&lnf);
        toggles.add (b); parent->addAndMakeVisible (b);
        tAtt.add (new APVTS::ButtonAttachment (proc.apvts, id, *b));
        return b;
    }
    juce::ComboBox* makeComboOn (juce::Component* parent, const juce::String& id, const juce::StringArray& items)
    {
        auto* c = new juce::ComboBox(); c->addItemList (items, 1); c->setLookAndFeel (&lnf);
        combos.add (c); parent->addAndMakeVisible (c);
        cAtt.add (new APVTS::ComboBoxAttachment (proc.apvts, id, *c));
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
        // FX toggles row
        if (fxPageComp != nullptr)
        { auto b = fxPageComp->getLocalBounds(); const int w = b.getWidth() / 10;
          for (int i = 0; i < 10; ++i) fxToggles[i]->setBounds (b.removeFromLeft (w).reduced (3, 30)); }
        if (arpPageComp != nullptr)
        { auto b = arpPageComp->getLocalBounds(); auto top = b.removeFromTop (24);
          arpRun->setBounds (top.removeFromLeft (70)); top.removeFromLeft (8);
          arpRate->setBounds (top.removeFromLeft (110)); top.removeFromLeft (8);
          arpMode->setBounds (top.removeFromLeft (130));
          b.removeFromTop (10); auto grid = b.removeFromTop (60); const int sw = grid.getWidth() / 16;
          for (int i = 0; i < 16; ++i) arpStep[i]->setBounds (grid.removeFromLeft (sw).reduced (2)); }
        if (wtPageComp != nullptr)
            wtFrame->setBounds (wtPageComp->getLocalBounds().removeFromTop (60));
        juce::ignoreUnused (area);
    }

    // Read-only matrix route list.
    struct RouteList : public juce::Component
    {
        explicit RouteList (PresetManager&) {}
        void paint (juce::Graphics& g) override
        {
            g.setColour (theme::tMuted);
            g.setFont (juce::Font (juce::FontOptions (12.0f)));
            g.drawText ("Mod matrix routing (drag-to-assign UI coming next increment)",
                        getLocalBounds(), juce::Justification::centred, false);
        }
    };

    ZandersWaveAudioProcessor& proc;
    ZWLookAndFeel& lnf;

    Module envMod, lfoMod, oscMod, mixMod, filtMod, outMod, macroMod, lowerMod;
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

    LabeledKnob *envA{}, *envD{}, *envS{}, *envR{}, *lfoRate{}, *lfoDepth{}, *lfoRise{};
    LabeledKnob *oscWt{}, *oscWarp{}, *oscUni{}, *oscDet{}, *oscLvl{}, *oscPan{};
    LabeledKnob *mixLvl[4]{}, *cutoff{}, *reso{}, *drive{}, *fmix{}, *master{}, *macro[4]{}, *wtFrame{};
    juce::TextButton *mixEn[4]{}, *filterEnable{}, *route[4]{}, *arpRun{}, *arpStep[16]{}, *fxToggles[10]{};
    juce::ComboBox *filterType{}, *arpRate{}, *arpMode{};
    juce::Component *fxPageComp{}, *arpPageComp{}, *wtPageComp{}, *matrixPage{};

    juce::TextButton prevBtn, nextBtn;
    juce::TooltipWindow tooltip { this, 600 };
    juce::String presetName { "Init" };
    int modBarY = 620;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZWPanel)
};

} // namespace zw

//==============================================================================
ZandersWaveAudioProcessorEditor::ZandersWaveAudioProcessorEditor (ZandersWaveAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
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
