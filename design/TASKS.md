# Build Tasks — ZandersWave VST3

Ordered, checkable milestones for implementing the design in `README.md` as a real plugin (JUCE/C++ recommended). Work top-to-bottom; each milestone should compile, load in a DAW, and be testable before moving on. Check items off as you go.

## M0 — Project scaffold
- [ ] Create a JUCE plugin project (VST3 + AU), stereo synth, MIDI input enabled.
- [ ] Set up `AudioProcessorValueTreeState` (APVTS) as the single parameter store.
- [ ] Confirm it builds, loads in a DAW, and passes pluginval.

## M1 — Parameter tree
- [ ] Add every parameter from README "Full Parameter Spec" to the APVTS with correct ranges, defaults, skews (log for Cutoff/times), and units.
- [ ] Verify all params show up and automate in the DAW (no UI yet — generic editor is fine).

## M2 — Voice engine (core sound)
- [ ] Voice allocator: polyphonic (16+), note-on/off, velocity, voice stealing.
- [ ] OSC A & B: wavetable playback with frame interpolation (WT Position) + unison (count, detune) + octave/pan/level.
- [ ] SUB (sine/tri/square + octave + saturate) and NOISE (white/pink/vinyl + color).
- [ ] Multimode filter (LP24/12, HP24/12, BP12, Notch) with cutoff/reso/drive + per-source routing.
- [ ] ENV1 → amp VCA (ADSR). Sound should play from MIDI.

## M3 — Modulation
- [ ] ENV2/ENV3, LFO1–4 (shapes, rate w/ tempo sync, depth, rise), Macros 1–4.
- [ ] Mod matrix: list of {source, dest, amount}; sum per-destination per block; clamp to range.
- [ ] Param smoothing on all modulated/automated values.

## M4 — FX rack
- [ ] Serial chain of 10 bypassable slots (Hyper, Distort, Flanger, Phaser, Chorus, Delay, Reverb, Comp, EQ, Filter).
- [ ] Implement each effect's full parameter set (README lists representative dials — expand).

## M5 — Arp + global/voicing
- [ ] Arpeggiator: 16-step pattern, rate, mode, octave range; feeds the voice engine.
- [ ] Global: polyphony, mono/legato, glide/portamento, pitch-bend range, MPE toggle.

## M6 — Presets & state
- [ ] Full state save/load via APVTS (DAW session recall).
- [ ] Preset format + factory bank + user save/load + a browser.
- [ ] Ship a wavetable library (frames) and bundle the two fonts.

## M7 — UI (match the prototype)
- [ ] Custom `LookAndFeel` using the README Design Tokens (spectrum, accent, glows, wells, radii, fonts).
- [ ] Build the regions: header/preset browser, left rail (ENV+LFO), center hero (source-switched 3D wavetable + source mixer), right rail (filter response + output scope), mod-sources bar, lower tabs (FX/Matrix/Arp/Wavetable), footer (macros + wheels + keyboard).
- [ ] Live displays: 3D wavetable, filter response curve, ADSR editor, LFO w/ moving phasor, output scope/meter.
- [ ] Drag-to-assign modulation (click source → click a knob's mod dot → adds a matrix row; draw mod-range arc on the ring).
- [ ] Resizable window.

## M8 — Polish & ship
- [ ] Oversampling on oscillators/FX where needed; denormal handling.
- [ ] Double-click-to-default, fine-drag (shift), value tooltips.
- [ ] pluginval strictness 10, CPU profiling, preset audition pass.

> Tip: keep the prototype (`ZandersWave_prototype.html`) open side-by-side while building M7 — it's the visual contract.
