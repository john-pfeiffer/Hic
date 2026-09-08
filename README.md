# Hic

A restrained glitch drum machine. Small, damped, slightly broken percussion:
clicks and pops where the hats would be, blanket-muffled kicks, rimshots and
pencil taps instead of snares, crackle and hiss beds that breathe with the
kick, and a stutter only every few bars.

Reference points: Lali Puna, The Notwist (*Neon Golden*), Ms. John Soda,
Múm (*Finally We Are No One*), Arovane, Isan, Dntel, early Four Tet.

## What it does

- **Twelve pads, five synthesis engines.** Kick (filtered sine with a snap
  or blanket attack), Click (impulses, dipoles, reduced-bit bursts,
  CD-skip trains), Modal (a struck-object model with cross-stick, rimshot,
  woodblock, pencil tap, muted guitar, glockenspiel, toy piano, bell and
  bowl presets), Noise (hats, brushes, shakers, paper) and Ping (FM,
  ring-modulated and pitch-bent synthetic micro-percussion).
- **Two factory kits on the General MIDI map**, so any existing drum
  pattern plays: *Neon* (the Notwist / Lali Puna palette) and *Micro*
  (morphing synthetic micro-percussion in the spirit of modular and FM
  percussion sample packs).
- **Feel.** Every hit can be nudged a few milliseconds and scattered in
  time and velocity, deterministically from a seed, so a loop sounds the
  same each time until you change the seed. A per-pad *morph* control
  drifts the macros a little on every hit.
- **A bed** of vinyl crackle and tape hiss that can run free, gate to the
  clock or to step flags, and duck under the kick.
- **Restrained glitch.** A beat repeat that fires at a low, seeded
  probability with a minimum number of bars between stutters; a granular
  freeze that grabs the last click and turns it into ticking insects
  (MIDI note 90 holds it, 91 forces a stutter); a short spring or room
  reverb with no hall setting.
- **A step sequencer** with polymetric track lengths, per-step velocity,
  nudge, probability, ratchet, accent, reverse (the hit swells into its
  step) and bed-gate flags, synced to the host or an internal clock.
- **Sample export.** *Export kit* bounces every pad as a set of one-shots
  at several seeds and velocities; *Bounce loop* renders the active
  pattern. Build your own packs from the instrument.

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
