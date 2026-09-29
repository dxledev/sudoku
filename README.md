# Sudoku

A C++20 / Qt 6 desktop game for Wayland. Four difficulty levels, a flat UI with solid colors, keyboard controls, pencil notes, and themes that update in place from JSON.

## Build and run

Requires CMake 3.21+, a C++20 compiler, Qt 6.7+ Core/Gui/Widgets, and Qt's Wayland platform plugin. Tests also require Qt Test. On Arch these are provided by `cmake`, `ninja`, `gcc`, `qt6-base`, and `qt6-wayland`.

```bash
cd /home/dxle/builds/games/sudoku
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
./build/sudoku
```

The last game resumes automatically. Start a fresh game with `./build/sudoku --difficulty expert`, or choose a difficulty in the window. Replacing an unfinished game through the UI asks for confirmation.

The native window can be tiled, floated, maximized, and resized by the compositor. Its minimum size is 720 × 760 logical pixels to keep the board and controls usable. Wayland is selected automatically when `WAYLAND_DISPLAY` is present; `QT_QPA_PLATFORM` can override this.

## Pencil notes / scratchpad

Select an empty tile and click **Notes** or press **N**. Enter several possible digits with the keypad or keyboard; small digits appear inside that tile without setting its final number. Enter the same digit again to remove that note. Press **N** again to enter a final number.

Entering a final number clears that tile's notes. A correct final number also removes that digit from the notes in its row, column, and box. Undo restores both numbers and affected notes. Erase clears a tile's number or all its notes. Fixed clues cannot be edited.

| Control | Action |
| --- | --- |
| Click / arrow keys | Select or move between tiles |
| 1–9 / keypad | Enter a final number or toggle a pencil note |
| N | Toggle notes mode |
| Backspace / Delete / 0 | Erase the selected tile |
| Ctrl+Z | Undo |
| H | Reveal one correct number |
| Space | Pause / resume; the board is hidden while paused |
| Ctrl+N | New puzzle at the current difficulty |

Mistake highlighting can be switched off. Hints reveal the selected editable tile, or another unsolved tile when the selection is fixed or already correct.

## Live themes

```bash
./build/sudoku theme list
./build/sudoku theme preset paper
./build/sudoku theme preset forest
./build/sudoku theme set 'accent=#c4b5fd' 'selection=#41344f'
./build/sudoku theme show
./build/sudoku theme path
```

Built-in themes: `forest`, `paper`, `slate`, and `rose`. Every change validates the complete palette, atomically saves `theme.json`, and triggers the running window's file watcher. The UI repaints after a short 35 ms debounce without resetting the board, notes, undo history, or timer. Multiple open windows using the same theme file receive the update.

### Your static, Caelestia, and Noctalia themes

Follow the active desktop palette to switch automatically with `~/bin/theme`, shell-mode transitions, Static/Dynamic changes, and dynamic wallpaper palettes:

```bash
./build/sudoku theme follow desktop
```

This binds to `~/.config/themes/.caelestia-use/Colors.qml`. Your existing `theme-apply` coordinator switches this selector to `current`, `.dynamic/caelestia`, or `.dynamic/noctalia`. Sudoku watches both the selector and the selected palette, including atomic saves, nested symlinks, and directory replacements. A changed palette saves to Sudoku's JSON and repaints the running window; identical palettes do not trigger another repaint. When closed, Sudoku refreshes the saved colors on its next launch. No extra polling process or shell restart is needed.

To pin Sudoku to a particular provider instead:

```bash
./build/sudoku theme follow static akane
./build/sudoku theme follow static ~/.config/themes/nord/
./build/sudoku theme follow static current
./build/sudoku theme follow caelestia
./build/sudoku theme follow noctalia
```

