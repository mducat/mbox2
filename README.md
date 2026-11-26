# Music Box 2

Aide à la composition centrée sur la théorie musicale et ses représentations
géométriques (cercle des quintes, Tonnetz, polygones d'accords), sous forme de
plugin **VST3** / application **Standalone**, pensé pour **FL Studio**.

> État actuel : squelette. Le plugin reçoit du MIDI, le renvoie tel quel, et
> affiche les notes jouées sur le cercle des quintes avec le nom de l'accord
> reconnu.

## Architecture

```
mbox2/
├── core/                 # Bibliothèque C++20 pure (aucune dépendance JUCE ni UI)
│   ├── include/mbox/theory/
│   │   ├── PitchClass.h  # classes de hauteur, ensembles (masque 12 bits), cercle des quintes
│   │   ├── Scale.h       # gammes et modes, degrés, quantification
│   │   └── Chord.h       # qualités d'accords, reconnaissance d'accords
│   ├── src/
│   └── tests/            # tests unitaires (Catch2)
├── plugin/               # Plugin JUCE qui utilise mbox::theory
│   └── Source/           # PluginProcessor (MIDI) + PluginEditor (UI provisoire)
└── CMakeLists.txt
```

Toute la logique musicale vit dans `core/` (cible CMake `mbox::theory`) : elle se
teste sans DAW et pourra être réutilisée ailleurs. Le plugin n'est qu'une
enveloppe JUCE (MIDI, état, interface).

## Prérequis (Windows)

- **Visual Studio 2022 ou plus récent** (ou Build Tools) avec la charge de travail
  « Développement Desktop en C++ »
- **CMake ≥ 3.22** (inclus avec Visual Studio, ou https://cmake.org)
- **Git**

JUCE 8 et Catch2 sont téléchargés automatiquement par CMake au premier configure
(FetchContent), rien à installer à la main.

## Compiler

Depuis une invite « Developer PowerShell for VS » à la racine du repo :

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

CMake choisit automatiquement la version de Visual Studio la plus récente
installée (pour en forcer une : `-G "Visual Studio 17 2022"`). Le premier
configure prend une minute (téléchargement de JUCE). Résultats :

| Cible       | Emplacement                                                         |
|-------------|---------------------------------------------------------------------|
| VST3        | `build\plugin\MusicBox2_artefacts\Release\VST3\Music Box 2.vst3`    |
| Standalone  | `build\plugin\MusicBox2_artefacts\Release\Standalone\Music Box 2.exe` |

Pour travailler dans l'IDE, ouvrez la solution `MusicBox2` générée dans `build\`, ou ouvrez directement
le dossier dans Visual Studio / CLion / VS Code (support CMake natif).

### Tests unitaires

```powershell
ctest --test-dir build -C Release --output-on-failure
```

Pour ne compiler que la bibliothèque et ses tests (rapide, sans JUCE) :

```powershell
cmake -S . -B build-core -DMBOX_BUILD_PLUGIN=OFF
cmake --build build-core --config Release
ctest --test-dir build-core -C Release
```

### Options CMake

| Option              | Défaut | Rôle                                                  |
|---------------------|--------|-------------------------------------------------------|
| `MBOX_BUILD_PLUGIN` | `ON`   | Plugin JUCE (VST3 + Standalone)                       |
| `MBOX_BUILD_TESTS`  | `ON`   | Tests unitaires de `core/`                            |
| `MBOX_BUILD_CLAP`   | `OFF`  | Version CLAP en plus, via `clap-juce-extensions`      |

Pour utiliser une copie locale de JUCE au lieu du téléchargement :
`-DFETCHCONTENT_SOURCE_DIR_JUCE=C:/chemin/vers/JUCE`.

## Utiliser dans FL Studio

1. Copiez `Music Box 2.vst3` dans `C:\Program Files\Common Files\VST3\`
   (ou ajoutez le dossier de build aux chemins de recherche de FL :
   *Options → Manage plugins → Plugin search paths*).
2. Dans *Manage plugins*, cliquez sur **Find installed plugins**.
3. Ajoutez **Music Box 2** comme instrument (*Channel rack → Add*). C'est un
   « instrument » silencieux : il reçoit les notes du piano roll ou du clavier
   MIDI et les affiche.
4. Pour envoyer sa sortie MIDI vers un autre instrument : dans la fenêtre du
   wrapper, onglet *Settings* (icône engrenage), donnez un numéro de **port de
   sortie MIDI**, puis mettez le même numéro comme **port d'entrée MIDI** dans
   les réglages du wrapper de l'instrument cible.

Astuce de développement : la version Standalone permet de tester l'interface
avec un clavier MIDI sans lancer FL Studio (*Options → Audio/MIDI settings*).

## Feuille de route (idées)

- Sélection de gamme / tonalité et quantification des notes entrantes
- Visualisations : Tonnetz, polygones d'accords sur le cercle chromatique
- Génération de progressions d'accords (sortie MIDI)
- Paramètres automatisables et sauvegarde d'état
