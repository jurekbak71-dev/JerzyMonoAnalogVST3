# Jerzy Mono Analog Grid — VST3

Samodzielny monofoniczny syntezator analog-modeling VST3/Standalone oparty na JUCE.

## Funkcje
- 2 × band-limited VCO (PolyBLEP): sine / triangle / saw / square
- suboscylator, noise i analogowy drift
- nieliniowy mixer
- 24 dB/oct nonlinear ladder-style VCF z rezonansem, drive, key tracking i bipolar filter envelope
- dwa analogowo zakrzywione ADSR
- LFO z synchronizacją do tempa
- glide, legato, retrigger i wybór priorytetu nut
- arpeggiator
- przełącznik GRID → ARP: kroki sekwencera i pady Launch wyzwalają arpeggiator, gdy ARP jest włączony; bramki i ratchety GRID zwalniają i ponawiają nuty
- siatka 8×8 RGB jako sekwencer / launch-pad
- root note, wybór skali i długość sekwencji do 8 banków / 64 kroków
- uruchamianie sekwencera przez MIDI
- parametry automatyzowalne przez host/DAW
- 4× wewnętrzny oversampling i filtracja antyaliasingowa

## 0.10.0 — GRID → ARP i wydajność

Włącz `ARP ON` oraz `GRID → ARP` na stronie GRID lub ARP. Sekwencer GRID
lub pady w trybie Launch przekazują nuty do arpeggiatora. Wyłączenie
`GRID → ARP` pozostawia zwykłe odtwarzanie GRID. Stare presety domyślnie
mają tę opcję wyłączoną. Przełącznik podlega automatyzacji w DAW.

Odczyty ustawień GRID są wykonywane raz na blok audio. Bufory MIDI są
ponownie używane między blokami, a zbędne kopiowanie MIDI zostało usunięte.
Efekty pomijają wyłączone moduły, a panel nie odświeża całego tła co 50 ms.
Algorytm syntezy, oversampling i parametry brzmieniowe pozostają bez zmian.

## Build Windows / FL Studio
Wymagane: Visual Studio 2022 z workloadem Desktop development with C++ oraz CMake.

```bat
build_windows.bat
```

lub:

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

Plugin: `Jerzy Mono Analog Grid.vst3`

Standardowy katalog Windows VST3:

`C:\Program Files\Common Files\VST3`

Po instalacji wykonaj ponowne skanowanie w FL Studio.

## CI
Workflow `.github/workflows/build-windows.yml` buduje Windows x64 VST3 na `main` i publikuje artefakt `Jerzy-Mono-Analog-Grid-Windows-VST3`.


## 0.3.0 — analog signal path and envelope repair

This is an original analog-inspired instrument, not a component-accurate emulation
of Moog, Bass Station II or Circuit Mono Station. It keeps the existing Grid/ARP
and host parameter IDs. Existing presets load, but intentionally sound different.

- Free-running PolyBLEP VCOs; triangle integrator starts at the correct phase level
  and uses a rate-independent DC correction. Sub pitch follows OSC1 including octave
  and drift, at half its instantaneous frequency (independent initial phase).
- Summed channel levels drive a bounded, monotonic asymmetric mixer saturator with
  DC removal. High input no longer causes polynomial foldback. Filter and output
  drive remain separate sound-shaping stages.
- Four-pole nonlinear ladder with safeguarded Newton feedback solve, rather than
  fixed-point relaxation that can fail at high cutoff/resonance.
- Main timbral controls have 3 ms smoothing; VCOs continue running across note gates.
- Amp and filter ADSR reach their stage endpoints at the displayed durations
  (rounded to an internal sample). Attack charges toward 120%; decay/release use
  a small target overshoot. Retrigger preserves the current capacitor level.
  Live time edits scale the remaining stage duration without resetting amplitude.
- GUI plots the same capacitor curves and actual A/D/S/R settings. Its time axis
  uses a 250 ms sustain hold for illustration; actual sustain lasts until note-off.
  Wider editable sliders show milliseconds/seconds and sustain percentage.
  Window resizing preserves the panel aspect ratio, including the ARP extension.
- Unknown note-offs no longer trigger envelopes; note-priority fallback correctly
  retriggers in non-legato mode. Held-note storage is reserved during preparation.

### Validation

CTest `MonoDSP` checks ADSR endpoints and retrigger at 44.1/48/96 kHz,
triangle DC and amplitude at 27.5/110/880 Hz, extreme ladder settings,
full voice output/release, and saturator monotonicity. Windows CI runs these
checks before packaging VST3. Run locally after configuring/building:

```sh
ctest --test-dir build -C Release --output-on-failure
```

