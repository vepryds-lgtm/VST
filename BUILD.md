# AuraSynth + AuraDrums – byggeguide (Windows, VST3 til FL Studio)

> **Ærlig status:** Koden er kompileret og testet på **Linux** (JUCE 8.0.8, GCC/Ninja): motor, parametre, presets,
> MIDI, state save/restore og GUI-rendering er verificeret med en automatisk testsuite (`AuraRenderTest`, ~300 checks, også kørt med AddressSanitizer + UBSan).
> **Windows-bygget og plugin-scanning i FL Studio er IKKE testet** – det kræver en Windows-maskine.
> Lydene er målt og loudness-kalibreret, men ikke "lyttet til" af et menneske.
> Det samme gælder **AuraDrums** (se afsnit 9): 72 syntetiserede lyde, målt og balanceret, men ikke lyttet til; ikke prøvet i FL Studio.

## 0. Nemmeste vej
Dobbeltklik på **`INSTALL_AuraSynth.bat`** (pak zip-filen ud først). Den installerer selv Git, CMake og Visual Studio Build Tools via winget, henter JUCE, bygger og kopierer **begge plugins (AuraSynth og AuraDrums)** til FL Studios VST3-mappe. Derefter: *Options → Manage plugins → Find installed plugins*. (Scriptet er skrevet, men ikke kørt på en rigtig Windows-maskine.)

## 1. Forudsætninger
1. **Visual Studio 2022** (Community er gratis) med workload *Desktop development with C++*.
2. **CMake ≥ 3.22** (følger med VS, eller cmake.org).
3. **Git**.
4. JUCE 8.0.8 hentes af scriptet (eller manuelt, se nedenfor).

## 2. Byg
Åbn *x64 Native Tools Command Prompt for VS 2022* i projektmappen og kør:

    tools\build_windows.bat

Eller manuelt:

    git clone --depth 1 --branch 8.0.8 https://github.com/juce-framework/JUCE.git third_party\JUCE
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DAURA_BUILD_TESTS=OFF
    cmake --build build --config Release --target AuraSynth_VST3 AuraDrums_VST3 --parallel

Resultat: `build\AuraSynth_artefacts\Release\VST3\AuraSynth.vst3` og `build\AuraDrums_artefacts\Release\VST3\AuraDrums.vst3`

Uden Visual Studio-GUI: ingen krav – kun kommandolinjen ovenfor.
Ingen Windows-maskine? Push mappen til GitHub; `.github/workflows/windows.yml` bygger VST3'en og lægger den som download-artifact.

