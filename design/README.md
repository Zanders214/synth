# Handoff: ZandersWave — Wavetable Synthesizer (VST3 / AU / AAX)

## Overview
**ZandersWave** is a Serum 2–class wavetable software synthesizer. This package is the **UI and UX specification** for building it as an audio plugin. The goal is a fully playable instrument plugin with: two wavetable oscillators + sub + noise, a multimode filter, three envelopes, four LFOs, a drag-to-assign modulation matrix, a ten-slot effects rack, an arpeggiator, preset management, and polyphonic/MPE voice handling.

It is styled in the **Neon Plugins / "Zanders"** visual language: dark glass panel, a four-stop spectrum (cyan→violet→pink→amber) used for all meters/rings/curves, a single blue accent, and color-matched glows.

## About the Design Files
The files in this bundle are **design references created in HTML/JS** — an interactive prototype showing the intended look, layout, and behavior. **They are not production code to ship.** A VST3 cannot be built from HTML.

Your task is to **recreate this design as a native audio plugin** in the appropriate plugin environment, implementing a real DSP audio engine behind the UI:

- **Recommended framework: [JUCE](https://juce.com) (C++).** It is the industry standard for VST3/AU/AAX, handles the plugin formats, parameter automation, preset (state) save/load, and MIDI, and lets you draw this exact UI with a custom `LookAndFeel` + `Component`s.
- Alternatives: **iPlug2** (C++), or **nih-plug** (Rust) if you prefer. Any of these can host the format; the choice doesn't change the spec below.
- The HTML prototype is the **visual + interaction contract**. The audio DSP (wavetable oscillators, filter, envelopes, LFOs, FX, voice allocation) must be written natively — the prototype's WebAudio is only a rough sketch so the keyboard makes sound; do not treat it as reference DSP.

### How to view the prototype
Open **`ZandersWave_prototype.html`** in any browser — it is fully self-contained (no server, no internet needed). Drag knobs/sliders, switch oscillator tabs, click the lower tabs (FX / Matrix / Arp / Wavetable), and play the on-screen keyboard. The source is in `source/ZandersWave.dc.html`.

## Fidelity
**High-fidelity.** Colors, typography, spacing, glows, and interactions are final. Recreate the UI to match — exact hex values, fonts, and the spectrum/glow treatment are listed in **Design Tokens** below. Where the prototype is a simplified stand-in for deeper functionality (e.g., wavetable warp *modes*, full FX parameter sets), that is called out so you know to expand it.

---

## Plugin Architecture (signal flow)

```
                 ┌── OSC A (wavetable, unison) ──┐
   MIDI note ──► ├── OSC B (wavetable, unison) ──┤
   + velocity   ├── SUB  (sine/tri/square) ──────┤──► FILTER (multimode) ──► AMP (ENV1) ──┐
                 └── NOISE (white/pink/vinyl) ────┘     ▲                                   │
                                                        │ routing toggles A/B/Sub/Noise     ▼
   Modulators:  ENV1(amp) ENV2 ENV3 · LFO1–4 · MACRO1–4 · VEL · NOTE                    FX RACK (10 slots, serial)
        │                                                                                   │
        └──────────────── MOD MATRIX (source → destination × amount) ────────────►         ▼
                                                                                        MASTER OUT
   ARP (optional) re-triggers notes into the voice engine.
```

- **Per-voice** (polyphonic): OSC A, OSC B, SUB, NOISE → mix → FILTER → AMP envelope. Allocate a voice per held note; target 16+ voices, MPE-aware.
- **Global** (post-voice mix): FX rack → master gain.
- **Modulation** is computed per-voice (envelopes, velocity, note) and global (LFOs can be poly or global, macros are global) and summed per destination via the matrix.

---

## Screens / Views

This is a **single resizable window** (design size **1320 × ~900 px**, dark panel on near-black). One panel, no separate screens. Regions, in reading order:

### 1. Header bar (full width, ~52px tall)
- **Left:** wordmark `Zanders` + `Wave` (the product word in spectrum-cyan), then a blue pill **Badge** reading `WAVETABLE`.
- **Center:** preset browser — `‹` / `›` arrows flank a recessed well showing a glowing dot + preset name (Title Case) + `NN / NN` index in mono; then **SAVE** and **BROWSE** buttons.
- **Right:** voice count (`N VOICES` mono) + `MPE · 16`, and a small spectrum output meter with a `-∞ -12 -6 0` dB scale.

### 2. Left rail (304px wide) — modulation
- **ENVELOPE module:** tab row `ENV1 · AMP` / `ENV2` / `ENV3`; an editable **ADSR curve** in a dark well (drawn cyan with glow, draggable node handles); below, four arc dials **ATTACK / DECAY / SUSTAIN / RELEASE**. ENV1 is hardwired to amplitude.
- **LFO module:** tab row `LFO1–4`; an animated **LFO shape display** (pink curve, a moving white phasor dot riding it); a shape selector `Sine / Triangle / Saw / Ramp / Square / Random`; three dials **RATE / DEPTH / RISE**. Rate shows tempo-sync divisions (`1/1 … 1/32`).

### 3. Center — the hero (flexible width)
- **Oscillator editor (hero):** a segmented tab control **OSC A / OSC B / SUB / NOISE** (each with a colored on/off status dot). To the right: an **OCT −/+** stepper and a master on/off toggle for the selected source. A caption line shows the wavetable name + `FRAME nnn / 256`.
  - The big **wavetable display** is the centerpiece: a **3D stack of angled waveform frames** (Serum-style perspective), colored along the spectrum by depth, with the current frame highlighted in white + glow. It animates subtly and reshapes as WT POS / WARP change.
  - For **SUB**: a clean sine + sub-octave harmonic display. For **NOISE**: an animated spectral field.
  - **Controls** below adapt to the source:
    - OSC A/B: dials **WT POS / WARP / UNISON / DETUNE**, then **LEVEL** and **PAN** sliders.
    - SUB: wave pills `Sine / Tri / Square`, **SATURATE** dial, **LEVEL** slider.
    - NOISE: type pills `White / Pink / Vinyl`, **COLOR** dial, **LEVEL** slider.
- **Source mixer (always visible, under the hero):** four channel cards **OSC A / OSC B / SUB / NOISE**, each with name, colored on/off dot, and a **LEVEL** slider. Clicking a card selects that source into the hero editor. This keeps the whole voice visible while you edit one source.

### 4. Right rail (344px wide) — filter & output
- **FILTER module:** on/off toggle; filter-type pills `LP24 / LP12 / HP24 / HP12 / BP12 / Notch`; a live **filter response curve** (blue, glowing, with a resonance bump and a cutoff marker line) in a well; dials **CUTOFF / RESO / DRIVE / MIX**; and routing buttons **OSC A / OSC B / SUB / NOISE** (which sources feed the filter).
- **OUTPUT module:** an animated output **oscilloscope**, a spectrum output **meter** with a dB scale, and a **MASTER** dial (−24…0 dB).

### 5. Mod-sources bar (full width)
A row labeled `MOD SOURCES` with selectable chips: `ENV1 ENV2 ENV3 · LFO1 LFO2 · VEL NOTE`. Selecting one enters **assign mode** (cursor → crosshair); see Interactions.

### 6. Lower workspace (full width, tabbed)
Tab strip: **FX RACK / MOD MATRIX / ARPEGGIATOR / WAVETABLE**.
- **FX RACK:** a horizontal chain of 10 slots `Hyper, Distort, Flanger, Phaser, Chorus, Delay, Reverb, Comp, EQ, Filter` — each a card with an on/off dot + checkbox; selecting a slot reveals its parameter dials below (4 per effect) + an ENGAGE/BYPASS toggle.
- **MOD MATRIX:** editable table — rows of `[dot] SOURCE ▸ select │ DESTINATION ▸ select │ AMOUNT bipolar slider │ ±value │ ✕`. An `+ ADD ROUTE` button. Knob assignments (see Interactions) create rows here; this is the single source of truth for modulation routing.
- **ARPEGGIATOR:** a 16-step on/off grid (bars), a RUN/STOP toggle, RATE pills, MODE select (`Up / Down / Up/Dn / Random / Chord / As Played`), and OCT range 1–4.
- **WAVETABLE:** a larger frame view + a `FRAME POSITION` slider (0–256), a harmonic-spectrum bar display, and `MORPH / SPECTRAL / IMPORT WAV` actions (wavetable editor — see Notes).

### 7. Footer (full width)
**MACROS** block (4 knobs `MACRO 1–4`, each with an `ASSIGN` button) sits left of two **PITCH / MOD wheels**, beside a full **playable keyboard** (~2 octaves visible, white/black keys depress with a glow on press).

---

## Interactions & Behavior

- **All knobs/dials:** vertical drag to change value (ns-resize). **Sliders:** horizontal drag. Double-click should reset to default (add this in native; prototype omits it).
- **Drag-to-assign modulation (key feature):** click a chip in **MOD SOURCES** (or a macro's **ASSIGN**) → assign mode. Then click the small **mod dot** at the top-right corner of any knob to create a routing from that source to that parameter. The routing appears as a row in **MOD MATRIX** with a default amount; the knob's dot lights in the source's color. A knob targeted by multiple sources shows a count badge. In native, also draw a colored modulation-range arc around the ring.
- **Oscillator tabs / source-mixer cards:** switch which source the hero editor shows. On/off dots mute/enable each source.
- **Filter routing buttons:** toggle which sources pass through the filter.
- **Lower tabs:** switch the workspace module. **FX slots:** click to select (show params); the corner checkbox toggles bypass.
- **Preset browser:** `‹`/`›` step presets and ease parameters toward the loaded values (don't snap — ~8/s lerp). **SAVE** captures the current full parameter state as a new preset.
- **Keyboard / wheels:** press = note on (voice allocate), release = note off (enter release stage). Pitch wheel = ± bend (default ±2 semitones); mod wheel = a mod source.
- **Arp:** when RUN is engaged, held notes are sequenced through the step grid at RATE in the selected MODE/OCT.

### Motion (match these)
Short and mechanical, no bounce. State changes fade `0.15s`; fills swap `0.08s`; key/knob press depresses `translateY(2px)` over `0.04s`. Continuous motion (wavetable shimmer, LFO phasor, scope) is animation-frame driven, not looping CSS. Easing `cubic-bezier(0.4, 0, 0.2, 1)`.

---

## Full Parameter Spec

Implement each as an automatable plugin parameter (JUCE `AudioProcessorValueTreeState`). Ranges/defaults are taken from the design; units in parentheses.

### Oscillator A & B (wavetable) — duplicate for both
| Param | Range | Default A / B | Notes |
|---|---|---|---|
| Enable | on/off | on / on | mute source |
| WT Position | 0–100% (frame 0–255) | 30% / 62% | scrubs through wavetable frames |
| Warp | 0–100% | 42% / 24% | wavetable warp amount (add warp **modes** — see Notes) |
| Unison | 1–16 voices | 4 / 1 | prototype caps display at ~7; allow to 16 |
| Detune | 0–100 cents | 18c / 30c | unison spread |
| Level | 0–100% | 82% / 0% | |
| Pan | −100…+100 | 0 / 0 | |
| Phase / Retrig | 0–360° | 0 | oscillator start phase |
| Octave | −3…+3 | 0 / 0 | |
| (add) Coarse/Fine, Phase-rand, Unison blend/width per Serum parity |

### Sub oscillator
| Param | Range | Default |
|---|---|---|
| Enable | on/off | on |
| Wave | Sine / Tri / Square | Sine |
| Octave | −2…+2 | −1 |
| Saturate | 0–100% | 22% |
| Level | 0–100% | 46% |

### Noise oscillator
| Param | Range | Default |
|---|---|---|
| Enable | on/off | off |
| Type | White / Pink / Vinyl | White |
| Color | 0–100% | 40% |
| Level | 0–100% | 0% |

### Filter
| Param | Range | Default |
|---|---|---|
| Enable | on/off | on |
| Type | LP24 / LP12 / HP24 / HP12 / BP12 / Notch | LP24 |
| Cutoff | 20 Hz – 20 kHz (log) | ~2.2 kHz (0.68 norm) |
| Resonance | 0–100% | 26% |
| Drive | 0–100% | 16% |
| Mix | 0–100% | 100% |
| Routing | per source: OSC A / OSC B / SUB / NOISE | A on, B on |

### Envelopes ×3 (ENV1 = Amp, hardwired to VCA)
| Param | Range | ENV1 def | ENV2 def | ENV3 def |
|---|---|---|---|---|
| Attack | 0–~8 s (exp) | ~10 ms | ~0.2 | 0 |
| Decay | 0–~8 s | ~0.34 | 0.50 | 0.22 |
| Sustain | 0–100% | 66% | 30% | 100% |
| Release | 0–~8 s | ~0.40 | 0.52 | 0.30 |

### LFOs ×4
| Param | Range | Default (LFO1) |
|---|---|---|
| Shape | Sine / Triangle / Saw / Ramp / Square / Random | Sine |
| Rate | sync `1/1…1/32` or 0.01–40 Hz free | ~1/8 |
| Depth | 0–100% | 62% |
| Rise (fade-in) | 0–~4 s | 0 |
| (add) Phase, Mode (trigger/env/free), Rate sync toggle |

### Macros ×4
`MACRO 1–4`, 0–100%, defaults 50 / 20 / 0 / 74%. Each is a global mod source assignable via the matrix.

### Master
`Master Out`, −24…0 dB, default ~−5 dB (0.80 norm).

### Modulation Matrix
List of routes `{ source, destination, amount (−1…+1) }`. Sources: `ENV1–3, LFO1–4, MACRO1–4, VEL, NOTE` (extend with MPE, Random, Mod Wheel, Aftertouch). Destinations: any continuous parameter above. Prototype defaults: ENV1→OSC A Level (+1.00), LFO1→Cutoff (+0.42), ENV2→OSC A WT (+0.26), MACRO1→OSC B WT (−0.50).

### FX Rack (10 serial slots, each bypassable)
Suggested params (prototype shows 4 dials each — expand to full ranges):
- **Hyper** (unison/dimension): Detune, Voices, Width, Mix
- **Distort:** Drive, Tone, Mix, Out (+ mode: tube/diode/fold/etc.)
- **Flanger:** Rate, Depth, Feedback, Mix
- **Phaser:** Rate, Depth, Stages, Mix
- **Chorus:** Rate, Depth, Voices, Mix
- **Delay:** Time (sync), Feedback, Width, Mix (+ ping-pong, filtering)
- **Reverb:** Size, Decay, Damp, Mix (+ pre-delay)
- **Comp:** Threshold, Ratio, Attack, Makeup (+ release)
- **EQ:** Low, LoMid, HiMid, High (turn into a full parametric EQ)
- **Filter:** Cutoff, Reso, Type, Mix

### Arpeggiator
Enable; 16-step on/off pattern; Rate `1/4 1/8 1/8T 1/16 1/16T 1/32`; Mode `Up / Down / Up-Dn / Random / Chord / As Played`; Octave range 1–4. (Add gate, swing, per-step velocity for parity.)

### Global / voicing (add — not in prototype, needed for parity)
Polyphony count, Mono/Legato, Glide/Portamento time, Pitch-bend range, MPE on/off.

---

## State Management
- One flat parameter tree (JUCE `AudioProcessorValueTreeState`) holding every parameter above — gives you automation + DAW save/load for free.
- **Presets** = serialized snapshots of the whole tree (name + values). Provide factory presets and user save/load. The prototype persists user presets to browser storage; in the plugin, save to disk (`.vstpreset` or your own format) and expose a browser.
- **Modulation matrix** is its own serialized list (source/dest/amount). Per block, compute each modulator, sum contributions per destination, apply on top of the base parameter value (clamp to range).
- Wavetable data: ship a library of wavetables (each = N single-cycle frames, typically 256 frames × 2048 samples). WT Position interpolates between frames; Warp transforms them.

---

## Design Tokens (exact values)

### Color — spectrum (the brand; used for all meters/rings/curves, 0→100% = left→right)
- Cyan `#34d8ff` · Violet `#8b7bff` · Pink `#ff5fa8` · Amber `#ffc24b`
- Ramp = linear gradient cyan→violet→pink→amber. Cool sub-ramp = cyan→violet. Warm sub-ramp = pink→amber.

### Color — accent / signal
- Accent blue `#5e93ff`; accent gradient `linear-gradient(180deg,#5e93ff,#8b7bff)`; accent soft fill `rgba(94,147,255,0.12)`; accent line `rgba(94,147,255,0.30)`; accent glow `rgba(94,147,255,0.45)`.
- Danger (destructive/engaged) `#ff5a5a → #e23b3b`.

### Color — surface & text
- Panel: `radial-gradient(120% 80% at 50% -10%, #1a2030, #0a0b12)`. Page behind: `#06070c`. Display wells: `#070a0e` with inset shadow. Knob face: `radial-gradient(circle at 40% 30%, #222731, #0b0d12)`.
- White-alpha layers (structure on dark): 5% / 6% / 8% / 10% / 12%. Borders are white-alpha hairlines `rgba(255,255,255,0.06–0.12)`; the only colored border is the accent.
- Text: primary `#e8ecf3`, secondary `#cfd4dc`, tertiary `#9aa3b3`, muted `#8a93a3`, label `#7e8794`, faint/ticks `#5d6473`.

### Typography
- Display / UI / numerals: **Space Grotesk** (600 workhorse, 700 on engaged buttons). All-caps labels track wide `0.14–0.22em`; big numerals track tight `-0.02em`.
- Every measured value/unit: **JetBrains Mono** (e.g., `2.19 kHz`, `-6.0 dB`, `74%`, `18c`, `FRAME 077 / 256`).
- Sizes: hero knob readout ~38px; section labels 10–11px; mono captions 8.5–10px. (Both fonts are Google Fonts; bundle them in the plugin.)

### Radii, glow, elevation
- Radii: panel 16px, windows/wells 12px, buttons 10–12px, chips/controls 8px, badges 20px pill, knobs/dots full circle.
- **Glow** is the signature: elements glow in **their own color** — dots `0 0 7px <color>`, engaged buttons `0 0 22px <color-alpha>`, rings/arcs use `drop-shadow(0 0 7px …)`. The hero knob's white indicator glows white.
- Elevation is mostly **inset** (wells, knob faces). One outer drop on the whole panel: `0 18px 50px rgba(0,0,0,0.40)`. No mid-level card shadows on dark.

### Iconography
Near-zero by design — **no icon library, no emoji.** Status is shown with **color** (glowing dots, filled rings, curves). Only a couple of Unicode glyphs (`‹ › − + × ▶ ■`) and the spectrum sweep ring as the brand motif. Keep it that way.

---

## Assets
None to import — every control, meter, and the wordmark are generated from CSS/SVG and the tokens above (the wordmark is type, not a logo file). You will need to **author/ship a wavetable library** (the prototype's frames are procedurally drawn placeholders) and **bundle the two Google fonts**.

## Files in this bundle
- `ZandersWave_prototype.html` — self-contained interactive prototype. Open in a browser. **This is the visual + behavior spec.**
- `source/ZandersWave.dc.html` — the prototype source (HTML template + a JS logic class). Read it to see exact layout math, control composition, the wavetable/filter/envelope/LFO curve drawing, and default parameter values.
- `README.md` — this document.

## Notes & gaps to close for full Serum 2 parity
The prototype intentionally simplifies a few deep features — treat these as "expand in native":
- **Wavetable engine:** real frame interpolation + a set of **warp modes** (sync, bend, PWM, asym, remap, quantize, etc.); the prototype's Warp is a single continuous knob. Include a wavetable **editor/importer** (the WAVETABLE tab stubs `MORPH / SPECTRAL / IMPORT WAV`).
- **Oscillator extras:** per-osc coarse/fine tune, unison blend/width/phase-rand, sub-osc sync.
- **Filter:** more types (comb, formant, phaser-filter) for parity; per-source pre-filter mix.
- **FX:** the 4 dials per slot are representative — implement each effect's full parameter set.
- **Voicing/MPE/global** section (polyphony, mono/legato, glide, bend range) is listed in the spec but not drawn — add a small GLOBAL panel or settings menu.
- **Oversampling, parameter smoothing, and denormals** — standard DSP hygiene, not shown in UI.
