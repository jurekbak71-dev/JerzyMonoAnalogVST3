# Bass Ambient 0.4.0 — iPhone / iPad

Samodzielny instrument na iOS 16 lub nowszy. Działa w poziomie. Plik IPA jest kompilacją dla fizycznego urządzenia ARM64, bez podpisu wydawcy. Podpis zostaje nadany na Twoim Windows przez Sideloadly z użyciem bezpłatnego konta Apple. Nie wymaga płatnego Apple Developer ani TestFlight.

## Instalacja z Windows

1. Pobierz Sideloadly wyłącznie z https://sideloadly.io/ . Zainstaluj wymagane przez jego aktualną instrukcję wersje iTunes i iCloud (ze strony Apple, nie Microsoft Store).
2. Podłącz odblokowanego iPhone'a kablem USB. Na telefonie potwierdź „Zaufaj temu komputerowi”.
3. Otwórz Sideloadly. Wybierz iPhone'a i przeciągnij `Bass-Ambient-0.4.0-iOS.ipa` do programu.
4. Wpisz konto Apple w Sideloadly, kliknij Start i potwierdź logowanie/2FA, jeśli program o to poprosi. Logowanie wykonujesz samodzielnie — nie przesyłaj hasła ani kodów w rozmowie.
5. Jeśli iPhone wymaga zaufania profilowi: Ustawienia → Ogólne → VPN i urządzenia zarządzane → aplikacja dewelopera → Zaufaj. Nazwy pozycji mogą różnić się zależnie od wersji iOS.
6. Włącz Tryb dewelopera: Ustawienia → Prywatność i ochrona → Tryb dewelopera; uruchom telefon ponownie i potwierdź. Ta pozycja może pojawić się dopiero po pierwszej próbie instalacji.
7. Uruchom Bass Ambient i obróć telefon poziomo. Numer wersji w górnym pasku: 0.4.0.

## Bezpłatny podpis — 7 dni

Bezpłatny podpis wygasa po 7 dniach. Włącz automatyczne odnawianie w Sideloadly i regularnie udostępniaj mu telefon przy uruchomionym komputerze (USB; Wi-Fi po konfiguracji parowania zgodnie z instrukcją Sideloadly). Po wygaśnięciu ponów instalację tym samym kontem i identyfikatorem aplikacji. Nie usuwaj aplikacji przed aktualizacją: usunięcie kasuje jej lokalne dane. Apple ogranicza liczbę aktywnych aplikacji instalowanych tą metodą; dla instrumentu potrzebny jest jeden wolny slot.

Instrukcja i aktualne wymagania: https://sideloadly.io/faq.html . Nie stosuj płatnych certyfikatów ani jailbreaka.

## Pierwsze granie

- Wybierz `Rock Pick Bass` i dotknij **BASS START**. **BASS STOP** zatrzymuje generowanie, pozostawiając wybrzmienie. **PANIC** natychmiast wycisza wszystkie warstwy.
- Dla ambientu wybierz `Living Landscape` lub `Krell Laboratory` i **AMBIENT START**.
- **KEYBOARD** pokazuje klawiaturę ekranową. Dla bezpośredniej gry basem włącz BASS → PHRASE → DIRECT MIDI. Dla fraz generowanych pozostaw DIRECT wyłączone i przytrzymaj klawisz. Przy grze klawiaturą wyłącz przyciski START odsłuchu, aby nie mieszać dwóch źródeł wyzwalania.
- Gitara: BASS → SOUND → Bass instrument → Bass guitar. Style: BASS → PHRASE → Bass style. Tempo i nuta przycisków START: BASS → PERFORMANCE.
- **MORE**: Generate, Mutate, Undo, zapis/odczyt scen A–D i eksport MIDI/WAV. Wszystkie trzy racki efektów pozostają w FX.
- Przewijaj stronę po tle. Przesunięcie na gałce reguluje parametr. Klawiaturę można schować, aby powiększyć obszar edycji.
- **AUDIO** otwiera ustawienia dźwięku i wejść MIDI. Nowo podłączone wejścia MIDI są automatycznie włączane; możesz je wyłączyć w ustawieniach.
- Ustawienia i sceny są zapisywane automatycznie. Przejście aplikacji w tło wyłącza dźwięk; po powrocie uruchom odtwarzanie ponownie.
- Eksport MIDI/WAV zapisuje ostatnie 16 ćwierćnut, maksymalnie 30 sekund. Otwiera systemowe udostępnianie; wybierz „Zachowaj w Plikach”. Kopia jest także w Pliki → Na moim iPhonie → Bass Ambient. Zapis nie wysyła danych automatycznie.
- Do grania z małym opóźnieniem używaj głośnika lub słuchawek/interfejsu przewodowego. Bluetooth audio może wprowadzać wyraźne opóźnienie.

## Weryfikacja

Budowa urządzenia i symulatora, testy DSP względem wersji desktopowej oraz test klawiatury, presetów, scen, zapisu i eksportu w symulatorze są wykonywane w GitHub Actions. Instalacja przez Sideloadly, rzeczywista latencja, podłączane urządzenia MIDI, obciążenie i temperatura wymagają sprawdzenia na fizycznym iPhonie. Symulator nie zastępuje tych testów.
