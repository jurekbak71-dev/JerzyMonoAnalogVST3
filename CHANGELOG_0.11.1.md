# 0.11.1 — paraphonic AMP release and two Piano Rolls

- Fixed the shared AMP envelope's Release: released oscillators remain signal sources for the VCA tail instead of fading out in 3 ms. Simultaneous releases retain both sounding oscillator gains; a new phrase does not revive the other oscillator's previous tail.
- Direct host MIDI in Paraphonic mode now explicitly addresses CH 1 = OSC 1/sub and CH 2 = OSC 2. Other channels are ignored in this mode. Mono/Poly 6 retain their existing note allocation.
- Separate, fixed-storage held-key lists and velocities per oscillator, with last/low/high note priority. Overlapping Piano Roll notes return to the previously held key of the same channel. Same-pitch note-offs cannot release the other oscillator.
- Channel-scoped CC 123 releases held keys; CC 120 cuts that oscillator and clears its keys. MIDI/grid-gated external audio receives oscillator-scoped triggers as well.
- Shared AMP and filter envelope triggering respects LEGATO/RETRIGGER: first note starts attack, last release starts release; disable LEGATO or enable RETRIGGER for each incoming note to renew the attack. Added an explanation to OSC/MIX and FL routing instructions to README.
- Regression tests measure rendered AMP attack/decay/sustain/release, independent MIDI addressing, identical pitches, held-key fallback, note flush and retrigger. Processor tests feed both channels through processBlock. Existing parameter IDs and preset schema are unchanged.
