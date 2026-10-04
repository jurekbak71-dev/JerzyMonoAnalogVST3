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
- siatka 8×8 RGB jako sekwencer / launch-pad
- root note, wybór skali i długość sekwencji do 8 banków / 64 kroków
- uruchamianie sekwencera przez MIDI
- parametry automatyzowalne przez host/DAW
- 4× wewnętrzny oversampling i filtracja antyaliasingowa

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
