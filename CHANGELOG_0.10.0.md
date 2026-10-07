# 0.10.0

- Nowy automatyzowalny przełącznik `GRID → ARP` na stronach GRID i ARP.
- Nuty sekwencera (także ratchety) i padów Launch trafiają do ARP, z poprawnym zwalnianiem bramek; bez zdublowanego wyjścia MIDI z GRID.
- Pad Launch przekazuje zdarzenie do wątku audio, bez wywoływania silnika syntezy z wątku GUI.
- Parametry sekwencera są pobierane raz na blok zamiast raz na próbkę; MIDI wykorzystuje ponownie pamięć buforów.
- Wyłączone efekty pomijają obliczenia, odczyt `Comp Drive` i `Width` jest poza pętlą próbek, a opóźnienie i stereo width używają tańszego zawijania indeksu.
- Statyczna powierzchnia GUI nie jest odmalowywana przez licznik poziomu.
- Brzmienie syntezy, oversampling, algorytmy aktywnych efektów i dotychczasowe presety są zachowane. Nowa opcja jest domyślnie wyłączona.
