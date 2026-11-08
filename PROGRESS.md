# RAYGUN by TRILL TRON - PROGRESS

## Status
| Milestone | State |
|---|---|
| Engine (2 wavetable osc + sub + noise, unison, FM, warp, SVF, 2 ADSR, 2 LFO, 8-slot mod matrix, 8 macros, arp, chorus/delay/reverb/charge) | done, unit-tested |
| Two-layer system (A / B / A+B / Split w/ crossfade / Velocity split, per-layer key range + MIDI ch) | done, unit-tested |
| Stem wavetables (5 TRILL tables cut from the stems) | done (`tools/make_wavetables.py`) |
| Factory presets | **595** generated, render-checked, loudness-normalised (`tools/gen_presets.py`) |
| GUI (ray gun main page, 8 tabs, preset arsenal browser, 75-200% vector scaling) | done, screenshot-verified headless |
| Keyboard-focus safety | done by construction (no widget accepts focus; only the search box does and returns it) |
| Linux VST3 + headless plugin harness | builds + passes |
| Windows VST3 | MinGW cross-build in this container (see Known gaps); MSVC build via GitHub Actions (`.github/workflows/build.yml`) |
| macOS / CLAP | CI job present for macOS; CLAP not done |

## Measured (Release, 44.1 kHz, 512 block, one core)
| scenario | CPU |
|---|---|
| 6 voices, 1 layer, Clean | 1.0 % |
| 8 voices, uni 3, FX, 1 layer | 2.9 % |
| both layers, 8 voices each, uni 3, FX | 5.8 % (4.8 % Eco) |
| worst case: both layers, 16 voices, uni 7, FX, Hot | 19.8 % (9.7 % Eco) |
Aliasing at A6/C7/E7 saw: -56 dB or better. 120-patch random-parameter fuzz: no NaN/inf, bounded output.

## Decisions (spec docs 01-08 were not supplied, so these are mine)
* Macros are bipolar around 0.5 (0.5 = neutral). Fixed mapping: Bright=cutoff, Chorus, Attack, Release, Charge=saturation+drive, Motion=table pos+LFO rate, Space=reverb+delay, Drift=detune+osc2 fine.
* Modulation is control-rate (every 16 samples) - cheaper than audio-rate; filter coefficients update per chunk.
* Wavetables: 1024-sample frames, 32 frames/table, 9 band-limited mip levels built at load via FFT.
* Quality: Eco = unison<=3, LP24->LP12, chorus off, lighter reverb. Hot = extra output drive.
* Presets are `.rgp` JSON, only non-default params stored. Factory set is embedded in the binary (no install-path dependence).
* Characters: BLASTER-8, MONO-VOLT, FIVE-STAR, CHROME-FM, NEON-ROM, PULP-TUBE, XENO (original names).

## Bugs found by testing
* ASan caught a heap overflow in the delay/chorus interpolated read (float rounding made index == size). Fixed.

## Known gaps / honest notes
* **ARP/SEQ**: arpeggiator only; no step sequencer yet.
* **Not yet tested inside FL Studio itself** - needs a Windows machine. Host-side focus behaviour (typing keyboard) is built to spec but unverified in FL.
* VST3 only (no VST2 - licensing). FL Studio needs 20.7+ for VST3.
* No CLAP yet; macOS only via CI.
* Wavetable "stem DNA" tables are cut from the 5 supplied wavs only; `reference/` is git-ignored (large).
