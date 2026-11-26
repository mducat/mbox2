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
│   │   ├── PitchClass.h  # classes de hauteur sur N pas par octave, ensembles, cercle des quintes
│   │   ├── Tuning.h      # tempéraments : N-TET, tables de cents, fréquences, noms de notes
│   │   ├── Scale.h       # gammes et modes, degrés, quantification
│   │   └── Chord.h       # qualités d'accords, reconnaissance d'accords
│   ├── include/mbox/instrument/
│   │   ├── Keyboard.h    # disposition d'un clavier pour n'importe quel N-TET
│   │   ├── Fretboard.h   # manche (accordages de cordes, frettes en N-TET)
│   │   └── GuitarVoicing.h # recherche de doigtés jouables d'un accord
│   ├── include/mbox/geometry/
│   │   └── TrichordSpace.h  # espace des accords de 3 notes (orbifold de Tymoczko), conduites de voix
│   ├── src/
│   └── tests/            # tests unitaires (Catch2)
├── plugin/               # Plugin JUCE qui utilise mbox::theory
│   └── Source/
│       ├── PluginProcessor.*  # MIDI + état sauvegardé avec le projet
│       ├── PluginEditor.*     # fenêtre : en-tête (tempérament, tonalité) + un onglet par widget
│       ├── state/AppState.*   # état partagé par les widgets
│       └── widgets/           # Widget.h (classe de base), WidgetRegistry.cpp (liste), widgets
└── CMakeLists.txt
```

### Tempéraments (microtonal)

Le cœur ne suppose plus 12 notes par octave. Une `PitchClass` est un pas
`0..N-1` d'une octave divisée en N (`PitchClass (6, 19)` = mi en 19-TET) ; N
vaut 12 par défaut, donc tout le code 12-TET existant reste valide. Les gammes
et accords sont définis en demi-tons et réalisés au pas le plus proche dans
n'importe quel N (`Scale (PitchClass (0, 31), scales::major)`), ou directement
dans leur propre N (`ChordQuality { "Neutral", "n", kNeutral, 24 }`).
`Tuning` dit comment sonnent les pas : division égale (`Tuning::equal (19)`) ou
table de cents (intonation juste, tempéraments historiques...). Les numéros de
note suivent la convention MIDI (`octave * N + pas`, do central = `5 * N`, soit
60 en 12-TET).

### Widget « Piano / guitare »

Affiche les notes actives sur un clavier et un manche, nomme l'accord et montre
où ses notes se trouvent ailleurs sur le manche. Un clic sur une touche ou une
case active/désactive une note (une case par corde, comme sur une vraie
guitare). Les boutons `<` `>` parcourent les doigtés jouables de l'accord
courant (ou de l'accord sélectionné dans la progression si rien ne joue). Les
notes activées à la main sont jouées sur la sortie MIDI (case « Sortie MIDI »,
12-TET seulement pour l'instant). Les deux instruments se redessinent pour le
tempérament choisi : N touches et N frettes par octave.

### Espace des accords de 3 notes (onglet « Espace des accords 3D »)

Chaque accord de 3 notes (multiensemble de classes de hauteur du N-TET courant,
364 en 12-TET) est un point de l'orbifold de Tymoczko (*The Geometry of Musical
Chords*, Science 2006), un prisme à section triangulaire :

- l'axe vertical est la transposition (la somme des notes) ; le haut est
  recollé au bas après un tiers de tour ;
- les accords augmentés (équidistants) sont sur l'axe central, les majeurs et
  mineurs tournent autour, les accords à note doublée sont sur les faces et les
  unissons sur les arêtes ;
- la distance entre deux points est la taille de la conduite de voix qui les
  relie ; les traits fins joignent les accords à un pas de distance.

Les notes actives allument tous les accords de 3 notes qu'elles contiennent
(un accord de septième en allume quatre), la progression est tracée en orange,
et un clic sur un point joue cet accord. Navigation : glisser pour tourner,
Maj/clic droit pour déplacer, molette pour zoomer, double-clic pour recentrer.
Toute la géométrie (`mbox::geometry`) est dans `core/` et testée ; elle accepte
des hauteurs fractionnaires, donc n'importe quel tempérament.

### Ajouter un widget

1. Créer `plugin/Source/widgets/MonWidget.{h,cpp}` dérivant de `mbox::Widget`
   (qui donne accès à `state`, l'`AppState` partagé, et repeint à chaque
   changement). Les nouveaux `.cpp` sont compilés automatiquement.
2. Ajouter une ligne dans la liste de `WidgetRegistry.cpp`.

`AppState` contient le tempérament courant, la tonalité, la progression
d'accords (avec un accord sélectionné) et les notes actives (MIDI + notes
activées à la main), et prévient ses `Listener` avec un masque de changements.

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
