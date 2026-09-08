# Sound design notes

The numbers that shape the instrument, kept next to the code they describe
(`dsp/include/hic/UnifiedVoice.h`, `dsp/src/Kit.cpp`).

## The voice: exciter, resonator, shaper

Every pad is the same signal path. Six macros, 0..1, resolved once per hit
(nothing transcendental runs per sample):

| Macro | What it does |
| --- | --- |
| Tune | Fundamental, 30 Hz .. 3 kHz, exponential. Follows the MIDI note on pads flagged to, and snaps to the global key on pads flagged to. |
| Decay | Overall length, 5 ms .. 2 s. The membrane decays in this time; each mode decays in a fraction of it (harmonic sets die faster up the series, metal sets ring longer). |
| Exciter | Crossfade from an impulse cluster (one to four seeded clicks, the CD-skip lives here), through a half-sine mallet whose width follows velocity and whose energy is normalised by area, to a bandpassed noise burst whose centre and length rise with the knob and darken for low tunes. |
| Body | Equal-power crossfade between a membrane (sine with a pitch drop, gaining odd harmonics toward the modal region) and a bank of six bandpass resonators whose ratios interpolate geometrically from a harmonic set {1, 2, 3, 4, 5, 6} to a metal set {1, 1.48, 2.41, 3.62, 5.31, 7.48}. Above 0.85 the modes detune slightly per hit and flatten in gain: hat and shaker territory. |
| Break | Triangle wavefold, then soft saturation, mixed in over the first quarter of the range. Above 0.5 a feedback path takes the shaper output, highpasses and lowpasses it, and injects it into the resonator and into the membrane's phase. Four independent bounds keep it musical: the loop signal is tanh-bounded, its envelope decays with the hit, the membrane integrates with a cap, and every 32 samples any mode that has grown is scaled down. |
| Drift | Per-hit re-roll of the other five from a seeded hash: Tune ±0.08, Decay ±0.15, Exciter ±0.20, Body ±0.12, Break ±0.15 at full drift, scaled by the bus Drift knob. Applied before key quantisation. |

Grit is part of the hit: seeded dipole dust at a density and colour that
follow the Exciter knob, bit-reduced, shaped by an envelope no longer than
150 ms, added before the shaper so Break folds it with the body. The amount
is the bus Texture knob times a floor of a quarter plus the Exciter value.

Why a bandpass resonator: the plain two-pole has gain at low frequencies,
so a slow mallet bump thumps through every mode. With zeros at DC and
Nyquist a unit impulse gives a decaying cosine of amplitude one at any
frequency, and a pulse of area A behaves like A impulses. That is why the
mallet is normalised by its area, and why a mallet longer than half a
period is shortened: a soft mallet cannot excite a high metal body, which
is also how real objects behave.

## The bus

Macro scales, detail shapes. Each bus knob drives a detailed struct that
stays reachable under the Detail button.

| Knob | Drives |
| --- | --- |
| Drive | `BusShaper`: fold gain 1 + 0.8 d, saturation 1 + 1.2 d, mixed in over the first half of the range, trimmed so it never gets louder. |
| Damp | `Damp`: a lowpass from 20 kHz down to 700 Hz plus a 1 kHz tilt that lifts the lows and lowers the highs as it closes. |
| Texture | The grit amount in every hit, and the static voice's level. Zero silences both exactly. |
| Space | The reverb return, and a size scale from half to one-and-a-half times the detail decay. |
| Drift | A multiplier, 0 .. 2, on every pad's Drift macro. |
| Feel | A multiplier on the feel detail's scatter maxima (10 ms and 0.25 of velocity by default). |

## Kits

Starting values, tuned by measurement (peak, decay window, spectral tilt,
estimated fundamental) and meant to be adjusted by ear. Tune in Hz, Decay
in ms, the other four are 0..1.

Neon (bus Drive .10, Damp .35, Texture .35, Space .30, Drift 1.0, Feel .5): see `makeDefaultKit` in `dsp/src/Kit.cpp`.
Micro (bus .25, .10, .45, .40, 1.3, .6): `makeMicroKit`.
Modular (bus .35, .15, .40, .50, 1.6, .4): `makeModularKit`.

Points in the macro space worth knowing:

- CD-skip hat: Exciter 0.1 .. 0.25, Body 0.8, Decay 12 ms, Drift 0.25.
- Blanket kick: Tune 40 .. 55 Hz, Exciter 0.4, Body 0.05, bus Damp up.
- Brush: Exciter 0.7 .. 0.8, Body 0.85, Decay 30 ms.
- Shaker: Exciter 1, Body 0.97, Texture up, Drift 0.35.
- Feedback thud: Break 0.6 .. 0.75, Body 0.3.
- Ring-modulated snare: Body 0.75, Break 0.7.
- Boing: a low tune, Break 0.8 .. 0.95, Decay 100 ms and up.
