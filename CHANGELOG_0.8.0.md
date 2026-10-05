# Jerzy Mono Analog Grid 0.8.0

- Reorganized the interface into dedicated Oscillator/Mixer, Filter/Envelope,
  Modulation, Grid, Arp, and FX pages with a high-contrast vintage console look.
- Rebuilt rotary controls with a shaded metal body, bevel, and amber pointer.
  Parameter readouts use real units rounded to whole values.
- Mixer and output drive now blend clean and driven paths without the former
  level compensation; floating-point headroom is retained for output FX.
- Compressor now uses a linked detector, soft knee, conservative automatic
  makeup gain, and drive coloration; it also works on mono output layouts.
- Added Grid probability and 1-4 ratchets, plus new Arp order and rhythm patterns.
- MIDI and host transport synchronization remain enabled. New parameters are
  appended to preserve existing automation and project state.
