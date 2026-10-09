# 0.11.0 — independent oscillator grids and expanded voices

- Mono (default), two-oscillator paraphony with shared filter/envelopes, and six-voice polyphony with per-voice filters/envelopes and bounded oldest-note stealing.
- Separate OSC 1 / OSC 2 grid memories, division (including triplets), exact 1–64-step lengths, direction and swing. OSC 1 length 0 retains the older bank-chain length. OSC 2 is silent in Mono mode.
- Per-note gate 1–16 steps. Click a pad to select it and toggle its note; right-click selects without toggling. Rests do not truncate held notes. A new note replaces the previous note on the same track. Ratchets retrigger and subdivide the selected gate. Separate tracks emit MIDI channels 1 and 2.
- NOTE GATE toggle: new instances use musical step gates; old presets retain legacy percentage gates until explicitly enabled.
- Random Square LFO alternates ±1 with independently random pulse durations. LFO > GATE applies signed modulation in steps, sampled at note trigger and bounded to 1–16. It uses a dedicated control-rate clock with the same waveform/rate/sync settings, not a voice envelope's fade.
- Optional stereo AUDIO IN bus. Input is processed through the same analog mixer drive, filter models, modulation, output saturation and shared post-synth FX. Stereo input keeps independent left/right filters. OPEN works without MIDI; MIDI / GRID GATE uses note envelopes. Input defaults off. Input gain resets to unity.
- Every knob/slider, including new controls and FX, uses right-click neutral reset.
- New parameters are appended; existing parameter IDs/indices, burgundy GUI styling, FX ordering and GRID → ARP remain intact.

## FL Studio

Load the VST3, enable/route its Audio In through Wrapper > Processing > Connections (or Patcher's audio routing), enable AUDIO IN, and choose OPEN or MIDI / GRID GATE. The plug-in cannot read unrelated mixer tracks without host routing. Grid/arp transport and tempo use the host playhead; disable HOST SYNC for preview playback. Polyphony is six complete analog voices, so playing six notes requires more CPU than Mono; idle poly voices are skipped.

This remains VST3, not an FL-native-format plugin. Native FL per-note slide flags/per-note automation are not promised. Windows automated tests verify processor/DSP behavior; interactive routing in FL Studio still requires a host smoke test.
