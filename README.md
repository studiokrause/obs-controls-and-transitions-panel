# OBS Horizontal Panels

Trzy niezależne wtyczki OBS Studio z **poziomymi** wersjami standardowych paneli.
Wszystkie kontrolki stoją obok siebie w jednym rzędzie, dzięki czemu panel można
zadokować jako wąski pasek (np. pod podglądem).

| Wtyczka | Dok (menu *Doki*) | Zawartość |
|---|---|---|
| `horizontal-controls` | **Sterowanie (H)** | Rozpocznij transmisję, Transmisja (YouTube), Nagrywanie + pauza, Bufor powtórek + zapis, Wirtualna kamera + konfiguracja, Tryb studyjny, Ustawienia |
| `horizontal-scene-transitions` | **Przejścia scen (H)** | Lista przejść, dodaj / usuń / właściwości, czas trwania |
| `horizontal-studio-transitions` | **Przejścia - tryb studyjny (H)** | Przycisk *Przejście*, menu opcji, szybkie przejścia (z menu), dodaj szybkie przejście, T-bar |

Kolory przycisków (stan wyłączony / włączony):

- *Rozpocznij transmisję*, *Rozpocznij nagrywanie* — ciemnoczerwony / czerwony
- *Uruchom wirtualną kamerę* — ciemnozielony / zielony
- *Tryb studyjny* — ciemnoniebieski / niebieski
- *Przejście* (tryb studyjny) — niebieski

Każdą wtyczkę instaluje się osobno — można użyć jednej, dwóch albo wszystkich.

## Jak to działa i dlaczego jest odporne na nowe wersje OBS

Panele są **lustrzanymi odbiciami** oryginalnych paneli OBS: ich przyciski kopiują
stan oryginałów (tekst, aktywność, widoczność, klasy motywu, menu) i przekazują do
nich kliknięcia. Całą pracę wykonuje więc sam OBS — potwierdzenia, transmisje z
opóźnieniem, przepływ YouTube, skróty klawiszowe, menu szybkich przejść, T-bar —
i panele zachowują się dokładnie tak jak oryginał, także w motywach.

- Oryginalne kontrolki są wyszukiwane po nazwach obiektów Qt, a panel trybu
  studyjnego po T-barze, więc działa to również wtedy, gdy panel przeniosła inna
  wtyczka (np. [obs-panel-mover](https://github.com/studiokrause/obs-panel-mover)).
- Jeżeli przyszła wersja OBS zmieni wnętrze okna i oryginału nie da się znaleźć,
  panel przełącza się na publiczne **obs-frontend-api** (start/stop wyjść, tryb
  studyjny, przejścia, czas trwania, T-bar) zamiast przestać działać.
- Wtyczki używają tylko stabilnego C API libobs / obs-frontend-api i Qt 6 Widgets,
  bez prywatnych symboli OBS.
- Oryginalne doki mogą być zamknięte — panele nadal działają.

Wymaga **OBS Studio 31.1 lub nowszego** (Windows x64). Przetestowane w OBS 33.0.0.

## Instalacja (Windows)

1. Pobierz ZIP z zakładki *Releases*: pojedyncze wtyczki
   (`horizontal-controls-<wersja>-windows-x64.zip` itd.) albo wszystkie razem
   (`obs-horizontal-panels-<wersja>-windows-x64.zip`).
2. Rozpakuj do `C:\ProgramData\obs-studio\plugins\` tak, aby powstało np.:

   ```
   C:\ProgramData\obs-studio\plugins\horizontal-controls\bin\64bit\horizontal-controls.dll
   C:\ProgramData\obs-studio\plugins\horizontal-controls\data\locale\*.ini
   ```

3. Uruchom OBS i włącz panele w menu *Doki*. Ich położenie OBS zapamiętuje razem
   z resztą układu.

Locale: `en-US`, `pl-PL`, `de-DE`. Teksty przycisków pochodzą z tłumaczeń OBS,
więc pasują do języka interfejsu.

## Budowanie

Potrzebne: CMake 3.22+, Visual Studio (MSVC), Qt 6 oraz pakiety CMake `libobs` i
`obs-frontend-api` (np. z [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate)
/ zbudowanego OBS i `obs-deps`).

```bash
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DCMAKE_PREFIX_PATH="C:/path/to/obs-sdk;C:/path/to/obs-deps-qt6;C:/path/to/obs-deps"
```

```bash
cmake --build build --config RelWithDebInfo
```

```bash
cmake --install build --config RelWithDebInfo --prefix dist
```

Release 1.0.1+ zbudowano z nagłówkami OBS 31.1.0 i Qt 6.8 (`obs-deps` 2025-05-23).
Zbudowane ze starszymi nagłówkami działają też w nowszym OBS (libobs i Qt 6
zachowują zgodność binarną wstecz) — nie odwrotnie.

## Struktura

```
common/hpanels.hpp                 wspólny kod: MirrorButton, HorizontalPanel
horizontal-controls/               wtyczka: Sterowanie
horizontal-scene-transitions/      wtyczka: Przejścia scen
horizontal-studio-transitions/     wtyczka: Przejścia trybu studyjnego
```

## Wersjonowanie

Wersje numerowane od `1.0.1` (`1.0.1`, `1.0.2`, …), wspólne dla trzech wtyczek.
Numer jest w `CMakeLists.txt` (`project(... VERSION ...)`), tag release to `v<wersja>`.

## Licencja

GPL-2.0-or-later (jak OBS Studio).
