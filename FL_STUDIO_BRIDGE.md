# FL Studio integration — development branch v0.1
## Current instrument capabilities
- Stereo AUDIO IN remains an FX-variant routing concern; do not assume instrument variant accepts Mixer inserts.
- Mono Analog Grid has two independent MIDI channels in PARAPHONIC mode; use two FL MIDI Out channels routed to the wrapper input port, with channels 1 and 2.
- Existing GRID/ARP sync and MIDI output should remain unchanged. Do not remap existing parameter IDs.
## Bridge workflow
1. In FL Studio use Jerzy Arp Chords.pyscript on a chord track or export generated JPF via `python -m jerzybridge to-midi`.
2. Import resulting MIDI to the two MIDI Out channels or into the instrument channel for direct play.
3. When playing Piano Roll directly, disable `GRID SEQ ON` and `ARP ON`; enable them only for intentional internal sequencing.
## Verification required
Measure DSP with both VST3 variants at 44.1/48/96 kHz, idle and 6 voices; isolate AUDIO IN routing and chorus crackle before production release.
No DSP or GUI changes were merged by this integration notes commit.