## 3. Installér
Kopiér hele mapperne `AuraSynth.vst3` og `AuraDrums.vst3` til `C:\Program Files\Common Files\VST3\` (kræver administrator).
Alternativt byg som administrator med `-DAURA_COPY_AFTER_BUILD=ON`.

## 4. FL Studio 26
1. *Options → Manage plugins*; sørg for at `C:\Program Files\Common Files\VST3` står under plugin-søgestier.
2. Klik **Find installed plugins** (evt. *Verify plugins*).
3. Find **AuraSynth** i *Plugin database → Installed → Generators → VST3* (eller via Add → More plugins), og tilføj på en kanal.
4. Spil MIDI. Forsiden (**SIMPLE**) har genre-knapper (HIP-HOP, MELODIC TRAP, R&B, JAZZ, AMBIENT, POP), lydtyper (Plucks, Leads, Pads, Keys, Bells, Textures, Bass), søgning, favoritter og seks knapper: Volume, Reverb, Delay, Brightness, Warmth, Movement. Knappen **RESET TO ORIGINAL SOUND** går tilbage til presettets egne indstillinger. **ADVANCED** viser alle tekniske knapper (oscillatorer, filter, mod-matrix, effekter, Layer B). Værten ser også de 165 factory presets som "programs".

### Hurtig start i FL Studio
Klik **MELODIC TRAP** → vælg fx *Plan A Pluck* eller *Airy Trap Pad* → spil. Egne lyde gemmes med **Save** og findes under *My sounds*.

## 5. Fejlfinding
| Fejl | Løsning |
|---|---|
| `Could not find JUCE` / `add_subdirectory ... does not contain CMakeLists.txt` | JUCE ikke hentet. Kør git clone-linjen, eller `-DAURA_JUCE_DIR=<sti>`. |
| `No CMAKE_CXX_COMPILER` / generator ikke fundet | Installér VS 2022 med C++-workload; brug `-G "Visual Studio 17 2022"`. |
| `C1083 cannot open include file ...juce_...` | Forkert JUCE-version/sti. Brug tag 8.0.8. |
| `LNK1104 cannot open file ...AuraSynth.vst3` / copy-fejl | Plugin er låst af FL Studio (luk den) eller mangler admin ved `AURA_COPY_AFTER_BUILD`. |
| FL finder ikke pluginet | Kopiér hele `.vst3`-**mappen** (ikke kun .dll), tjek søgesti, kør "Find installed plugins" igen. |
| Pluginet står som "failed" i scan | Installér *Microsoft Visual C++ Redistributable 2022 x64*; kør FL som normal bruger og scan igen. |
| Fejl i `Presets.cpp` ved start | `presetBuildErrors` viser ukendt parameter-ID; ret navnet i preset-tabellen. |
| Ændrede presets er for høje/lave | `AuraRenderTest --calibrate Source/PresetGains.inc` (Linux/Windows med `-DAURA_BUILD_TESTS=ON`) og genbyg. |

## 6. Tests (valgfrit)
    cmake -S . -B build -DAURA_BUILD_TESTS=ON
    cmake --build build --config Release --target AuraRenderTest AuraDrumsLibTest AuraDrumsPluginTest
    build\AuraRenderTest_artefacts\Release\AuraRenderTest.exe --quick
    build\Release\AuraDrumsLibTest.exe            (lydbibliotek: niveau, balance, tonehøjde, motor)
    build\AuraDrumsPluginTest_artefacts\Release\AuraDrumsPluginTest.exe   (selve pluginet: state, MIDI, tone/space)

## 7. Licens
JUCE er under AGPLv3/kommerciel licens. Til personlig brug er det fint; hvis du distribuerer pluginet, skal
kildekoden frigives under AGPLv3, eller du skal have en JUCE-licens. VST3 er et varemærke tilhørende Steinberg.

## 8. Arkitektur (kort)
`Params` (én parametertabel, 115 stk.) → `PluginProcessor` (APVTS, MIDI, state, programs) → `SynthEngine`
(poly/mono/legato, unison, sync/FM, SVF-filtre, ADSR, 3 LFO + 6-slot mod-matrix, arp) → `FxChain` → output.
`Presets` (165 presets i 20 kategorier, tagget med genre og lydtype) + `PresetManager` (søg, favoritter, bruger-presets `.aurapreset`)
→ `PluginEditor`.

## 9. AuraDrums (trommeplugin)
Et separat plugin ved siden af AuraSynth: **72 syntetiserede lyde** (ingen samples, ingen ophavsretsproblemer) i stil med en mørk, melodisk trap-lyd
("Plan A"-stil er min fortolkning – jeg har ikke kunnet sammenligne med de rigtige optagelser):

| Kategori | Antal |
|---|---|
| Kicks | 8 |
| 808s | 8 (tonehøjde følger tangenterne, nul-pitch = tangent 37 = C#3 i FL) |
| Claps | 8 |
| Snares | 7 |
| Hi-hats (lukkede + åbne + shaker) | 10 (lukket hat afbryder åben hat) |
| Percs | 18 |
| FX & Textures (risers, sub drop, impact, tape stop, vinyl, vokal-"ooh" m.m.) | 13 |

**Brug:** vælg en lyd (klik på feltet = den spilles) – færdig. Alle lyde er højpasset, mono i bassen, loudness-matchet pr. kategori og har headroom, så kick + 808 + clap + hat kan spilles oven på hinanden uden at clippe.
`Volume`, `Tune`, `Length` (kun kortere), `Tone` (mørk/lys) og `Space` (rumklang uden bas) er valgfri.
**Single sound** = den valgte lyde ligger på alle tangenter (808 og tonale percs følger tangenterne). **Full kit** = 6 kits med GM-tastekort (kick C3 = MIDI 36, snare D3 = 38, clap D#3 = 39, closed hat F#3 = 42, open hat A#3 = 46), så en almindelig trommer-MIDI-fil spiller direkte.
Lydene syntetiseres i koden ved 48 kHz og afspilles med Hermite-interpolation ved værtens sample rate; hele biblioteket (ca. 0,7 s) bygges én gang når pluginet første gang indlæses og deles mellem alle instanser.

Kildekode: `Drums/DrumSounds.cpp` (lydene + loudness-finish), `Drums/DrumEngine.cpp` (stemmer, 808-mono, choke, gate), `Drums/DrumsProcessor.cpp`, `Drums/DrumsEditor.cpp`.
Eksport af alle lyde som 24-bit WAV (til at trække ind i FL's Channel sampler / FPC): `AuraDrumsLibTest --wav <mappe>`.
