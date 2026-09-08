# Porting Hic to a Daisy Seed class microcontroller

Nothing in this repository targets hardware yet. This note records the
constraints the core already respects and what a port would involve, so
that decisions made now do not close the door later.

## What the core already guarantees

- `dsp/` has no dependencies beyond the C++17 standard library headers
  `<cmath>`, `<cstdint>`, `<array>`. No JUCE types, no `std::vector`,
  no `std::string`, no iostreams.
- Compiled with `-fno-exceptions -fno-rtti`; no virtual dispatch in the
  audio path (voices switch on `PadType`).
- No heap allocation after construction. Every buffer is a fixed array
  sized from `HIC_MAX_SAMPLE_RATE` and `HIC_MAX_BLOCK`. The test harness
  overrides `operator new` and fails any test that allocates inside
  `Engine::process`.
- No per-sample transcendental calls. Coefficients are computed at
  trigger time or once per block; the sine is a polynomial.
- `float` everywhere; double promotion is a compile error in the core.
  The only `double` use is musical time (ppq) in the clock and sequencer,
  which the M7's FPU handles in software but only per block.
- Envelopes and resonators flush to zero, so denormals never accumulate
  even without hardware flush-to-zero.

## Budget

| | Value |
| --- | --- |
| Cortex-M7 at 480 MHz, 48 kHz | 10,000 cycles per stereo frame |
| Full engine, dense pattern, desktop estimate | about 1,000 cycles per frame at 3 GHz-equivalent |
| Target on the M7 | under 3,000 cycles per frame worst case |
| `sizeof(hic::Engine)` at 96 kHz | about 3 MB |
| `sizeof(hic::Engine)` at 48 kHz (`-DHIC_MAX_SAMPLE_RATE=48000`) | about 1.5 MB, fits the 64 MB SDRAM |

`tests/bench_cycles` prints the current numbers. Desktop cycles are only
a proxy; measure on the target with `DWT->CYCCNT` once a board exists.

## What a port would involve

1. Build `hic_dsp` with the arm-none-eabi toolchain, `-DHIC_MAX_SAMPLE_RATE=48000`,
   `-mfpu=fpv5-d16 -mfloat-abi=hard`. Place the `Engine` in SDRAM
   (`DSY_SDRAM_BSS`) since it is far larger than the 512 KB of internal
   SRAM.
2. Write a `main()` with libDaisy: configure the codec at 48 kHz with a
   block size of 32 or 48, own one `hic::Engine`, and call
   `engine.process(events, n, transport, outL, outR, block)` from the
   audio callback with `transport.valid = false` so the internal clock runs.
3. Map the panel: eight encoders or pots write `PadParams::macro[]` of the
   selected pad, buttons select pads and toggle steps, a MIDI input feeds
   `NoteEvent`s exactly as the plugin does.
4. Patterns and kits are plain structs; persist them to QSPI flash with a
   `memcpy` and the same `{magic, version, sizeof}` header the plugin uses.
5. Leave the reverse buffers, beat repeat ring and freeze ring at their
   48 kHz sizes; together they are under 1.5 MB.

## Things to avoid adding to the core

- Anything that allocates, throws, or uses RTTI.
- `double` in per-sample code.
- Dependencies on JUCE or the STL containers.
- Per-sample `std::exp`, `std::pow`, `std::sin`, `std::tan`. Use the
  helpers in `Math.h` or compute at trigger/block rate.
