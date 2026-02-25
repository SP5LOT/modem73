# modem73 Windows GUI — Podręcznik użytkownika

> **English version available:** [WINDOWS_GUI.md](WINDOWS_GUI.md)

---

## Co to jest modem73?

**modem73** to programowy TNC (Terminal Node Controller) działający w oparciu o modem OFDM [aicodix](https://github.com/aicodix/modem). Umożliwia transmisję danych cyfrowych przez dowolny transceiver SSB lub FM przy użyciu zwykłej karty dźwiękowej lub interfejsu USB audio — bez dedykowanego sprzętowego TNC.

**Windows GUI** (`modem73_gui.exe`) to graficzny interfejs z podglądem konstelacji, historią SNR, podglądem poziomu sygnału oraz pełną konfiguracją przez okienkowy panel ustawień.

Główne cechy:
- Wiele trybów OFDM — dopasowanie przepustowości do warunków propagacji
- Wszystkie metody PTT: VOX, Hamlib rigctld, port szeregowy RTS/DTR
- Konstelacja IQ, historia SNR i wskaźnik poziomu sygnału w czasie rzeczywistym
- Protokół KISS przez TCP — kompatybilny z Dire Wolf, Pat Winlink, Xastir, YAAC i innymi
- Ustawienia zapisywane automatycznie między sesjami
- Brak instalatora — wystarczy rozpakować i uruchomić

---

## Wymagania systemowe

| Składnik | Wymaganie |
|----------|-----------|
| System | Windows 10 lub nowszy (64-bit) |
| Procesor | Dowolny nowoczesny x86-64 |
| RAM | ~50 MB |
| Audio | Interfejs USB audio lub wbudowana karta dźwiękowa podłączona do transceiveraa |
| Transceiver | Dowolny TRX SSB lub FM z wejściem/wyjściem audio |
| PTT | VOX (bez dodatkowego sprzętu), RTS/DTR przez port szeregowy, lub Hamlib rigctld |

---

## Pobieranie i instalacja

1. Przejdź na stronę [Releases](../../releases) i pobierz plik `modem73_gui_windows.zip`
2. Rozpakuj archiwum do wybranego folderu, np. `C:\modem73\`
3. W folderze znajdziesz dwa pliki:
   ```
   modem73_gui.exe       (program)
   libwinpthread-1.dll   (wymagane — musi być w tym samym folderze)
   ```
4. Kliknij dwukrotnie `modem73_gui.exe`, żeby uruchomić program

**Nie jest wymagany żaden instalator, MSYS2 ani Visual C++ Redistributable.**

Ustawienia są automatycznie zapisywane w:
```
C:\Users\<TwojaNazwa>\AppData\Roaming\modem73\settings.ini
```

---

## Podłączenie sprzętu

```
Transceiver
 ├── Wyjście audio (głośnik/gniazdo słuchawkowe)  ──►  Wejście LINE IN interfejsu  ──►  PC
 └── Wejście audio (mikrofon/DATA IN)              ◄──  Wyjście LINE OUT interfejsu  ◄──  PC
 └── PTT                                           ◄──  RTS/DTR portu szeregowego (opcjonalnie)
```

- Używaj **interfejsu USB audio** (nawet tani adapter za kilkanaście złotych wystarcza) — zapewnia separację galwaniczną i chroni TRX
- Wyjście audio TRX podłącz do wejścia liniowego/mikrofonowego interfejsu
- Wyjście liniowe interfejsu podłącz do wejścia mikrofonowego lub DATA IN TRX
- Ustaw poziomy tak, żeby wskaźnik poziomu w modem73 pokazywał **od −20 do −6 dBFS** podczas odbioru

---

## Opis interfejsu

Program podzielony jest na dwa panele:

| Panel | Zawartość |
|-------|-----------|
| **Lewy** | Wszystkie ustawienia: Callsign, Audio, Modem, PTT, CSMA, Sieć |
| **Prawy** | Podgląd w czasie rzeczywistym: wskaźnik poziomu, konstelacja, historia SNR, log |

---

## Konfiguracja

### Identity (Tożsamość)

| Pole | Opis |
|------|------|
| **Callsign** | Twój znak wywoławczy (np. `SP1ABC`) — wymagany do pracy stacji |

---

### Audio

| Pole | Opis |
|------|------|
| **Capture device** | Wejście audio — podłączone do wyjścia audio TRX (sygnał odbiorczy) |
| **Playback device** | Wyjście audio — podłączone do wejścia audio TRX (sygnał nadawczy) |
| **Refresh** | Odświeżenie listy urządzeń audio (użyj po podłączeniu interfejsu USB) |

> Jeśli interfejs USB nie pojawia się na liście — kliknij **Refresh** po podłączeniu.

---

### Modem

| Pole | Opis |
|------|------|
| **Modulation** | Tryb OFDM: liczba podnośnych × bity na symbol. Wyższy tryb = większa przepustowość, ale mniejsza odporność na zakłócenia |
| **Symbol Rate** | Liczba symboli OFDM na sekundę — wpływa na pasmo i odporność |
| **Center Freq (Hz)** | Częstotliwość środkowa tonu audio. **1500 Hz** to standard dla SSB. Musi mieścić się w paśmie przepustowym TRX |

modem73 wymaga **2400 Hz pasma audio**. Upewnij się, że pasmo TRX obejmuje zakres `Freq środkowa ± 1200 Hz`.

**Typowe ustawienia dla SSB na KF:**
- Tryb pracy TRX: USB
- Center Freq: 1500 Hz (sygnał zajmuje pasmo 300–2700 Hz)
- Tryb modem: na słabych pasmach zacznij od trybu o niższym rzędzie

---

### PTT

Wybierz metodę PTT odpowiednią do swojego sprzętu:

| Metoda | Opis |
|--------|------|
| **NONE** | Brak sterowania PTT — używaj VOX-a TRX lub ręcznego PTT |
| **VOX** | Programowy VOX — modem73 nadaje ton inicjujący, który wyzwala VOX TRX |
| **rigctl** | Hamlib rigctld przez sieć — działa z większością nowoczesnych TRX |
| **serial** | Linia RTS lub DTR portu szeregowego — wymaga kabla lub adaptera USB-RS232 |

#### Ustawienia VOX

| Ustawienie | Opis |
|------------|------|
| **Lead time (ms)** | Czas oczekiwania po wyzwoleniu PTT przed wysłaniem audio — pozwala TRX na przejście w tryb nadawania (typowo 200–500 ms) |
| **Tail time (ms)** | Czas trzymania PTT po zakończeniu audio (typowo 100–300 ms) |
| **Freq (Hz)** | Częstotliwość tonu wyzwalającego VOX |

#### Ustawienia rigctl

| Ustawienie | Opis |
|------------|------|
| **Host** | Adres serwera rigctld — domyślnie `localhost:4532` |

Przed uruchomieniem modem73 uruchom rigctld:
```
rigctld -m <numer_modelu> -r <port>
```
Do testów bez TRX: `rigctld -m 1 -r none`

#### Ustawienia PTT przez port szeregowy

| Ustawienie | Opis |
|------------|------|
| **COM Port** | Numer portu COM systemu Windows (np. `COM3`) — sprawdź w Menedżerze urządzeń |
| **Line** | **RTS** lub **DTR** — zależy od okablowania Twojego interfejsu |

---

### CSMA

CSMA (Carrier Sense Multiple Access) zapobiega kolizjom na kanałach współdzielonych.

| Ustawienie | Opis |
|------------|------|
| **P (0–255)** | Prawdopodobieństwo nadawania gdy kanał jest wolny. `255` = nadaj natychmiast. Niższe wartości wprowadzają losowy czas oczekiwania |
| **Slot (ms)** | Długość przedziału czasowego dla algorytmu CSMA |

> Na łączach simplex / punkt-punkt ustaw P=255 (bez CSMA). Na kanałach współdzielonych (APRS, digipeater) używaj P=63–128.

---

### Network (Sieć)

| Ustawienie | Opis |
|------------|------|
| **KISS Port** | Port TCP dla połączeń KISS — domyślnie `8001` |

---

## Uruchamianie TNC

1. Skonfiguruj wszystkie ustawienia w lewym panelu
2. Kliknij **Apply** — ustawienia zostaną zapisane i modem zostanie przygotowany
3. Kliknij **START** — TNC zaczyna nasłuchiwać; przycisk zmienia kolor na czerwony
4. Podłącz aplikację do `localhost:8001` używając protokołu KISS
5. Kliknij **STOP**, żeby zakończyć sesję

---

## Monitorowanie sygnału (prawy panel)

| Wskaźnik | Opis |
|----------|------|
| **Level bar** | Bieżący poziom sygnału audio. Cel: od −20 do −6 dBFS podczas odbioru. Unikaj saturacji (czerwone) |
| **Constellation** | Konstelacja IQ odbieranego sygnału OFDM. Wyraźne skupiska punktów = dobra jakość sygnału |
| **SNR history** | Historia stosunku sygnału do szumu |
| **Log** | Kolorowe komunikaty: zielony = odebrana ramka, żółty = ostrzeżenie, czerwony = błąd |

Prawidłowa konstelacja pokazuje skupiska w oczekiwanych pozycjach. Rozproszone punkty świadczą o słabym sygnale, QRM lub problemach z poziomami audio.

---

## Zapis i odczyt ustawień

### Automatyczny zapis
Ustawienia są zapisywane po każdym kliknięciu **Apply** i odtwarzane automatycznie przy następnym uruchomieniu.

### Presety — zapis do pliku

W panelu ustawień znajdują się dwa przyciski:

| Przycisk | Działanie |
|----------|-----------|
| **Save to file...** | Otwiera okno zapisu — zapisuje bieżące ustawienia do dowolnego pliku `.ini` |
| **Load from file...** | Otwiera okno otwarcia — wczytuje wcześniej zapisany preset |

Przydatne do szybkiego przełączania między konfiguracjami (różne pasma, TRX, simplex vs. digipeater).

### Uruchamianie z konkretnym plikiem konfiguracyjnym

```
modem73_gui.exe --config C:\sciezka\do\moich_ustawien.ini
```

---

## Połączenie z oprogramowaniem

### Reticulum Network Stack (główne zastosowanie)

modem73 powstał przede wszystkim po to, żeby umożliwić korzystanie z [Reticulum](https://reticulum.network) — szyfrowanego stosu sieciowego mesh — przez łącze radiowe, na systemie Windows. Reticulum zajmuje się routingiem, szyfrowaniem i niezawodnym dostarczaniem danych; modem73 jest jego interfejsem radiowym.

Po uruchomieniu modem73 skonfiguruj Reticulum, żeby korzystał z niego jako interfejs KISS TNC. Dodaj poniższy wpis do pliku konfiguracyjnego Reticulum (`%APPDATA%\Local\Programs\reticulum\config` lub tam, gdzie przechowuje go Twoja instalacja):

```
[[modem73 Radio]]
  type = TCPClientInterface
  enabled = yes
  kiss_framing = True
  target_host = 127.0.0.1
  target_port = 8001
```

> **Uwaga:** `TCPClientInterface` z `kiss_framing = True` to właściwy typ interfejsu dla modemów programowych i TNC eksponujących KISS przez TCP — dokładnie tak jak robi to modem73. Typ `KISSInterface` służy wyłącznie do fizycznych TNC podłączonych przez port szeregowy.

Po podłączeniu Reticulum do modem73 następujące aplikacje działają przez łącze radiowe od razu po konfiguracji — **bez Linuxa, bez instalacji serwerów**:

| Aplikacja | Opis | Link |
|-----------|------|------|
| **MeshChat** | Przeglądarkowy czat mesh dla Reticulum — prosty w obsłudze, działa na Windows | [github.com/liamcottle/reticulum-meshchat](https://github.com/liamcottle/reticulum-meshchat) |
| **MeshChatX** | Rozszerzona wersja MeshChat z dodatkowymi funkcjami | [git.quad4.io/RNS-Things/MeshChatX](https://git.quad4.io/RNS-Things/MeshChatX) |
| **Sideband** | Pełnoprawny klient Reticulum: wiadomości, transfer plików, mapy | [github.com/markqvist/Sideband](https://github.com/markqvist/Sideband) |

> **Dlaczego Reticulum przez radio?** W odróżnieniu od AX.25/APRS, Reticulum zapewnia szyfrowanie end-to-end, wielohopowy routing mesh i działa bez żadnej centralnej infrastruktury ani internetu. Doskonale nadaje się do łączności kryzysowej i pracy w terenie bez dostępu do sieci.

---

### AX.25 / APRS / Winlink

modem73 działa również jako standardowy TNC KISS dla tradycyjnego oprogramowania packet radio. Podłącz dowolną aplikację obsługującą KISS do `TCP localhost:8001`:

| Aplikacja | Konfiguracja |
|-----------|-------------|
| **Dire Wolf** | `KISSPORT 8001` w pliku direwolf.conf |
| **Pat Winlink** | Tryb KISS TNC, host `localhost`, port `8001` |
| **Xastir** | Typ interfejsu: KISS TNC, hostname `localhost`, port `8001` |
| **YAAC** | Połączenie szeregowe: TCP `localhost:8001` |

---

## Rozwiązywanie problemów

| Objaw | Prawdopodobna przyczyna | Rozwiązanie |
|-------|------------------------|-------------|
| Brak urządzeń audio na liście | Urządzenie niezauwzone | Kliknij **Refresh**; sprawdź połączenie USB |
| Konstelacja pokazuje tylko szum | Brak lub złe wejście audio | Sprawdź kabel; upewnij się, że TRX wysyła audio na RX |
| Zbyt niski poziom audio | Za mała czułość wejścia | Podnieś wzmocnienie mikrofonu w systemie lub poziom audio TRX |
| Saturacja sygnału (clipping) | Za wysoki poziom wejścia | Obniż poziom wejścia w systemie lub zmniejsz poziom audio TRX |
| PTT nie działa (rigctl) | rigctld nie uruchomiony | Uruchom rigctld przed modem73 |
| PTT nie działa (serial) | Zły numer COM lub linia | Sprawdź Menedżer urządzeń; spróbuj RTS zamiast DTR lub odwrotnie |
| Brak połączenia KISS | Zapora sieciowa blokuje port | Zezwól na `modem73_gui.exe` w Zaporze systemu Windows |
| Program nie startuje | Brakujący plik DLL | Upewnij się, że `libwinpthread-1.dll` jest w tym samym folderze co exe |
| Uszkodzone ustawienia | Błędny plik settings.ini | Usuń `%APPDATA%\modem73\settings.ini` i uruchom ponownie |

---

## Wskazówki dla krótkofalowców

- **Częstotliwość nośnej a wskazanie VFO**: modem73 nadaje na częstotliwości audio, którą ustawisz. Jeśli VFO wskazuje 14.100 MHz USB, a center freq wynosi 1500 Hz, to sygnał RF jest na 14.101,5 MHz.
- **Sprawdź pasmo przepustowe TRX**: Wiele nowoczesnych transceiverów ma regulowany filtr DSP — rozszerz go do co najmniej 2,5 kHz dla modem73.
- **ALC**: Podczas nadawania trzymaj odchylenie ALC na minimalnym poziomie. Ustaw moc TRX na 50–80% i steruj poziomem przez poziom audio.
- **FM vs SSB**: modem73 działa również na FM (UKF/packet). Dla FM czasy PTT są mniej krytyczne — ustaw większy lead time.
- **Test lokalny**: Możesz przetestować dwa egzemplarze modem73 połączone kablami audio lub pętlą programową (loopback).

---

## Podziękowania

Ten projekt jest **forkiem** oryginalnego **modem73** autorstwa [RFnexus](https://github.com/RFnexus):

> **[https://github.com/RFnexus/modem73](https://github.com/RFnexus/modem73)**

Wszelkie zasługi za silnik TNC, implementację protokołu KISS oraz sam koncept modem73 należą do oryginalnego autora. Ten fork dodaje:
- Natywne GUI dla Windows (Dear ImGui + GLFW + OpenGL)
- Wsparcie dla kompilacji pod Windows (toolchain MSYS2 UCRT64)
- Gotowe pliki binarne dla Windows — bez potrzeby kompilacji

Serdeczne podziękowania dla **RFnexus** za stworzenie modem73 i udostępnienie go jako open-source, oraz dla **[aicodix](https://github.com/aicodix/modem)** za świetną bibliotekę modemu OFDM, na której opiera się cały projekt.

---

## Licencja

modem73 jest oprogramowaniem open-source. Szczegóły w pliku [LICENSE](LICENSE).
Port na Windows GUI — kod źródłowy dostępny w tym repozytorium.
