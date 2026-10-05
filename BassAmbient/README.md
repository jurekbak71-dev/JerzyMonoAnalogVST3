# Jerzy Bass Ambient 0.2.0

Osobny instrument VST3 / Standalone dla Windows x64, prototyp do odsłuchu w FL Studio.
Nie zastępuje Mono Analog Grid: inna nazwa, identyfikator VST3 i osobny projekt CMake.

## Obsługa w FL Studio

1. Skopiuj cały folder `Jerzy Bass Ambient.vst3` do `C:\Program Files\Common Files\VST3`.
2. W Plugin Manager FL Studio wykonaj skan, dodaj instrument do Channel Rack.
3. W Piano Roll narysuj długą nutę, np. A2. Generator basu pracuje podczas jej trwania.
4. Wybierz factory scene i styl / model. Kolejne nuty przenoszą podstawę frazy.
5. DIRECT wyłącza generator: każda nuta MIDI gra bezpośrednio basem.
6. W AMBIENT włącz pady i wybierz akord, dwa silniki oraz HOLD / FLOW / KRELL.
7. Trzy strony: BASS (synteza i generator), AMBIENT (pady), FX.
8. W FX wybierz BASS / AMBIENT / MASTER. Kliknij moduł, aby edytować jego parametry.
   Strzałki przestawiają moduły wyłącznie wybranego racka. Każdy rack ma własny udział FX.
   MASTER przetwarza sumę basu i pada; domyślnie jego udział wynosi 0.
   Końcowy poziom i soft limiter są dostępne w racku MASTER.
9. Parametry można automatyzować przez Browse parameters / Last tweaked FL Studio.

Skala jest wybierana w instrumencie; nie analizujemy harmonii utworu. MIDI dostarcza
podstawę, długość i dynamikę. Tempo, metrum i pozycja rytmiczna pochodzą z hosta.
Domyślnie generator zachowuje pozycję frazy przy zmianie nuty; Restart on root uruchamia ją od początku.
Latch utrzymuje podstawę po NOTE OFF. PANIC wyłącza także latch i czyści bufory FX.
Pitch bend: +/- 2 półtony. Obsługiwany sustain CC64 i all-notes-off.

## Brzmienie

- Bas: ACID 303 (inspirowany, nie emulacja obwodu 1:1), SUB 808 strojonym MIDI,
  Classic Analog (2 oscylatory i sub).
- Oscylatory basu korzystają z PolyBLEP. Głosy i nieliniowy filtr pracują w 2x częstotliwości.
- Pady: 12 głosów, 2 silniki na głos, analog saw / PWM / spectral / FM / grain texture.
  Grain texture jest proceduralnym oscylatorem, nie importerem sampli.
- Granular FX przetwarza rzeczywisty bufor audio syntezatora, z pitch i freeze.
- Trzy niezależne racki FX: BASS, AMBIENT i MASTER. Własne bufory, parametry,
  kolejność i udział efektów; automatyzacja i zapis wszystkich ustawień.
- Projekty 0.1.0 zachowują brzmienie: stare wspólne ustawienia FX są kopiowane
  do racka AMBIENT, a udział nowego MASTER pozostaje zerowy.
- Drive / soft-knee kompresor, chorus / flanger / Juno-inspired, delay sync
  clean / tape / analog, granular, stereo reverb, width, końcowy soft limiter.

Seed i parametry są zapisywane w projekcie. Bez Evolve fraza basowa jest powtarzalna.
Ambient jest deterministyczny przy odtworzeniu tego samego przebiegu od początku,
nie rekonstruuje całej wcześniejszej ewolucji przy skoku w środek utworu.

## Ograniczenia pierwszego prototypu

To punkt wyjścia do testów brzmieniowych, nie ukończony instrument produkcyjny.
Nie ma jeszcze ręcznej edycji kroków, eksportu frazy do MIDI, importu sampli,
oddzielnych wyjść audio, arpeggiatora akordowego, rotary ani pełnego modelowania
analogowych obwodów. Metrum wyznacza długość frazy; własny edytor grup akcentów
(np. 2+2+3) pozostaje do dodania. Parametry są wygładzane audio-rate,
ale automatykę pobieramy na granicach bloków hosta, bez obietnicy sample-accurate automation.

## Build

JUCE 9.0.3, CMake >= 3.24, C++17, Visual Studio 2022:

```
cmake -S BassAmbient -B build-bass-ambient -G "Visual Studio 17 2022" -A x64
cmake --build build-bass-ambient --config Release --parallel 2
ctest --test-dir build-bass-ambient -C Release --output-on-failure
```

Testy samego silnika nie wymagają JUCE:

```
g++ -std=c++17 -O2 -I BassAmbient/Source BassAmbient/tests/CoreTests.cpp -o bass-ambient-tests
./bass-ambient-tests
```

Przed dystrybucją trzeba dobrać odpowiednią licencję JUCE i wykonać odsłuchy oraz testy
GUI, skalowania, automatyki, zapisu projektu i obciążenia w rzeczywistym FL Studio.
