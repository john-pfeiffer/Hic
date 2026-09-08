# Hic

A restrained glitch drum machine. Small, damped, slightly broken percussion:
clicks and pops where the hats would be, blanket-muffled kicks, rimshots and
pencil taps instead of snares, crackle and hiss beds that breathe with the
kick, and a stutter only every few bars.

Reference points: Lali Puna, The Notwist (*Neon Golden*), Ms. John Soda,
Múm (*Finally We Are No One*), Arovane, Isan, Dntel, early Four Tet.

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

Or simply `scripts/verify.sh`.

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
