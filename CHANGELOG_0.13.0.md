# 0.13.0 — dual arpeggiators, MIDI root and audio input

- SHARED ARP explicitly drives OSC 1 and OSC 2 in paraphonic mode.
- DUAL ARP uses two independent clocks and note pools: MIDI CH 1 / GRID 1 -> OSC 1, MIDI CH 2 / GRID 2 -> OSC 2. DUAL activates the paraphonic engine while ARP is enabled, with shared AMP/filter controls.
- Independent division (including triplets), order, rhythm, octave range, gate, velocity, swing, probability, host sync, latch and retrigger.
- Added Diverge, Random Walk, Down-Up, Offbeat, Euclidean 3/8 and 5/8. Existing choice indices and parameter IDs are preserved.
- GRID FOLLOW MIDI is separate from MIDI TRIG. The newest held note becomes ROOT; releasing it restores the previous held note. After the last release the root remains at the last played note. Manual root mode remains available.
- Active stereo AUDIO IN bus, declared by default. Input is read before output overwrite and is processed independently in stereo.
- Added Jerzy Mono Analog Grid FX (separate VST3 identifier/category) for direct use in a Mixer slot without Patcher.
- Output meter includes the external audio input path.
- MIDI note-off, All Notes Off, host STOP/seek, lane isolation, grid/launch routing and audio bus/stereo covered by regressions.

FL Studio audio routing:
1. Install both VST3 folders and rescan plugins. Use Refresh plugin properties on an older Wrapper instance if its input list is stale.
2. For direct external audio processing, load Jerzy Mono Analog Grid FX into an effect slot on the receiving Mixer insert.
3. Route the chosen source Mixer insert to that insert. Set AUDIO IN ON, OPEN INPUT and INPUT GAIN 1.00. Set oscillator/sub/noise levels to zero for external-audio-only processing.
4. MIDI Out can feed the FX Wrapper's MIDI input port, using channels 1 and 2 for DUAL ARP. The Instrument variant remains available for Channel Rack use.

The actual FL Studio session must be checked on the user's machine; processor, DSP, VST3 build and bus declarations are tested here.