Design references:
- [Minimoog Model D manual](https://api.moogmusic.com/sites/default/files/2018-01/Minimoog_Model_D_Manual.pdf)
- [Bass Station II signal path, mixer and envelope documentation](https://userguides.novationmusic.com/hc/en-gb/articles/25494313827986-Bass-Station-II-in-detail)
- [Circuit Mono Station](https://eu.novationmusic.com/products/circuit-mono-station)

Suggested next extension: a selectable multimode filter (12/24 dB LP, HP, BP)
plus independent modulation-envelope routing to pitch/PWM. The current release
concentrates on making the existing ladder, oscillator mixer and two ADSRs reliable.

## 0.4.0 — multimode filter and modulation envelope destinations

`FILTER MODE` offers the existing **Ladder 24 dB** plus **LP/HP/BP 12 dB**
and **LP/HP/BP 24 dB**. The six new responses use trapezoidal state-variable
sections with asymmetric pre-filter drive. The four-pole LP/HP responses use
Butterworth section damping at minimum resonance. A 24 dB BP uses two bandpass
sections: its slopes are 12 dB/oct on each side; a 12 dB BP has 6 dB/oct per side.
All states keep running and mode changes crossfade over approximately 3 ms
(time constant; settling takes several time constants).

The second ADSR is now labelled `MOD ENV`. `ENV > PITCH` independently applies
-24&+24 semitones to both VCOs (sub follows OSC1). `ENV > PWM` applies -100&+100%
modulation depth to both pulse widths. It sums with manual PW and LFO PWM and
is clamped to 5&95% duty cycle. PWM is audible on Square/Pulse waveforms.
The existing bipolar `ENV AMOUNT` continues routing this ADSR to filter cutoff.

All three new parameters are appended after existing parameter IDs/indices.
Old presets explicitly restore Ladder 24 dB with pitch/PWM depths at zero.
The Ladder 24 dB path is unchanged when the new destinations are zero.

## 0.5.0 — Grid sequencer and MIDI performance

- The Grid adds Forward, Reverse, Ping-Pong, and Random playback directions.
- Grid and arpeggiator add host-automatable Swing and Velocity controls.
- The arpeggiator's As Played mode now follows the order in which keys were pressed;
  Up, Down, and Up-Down retain pitch-sorted behavior.
- Grid and arpeggiator note events are emitted as MIDI, with sample-offset note-on
  and note-off events. MIDI input continues to trigger the synth and the Grid's
  MIDI-trigger mode. VST3 advertises MIDI output for routing in FL Studio.
- Host Sync follows FL Studio transport position (PPQ), tempo and play/stop state.
  Turn Host Sync off to use the internal clock. Pattern state and all new controls are saved with the
  project; old presets receive safe defaults.
- Design references: Circuit Mono Station User Guide (step direction/length,
  separate note and modulation sequencing concepts), Arturia KeyStep Pro and
  Novation Summit arpeggiator documentation (key order, octave range, rhythm,
  swing, gate and performance control).


## 0.6.0 — Output FX chain

A dedicated FX page adds six reorderable output effects: compressor/limiter with
soft-clipping drive, tempo-synced mono/stereo/ping-pong delay, stereo reverb,
mid/side stereo width, Juno-style chorus/chorus/flanger, and a stereo rotary
speaker with optional host-tempo sync. FX order is stored with the plugin state.
Every FX control is a host-automatable parameter; generated and incoming MIDI is
left intact. Effects are bypassed by default to preserve existing presets.


## 0.7.0 — MIDI-triggered Grid and responsive FX page

- MIDI Trigger mode now starts/restarts the internal Grid from each incoming FL
  Studio MIDI note. That note becomes the sequence root, so the whole pattern
  transposes with the played note; overlapping held notes use the most recent
  remaining note as the root.
- The Grid page adds an octave selector from -2 to +2 octaves. It is host
  automatable and restores a neutral octave in older project states.
- The FX page now lays out its six modules from the live editor bounds, keeping
  the chain controls and knobs inside their cards when the plugin is resized.


## 0.9.0 — readable console, host swing and envelope routes

- The Grid page devotes its lower half to the step matrix and large groove controls.
  The old instructional text panel is removed. Labels and controls are separated
  on every page, with larger value readouts and an editor size from 1200×600 to
  1920×960 (default 1320×660).
- Grid and Arp swing now work with FL Studio PPQ transport sync. Odd steps are
  delayed and paired steps keep their total duration; gate and ratchets use the
  correct step duration. The Grid Ping-Pong direction completes its return trip.
- The filter envelope can additionally modulate OSC 2 pitch, resonance, mixer
  drive and amplifier level. All four depths are bipolar, host-automatable,
  saved with the project, and default to zero in older presets.
- Values display percentages, milliseconds, semitones, dB or frequency where
  appropriate. The clock and modulation paths have regression coverage.
