# Hic

A restrained glitch drum machine. Small, damped, slightly broken percussion:
clicks and pops where the hats would be, blanket-muffled kicks, rimshots and
pencil taps instead of snares, crackle and hiss beds that breathe with the
kick, and a stutter only every few bars.

Reference points: Lali Puna, The Notwist (*Neon Golden*), Ms. John Soda,
Múm (*Finally We Are No One*), Arovane, Isan, Dntel, early Four Tet.

## What it does

- **One voice on every pad: exciter, resonator, shaper.** Six macros with
  the same meaning everywhere. *Tune* (30 Hz to 3 kHz), *Decay* (5 ms to
  2 s), *Exciter* (a single click, a soft mallet, or a noise burst),
  *Body* (a tuned membrane with a pitch drop, through woody harmonic modes,
  to inharmonic metal and hat territory), *Break* (a wavefolder and
  saturation after the resonator, and above half way a feedback path that
  keeps tails alive: boings, lasers, bending decays), and *Drift* (every
  hit re-rolls the other five a little, deterministically from its seed).
- **Six bus knobs that glue the kit** the way a small modular rack would:
  *Drive* (a gentle fold and saturation), *Damp* (the blanket lowpass and
  tilt), *Texture* (grit inside every hit plus a clocked static voice),
  *Space* (a short spring or room), *Drift* (a multiplier on every pad's
  drift) and *Feel* (timing and velocity scatter). The detailed controls
  behind them sit under a Detail button.
- **A global Key.** Root and scale; pads that follow the key snap their
  Tune to the nearest scale degree at trigger time, so kick, snare and
  tuned percussion sit in the song. Drift on a keyed pad only moves it to
  a neighbouring degree.
- **Everything rhythmic.** There is no free-running noise bed. Grit lives
  inside each hit, and the static voice only exists in pulses on a clock
  division or on flagged steps, reseeded per bar so a bar of static is the
  same bar every time.
- **Three factory kits on the General MIDI map**, so any existing drum
  pattern plays: *Neon* (damped, woody, clicks), *Micro* (metal, folded,
  drifting) and *Modular* (laser and boing tails, everything keyed).
- **Restrained glitch.** A beat repeat that fires at a low, seeded
  probability with a minimum number of bars between stutters, and a
  granular freeze that grabs the last click and turns it into ticking
  insects (MIDI note 90 holds it, 91 forces a stutter).
- **A step sequencer** with polymetric track lengths, per-step velocity,
  nudge, probability, ratchet, accent, reverse (the hit swells into its
  step) and static-pulse flags, synced to the host or an internal clock.
- **Sample export.** *Export kit* bounces every pad as one-shots at several
  seeds and velocities; *Bounce loop* renders the active pattern.

Plugin state saved by the 0.1 version is not loaded by 0.2 (the pad model
changed); patterns are stored separately and still load.

## Shape of the project

- `dsp/` is a dependency-free C++17 synthesis core: no JUCE types, no heap
  use in the audio path, no exceptions or RTTI, fixed buffers, float math.
  Every sound is synthesized (modal resonators, seeded impulses, filtered
  noise); there is no sample library. This is the part that ports to a
  Daisy Seed class microcontroller later.
- `plugin/` is a thin JUCE wrapper (VST3, AU, Standalone).
- `tests/` is a hand-rolled harness with a WAV renderer, signal analysis,
  a heap-allocation guard, and a cycles-per-sample benchmark.

## Building the core and tests

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
build/tests/render_kit out/    # renders every pad and the demo beats to WAV
```

Or simply `scripts/verify.sh`. The render tool writes both kits pad by pad,
a GM demo beat through each, the bed on its own, a freeze demo and the
internal sequencer's demo pattern.

Hardware is not built yet, but the core is written to port to a Daisy Seed
class microcontroller; see `docs/PORTING_DAISY.md`.

## Building the plugin

Requires JUCE (fetched automatically via git, or drop a checkout into
`external/JUCE`). On Linux install the usual JUCE dependencies first:

```sh
sudo apt-get install libasound2-dev libxrandr-dev libxinerama-dev libxcursor-dev \
    libgl1-mesa-dev libfreetype-dev libx11-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DHIC_BUILD_PLUGIN=ON
cmake --build build
```

## License

The DSP core and tests are MIT. The plugin wrapper links JUCE, which is
GPLv3 or commercially licensed; see `LICENSE`.
