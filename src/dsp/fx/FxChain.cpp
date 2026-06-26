#include "FxChain.h"
#include "../../Parameters.h"

namespace zw
{

void FxChain::prepare (const juce::dsp::ProcessSpec& spec)
{
    hyper.prepare (spec);   distort.prepare (spec); flanger.prepare (spec); phaser.prepare (spec);
    chorus.prepare (spec);  delay.prepare (spec);   reverb.prepare (spec);  comp.prepare (spec);
    eq.prepare (spec);      filterFx.prepare (spec);
}

void FxChain::reset()
{
    hyper.reset(); distort.reset(); flanger.reset(); phaser.reset(); chorus.reset();
    delay.reset(); reverb.reset(); comp.reset(); eq.reset(); filterFx.reset();
}

void FxChain::prepareParams (const juce::AudioProcessorValueTreeState& s)
{
    auto g = [&s] (const char* slot, const char* p) { return s.getRawParameterValue (id::fx (slot, p)); };

    hy.enable  = g ("hyper", "enable");  hy.p1 = g ("hyper", "detune");  hy.p2 = g ("hyper", "voices");  hy.p3 = g ("hyper", "width");  hy.p4 = g ("hyper", "mix");
    di.enable  = g ("distort","enable"); di.p1 = g ("distort","drive");  di.p2 = g ("distort","tone");   di.p3 = g ("distort","mix");   di.p4 = g ("distort","out");   di.p5 = g ("distort","mode");
    fl.enable  = g ("flanger","enable"); fl.p1 = g ("flanger","rate");   fl.p2 = g ("flanger","depth");  fl.p3 = g ("flanger","feedback"); fl.p4 = g ("flanger","mix");
    ph.enable  = g ("phaser","enable");  ph.p1 = g ("phaser","rate");    ph.p2 = g ("phaser","depth");   ph.p3 = g ("phaser","stages"); ph.p4 = g ("phaser","mix");
    ch.enable  = g ("chorus","enable");  ch.p1 = g ("chorus","rate");    ch.p2 = g ("chorus","depth");   ch.p3 = g ("chorus","voices"); ch.p4 = g ("chorus","mix");
    dl.enable  = g ("delay","enable");   dl.p1 = g ("delay","time");     dl.p2 = g ("delay","feedback"); dl.p3 = g ("delay","width");   dl.p4 = g ("delay","mix");
    rv.enable  = g ("reverb","enable");  rv.p1 = g ("reverb","size");    rv.p2 = g ("reverb","decay");   rv.p3 = g ("reverb","damp");   rv.p4 = g ("reverb","mix");
    cp.enable  = g ("comp","enable");    cp.p1 = g ("comp","threshold"); cp.p2 = g ("comp","ratio");     cp.p3 = g ("comp","attack");   cp.p4 = g ("comp","makeup");
    eqp.enable = g ("eq","enable");      eqp.p1 = g ("eq","low");        eqp.p2 = g ("eq","lomid");      eqp.p3 = g ("eq","himid");     eqp.p4 = g ("eq","high");
    fi.enable  = g ("filter","enable");  fi.p1 = g ("filter","cutoff");  fi.p2 = g ("filter","reso");    fi.p3 = g ("filter","type");   fi.p4 = g ("filter","mix");
}

void FxChain::process (juce::dsp::AudioBlock<float>& block)
{
    if (on (hy.enable))  { hyper.setParams   (val (hy.p1), (int) val (hy.p2), val (hy.p3), val (hy.p4)); hyper.process (block); }
    if (on (di.enable))  { distort.setParams (val (di.p1), val (di.p2), val (di.p3), val (di.p4), (int) val (di.p5)); distort.process (block); }
    if (on (fl.enable))  { flanger.setParams (val (fl.p1), val (fl.p2), val (fl.p3), val (fl.p4)); flanger.process (block); }
    if (on (ph.enable))  { phaser.setParams  (val (ph.p1), val (ph.p2), (int) val (ph.p3), val (ph.p4)); phaser.process (block); }
    if (on (ch.enable))  { chorus.setParams  (val (ch.p1), val (ch.p2), (int) val (ch.p3), val (ch.p4)); chorus.process (block); }
    if (on (dl.enable))  { delay.setParams   (val (dl.p1), val (dl.p2), val (dl.p3), val (dl.p4)); delay.process (block); }
    if (on (rv.enable))  { reverb.setParams  (val (rv.p1), val (rv.p2), val (rv.p3), val (rv.p4)); reverb.process (block); }
    if (on (cp.enable))  { comp.setParams    (val (cp.p1), val (cp.p2), val (cp.p3), val (cp.p4)); comp.process (block); }
    if (on (eqp.enable)) { eq.setParams      (val (eqp.p1), val (eqp.p2), val (eqp.p3), val (eqp.p4)); eq.process (block); }
    if (on (fi.enable))  { filterFx.setParams (val (fi.p1), val (fi.p2), (int) val (fi.p3), val (fi.p4)); filterFx.process (block); }
}

} // namespace zw