`follow static` reads `Colors.qml` from `~/.config/themes/<name>/`, preferring it over `noctalia.json`. It accepts a directory or an explicit palette file. `current` follows the selector symlink, including changes to its target. QML colors are parsed as data: quoted hex (including Qt's `#AARRGGBB`) and numeric `Qt.rgba(...)` with fractions are supported. No QML engine is loaded. Alpha is flattened to opaque RGB for this solid-color window.

`follow caelestia` reads `~/.local/state/caelestia/shell-theme-palette.json`, including your Aether-backed `colours` format and Caelestia Material palettes. `follow noctalia` reads `~/.config/themes/.dynamic/noctalia/Colors.qml`, the dynamic application palette exported by your existing `noctalia-theme-sync` integration. These follow the last published palette of the selected provider even when that shell is inactive. XDG config/state directory overrides are respected.

To follow a different provider output, supply its path explicitly:

```bash
./build/sudoku theme follow caelestia /path/to/palette.json
./build/sudoku theme follow noctalia /path/to/Colors.qml
./build/sudoku theme follow file /path/to/native-noctalia-palette.json
```

Both Noctalia's `mPrimary`/`mSurface` schema and its native `primary`/`on_surface` schema are supported, including a `dark`/`light` wrapper (default `dark`, or its `mode` field). `theme import FILE` also accepts these palettes and `Colors.qml` as a one-time snapshot.

`theme follow desktop /absolute/path/to/application-selector` supports a different selector location. `theme follow desktop` resumes automatic desktop following after a manual preset or color override.

Following a source saves both the resolved colors and the source binding in Sudoku's own `theme.json`. While the game is running, source file edits, atomic replacements, and selector changes rederive the colors, save the new JSON, and repaint the existing UI. On restart it refreshes from the source again. Missing or invalid source updates keep the last valid palette. `theme preset`, `theme set`, and `theme import` detach source following; select `follow` again to resume it. Source files and shell configs are only read.

Background, foreground, accent, and error colors come from the provider; grid lines and selection states use solid tints of that palette. This avoids importing shell transparency or introducing gradients.

By default, the file lives at `$XDG_CONFIG_HOME/sudoku/theme.json`, falling back to `~/.config/sudoku/theme.json`. Both the game and CLI accept `--config-dir /absolute/path`, or `SUDOKU_CONFIG_DIR`.

```bash
./build/sudoku theme preset rose --dry-run
./build/sudoku theme export /tmp/my-sudoku-theme.json
./build/sudoku theme import /tmp/my-sudoku-theme.json
./build/sudoku --config-dir /tmp/sudoku-preview theme preset slate
./build/sudoku --config-dir /tmp/sudoku-preview
```

`--dry-run` validates and prints the proposed JSON without writing files. `theme path`, `theme show`, and `theme list` are read-only. Quote `#RRGGBB` assignments so the shell preserves them.

See [themes/forest.json](themes/forest.json) for the complete schema. All thirteen colors are independently configurable: `background`, `surface`, `surface_alt`, `text`, `muted`, `accent`, `accent_text`, `border`, `grid`, `selection`, `related`, `matching`, and `error`. Direct edits to the JSON file also reload live, including editors that save by replacing the file. Invalid edits leave the running UI on its last valid palette and display an error; correct the file to resume reloads. An invalid file at startup produces a clear CLI error instead of overwriting it.

## Difficulty and persistence

Every generated puzzle has exactly one solution, verified by a bounded backtracking solver. Puzzle generation runs on a short-lived worker thread so input and theme updates stay responsive.

| Difficulty | Given cells | Additional constraint |
| --- | --- | --- |
| Easy | 44–46 | Solvable using naked and hidden singles |
| Medium | 35–38 | Fewer clues |
| Hard | 29–32 | Fewer clues |
| Expert | 24–26 | Cannot be solved using singles alone |

These are clue-density tiers with the stated logical constraints, not a comprehensive human-technique rating; puzzles within a tier can vary in complexity.

Moves, pencil notes, elapsed time, hint count, and the mistake preference are saved atomically to `game.json` alongside the theme. Moves save immediately; time saves every ten seconds and on a normal close. Undo holds up to 200 moves in memory and lasts for the current application session. Closing during generation requests cancellation and joins the worker safely.

The board is one custom-painted widget, not 81 separate widgets. Game arrays have a fixed size, undo is bounded, repainting is event-driven, and the CLI uses `QCoreApplication` without opening a window. Wayland uses raster/shared-memory rendering by default; set `QT_WAYLAND_CLIENT_BUFFER_INTEGRATION` explicitly to override that choice.

## Validation and installation

```bash
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=wayland QT_WAYLAND_CLIENT_BUFFER_INTEGRATION=shm ./build/ui_tests
```

Tests cover all four generator tiers and uniqueness, fixed clues, notes, peer-note cleanup, undo, hints, completion, session validation, CLI dry runs, import/export, provider formats, source updates, selector symlink changes, atomic and direct theme edits, invalid-theme recovery, pause behavior, and compact layout geometry. `ctest` uses an offscreen platform for the UI test; the second command checks interaction in a real Wayland session.

Desktop integration for your setup installs the binary and icon, places the desktop entry at `~/.local/bin/applications/io.github.quiet_sudoku.desktop`, registers it through `~/.local/share/applications/`, appends a row to `~/.config/apps.list`, and enables desktop theme following:

```bash
bash scripts/install-desktop.sh --dry-run
bash scripts/install-desktop.sh
```

The installer preserves existing app-list rows and order, avoids duplicate registration, and backs up existing app-list/theme files before changing them. Its options expose the build directory, install prefix, config directory, app list, and theme selector. The desktop entry uses an absolute executable path, so it also works when a launcher's `PATH` omits `~/.local/bin`. Re-run the installer after rebuilding to update the installed binary.

The transition test can also exercise your real `theme-apply --activate-only` and `theme --current-only` scripts against temporary theme directories, without switching your live shell or wallpaper:

```bash
SUDOKU_DESKTOP_SCRIPTS_DIR="$HOME/bin" QT_QPA_PLATFORM=offscreen ./build/ui_tests desktopTransitions
```

For a conventional installation without personal app-list integration:

```bash
cmake --install build --prefix "$HOME/.local"
```

Keep `~/.local/bin` on your `PATH` to launch `sudoku` from a terminal or application launcher. Configure with `-DBUILD_TESTING=OFF` if Qt Test is unavailable.
