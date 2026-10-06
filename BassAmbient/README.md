# Jerzy Bass Ambient 0.3.0 — Windows x64 VST3 + Standalone

Instrument generatywny sterowany nutami z Piano Roll. BASS, AMBIENT i trzy osobne racki BASS / AMBIENT / MASTER. Identyfikator VST3 pozostaje ten sam co w 0.2.0.

## Instalacja

Zamknij host. Zastąp cały folder `Jerzy Bass Ambient.vst3` w `C:\Program Files\Common Files\VST3`, następnie przeskanuj wtyczki w FL Studio. Zachowaj kopię 0.2.0 i projektu przed podmianą. Plik EXE działa samodzielnie: w jego ustawieniach wybierz urządzenie audio i wejście MIDI.

## Szybki start

- Wybierz preset `Rock Pick Bass`, `Gothic Bass`, `Living Landscape`, `Rhythmic Piano` lub `Krell Laboratory`.
- W FL Studio wpisz długą nutę w Piano Roll. Jej wysokość określa podstawę, a długość — działanie generatora. Note Off zatrzymuje nowe zdarzenia; obwiednie i FX wybrzmiewają.
- Przy zatrzymanym transporcie użyj `BASS START` i/lub `AMBIENT START`. W BASS / PERFORMANCE ustaw nutę MIDI i tempo odsłuchu. Te parametry są też dostępne w AMBIENT / EVOLUTION.
- Start hosta wyłącza odsłuch próbny; sterowanie przejmuje MIDI. `PANIC` wyłącza odsłuch i czyści głosy oraz bufory efektów.

## BASS

Cztery źródła: ACID 303, SUB 808, Classic Analog i Bass guitar. Gitara jest syntezą modalną struny, z artykulacją Finger / Pick / Muted, regulacją jasności i tłumienia; nie jest biblioteką próbek prawdziwej gitary.

Osiem stylów: Italo Disco, Disco Polo, ACID, Funky, Techno, Rock, Post-punk i 808 / Trap. Trzy warianty każdego stylu zmieniają rozkład akcentów, pauzy, ruch melodyczny i artykulację. Styl jest niezależny od źródła dźwięku. Dostępne są synkopa, swing, gęstość, gate, slide, akcent, humanizacja, długość frazy i przejścia przy włączonym Evolve.

`DIRECT` odtwarza własne nuty bez generatora. Sustain CC64, pitch bend +/-2 półtony oraz Note Off działają w syntezie. CC1 zwiększa intensywność artefaktów ambientu; pozostałe parametry można przypisać do kontrolera przez automatykę hosta.

## AMBIENT

- BACKGROUND: Hold / Flow / Krell, dwie barwy na głos, nakładające się obwiednie, prowadzenie głosów, dryf i modulacja filtra oraz proporcji źródeł.
- RHYTHM: Synth piano / Electric piano / Pluck / Mallet, akordy jednoczesne, rozłożone, arpeggio i naprzemienne; osobny rytm, swing, gate, poziom oraz odpowiedzi w przerwach basu. Synth piano jest barwą syntezowaną, nie fotorealistycznym fortepianem z próbek.
- KRELL: niezależna warstwa zdarzeń; zmienne wysokości, czasy i obwiednie, osobny czas udziału drugiego oscylatora, różne interwały, FM i filtr. Event interval oznacza sekundy w trybie swobodnym, a ćwierćnuty przy beat sync. Engine 1/2 są wspólne dla tła i zdarzeń; FM jest słyszalne przy silniku FM.
- EVOLUTION: wolne zmiany, zakres ewolucji, częstość i siła nieregularnych trzasków i ubytków. Artefakty wpływają na suchy tor ambientu, również bez delay. Granular w racku FX przetwarza bufor syntezatora i ma Freeze. Oscylator Grain texture jest proceduralną barwą, nie samplerem.

Każda z trzech warstw ma osobny wyłącznik i poziom. Warstwy korzystają z tego samego wyjściowego racka AMBIENT.

## Generacja, sceny, zapis pomysłu

GENERATE tworzy nowe ziarno i wariant ustawień. MUTATE zmienia tylko część kroków, zależnie od Mutation amount, i delikatnie zmienia barwę. UNDO przywraca poprzedni stan generacji. W BASS / PHRASE dostępne są blokady nut, rytmu, barwy i FX. Blokady dotyczą generowania; nie blokują ręcznej edycji ani automatyki. Ziarna i ustawienia są zapisywane w projekcie. Powtarzalność zakłada tę samą częstotliwość próbkowania i identyczny start transportu/MIDI.

STORE, następnie A/B/C/D zapisuje komplet ustawień. Naciśnięcie zapisanej sceny przy odtwarzaniu przywołuje ją na następnej granicy taktu; przy zatrzymanym hoście następuje to od razu. Sceny są zapisane wraz z projektem. Parametr Recall scene pozwala automatyzować wybór; MIDI CC20 wybiera A/B/C/D zakresami 0–31 / 32–63 / 64–95 / 96–127. Start i skok transportu resetują generatory oraz historię FX, aby uniknąć poprzednich losowań i wiszących nut.

SAVE MIDI / SAVE WAV eksportuje ostatnie 16 ćwierćnut, maksymalnie 30 sekund. W MIDI bas ma kanał 1, akordy/tło kanał 2, zdarzenia kanał 3. WAV zachowuje brzmienie i efekty. MIDI zachowuje nuty, rytm i velocity; nie zapisuje modulacji DSP, FX ani slide jako nut pośrednich. Metadane tempa MIDI używają tempa z chwili eksportu; przy zmiennym tempie ustaw odpowiednią mapę w docelowym projekcie. W bardzo wolnym tempie bufor może zawierać mniej niż 16 ćwierćnut. Eksportuj bezpośrednio po udanym fragmencie — bufor rejestruje również ciszę.

## GUI i zgodność

BASS jest bursztynowy, AMBIENT turkusowy, FX fioletowy. Trzy główne strony, podsekcje, wektorowe gałki z cieniowaniem i pionowe przewijanie zachowują czytelność od 840 x 600. Kontrolki i racki obsługują automatykę hosta. Zapis schematów 1/2 jest migrowany do nowych ustawień.

Dodanie nowych pozycji do list Model i Style zmienia ich mapowanie znormalizowane: sprawdź starsze klipy automatyki tych dwóch list. Wartości zapisane w samym stanie instrumentu zachowują indeksy starych pozycji.

Testy automatyczne obejmują DSP, pamięć, MIDI, trzy racki, migrację, odsłuch, sceny, eksport i obrazy GUI. Nie zastępują odsłuchu ani testu we właściwym FL Studio. W tej sesji nie przeprowadzono testu w rzeczywistym FL Studio. Jeżeli host usypia wtyczkę podczas odsłuchu przy zatrzymanym transporcie, wyłącz Smart Disable dla tej instancji.

## Budowa

CMake >=3.24, C++17, JUCE 9.0.3. `cmake -S BassAmbient -B build-bass-ambient`, potem kompilacja Release i `ctest`. `BASS_AMBIENT_CORE_ONLY=ON` buduje same testy DSP bez JUCE. Workflow Windows dołącza VST3, EXE i tę instrukcję.
