# Sudoku

A C++20 / Qt 6 desktop game for Wayland. Four difficulty levels, a resizable window, a flat UI with solid colors, keyboard controls, pencil notes, and live theming through the CLI.

## Build and run

Requires CMake 3.21+, a C++20 compiler, Qt 6.7+ Core/Gui/Widgets, and Qt's Wayland platform plugin. Tests also require Qt Test. On Arch these are provided by `cmake`, `ninja`, `gcc`, `qt6-base`, and `qt6-wayland`.

```bash
cd sudoku
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

Keep the game open and run theme commands in another terminal. Changes apply to the running window automatically and persist across launches. No desktop theme manager is required.

```bash
./build/sudoku theme list
./build/sudoku theme preset paper
./build/sudoku theme preset forest
./build/sudoku theme set 'accent=#c4b5fd' 'selection=#41344f'
./build/sudoku theme show
./build/sudoku theme path
```

Built-in themes: `forest`, `paper`, `slate`, and `rose`. Every change validates the complete palette and atomically saves `theme.json`. The running UI reloads its colors without resetting the board, notes, undo history, or timer. Multiple open windows using the same theme file receive the update.

By default, the file lives at `$XDG_CONFIG_HOME/sudoku/theme.json`, falling back to `~/.config/sudoku/theme.json`. Both the game and CLI accept `--config-dir /absolute/path`, or `SUDOKU_CONFIG_DIR`.

```bash
./build/sudoku theme preset rose --dry-run
./build/sudoku theme export /tmp/my-sudoku-theme.json
./build/sudoku theme import /tmp/my-sudoku-theme.json
./build/sudoku --config-dir /tmp/sudoku-preview theme preset slate
./build/sudoku --config-dir /tmp/sudoku-preview
```

`--dry-run` validates and prints the proposed JSON without writing files. `theme path`, `theme show`, and `theme list` are read-only. Quote `#RRGGBB` assignments as shown in the examples.

See [themes/forest.json](themes/forest.json) for the complete schema. All thirteen colors are independently configurable: `background`, `surface`, `surface_alt`, `text`, `muted`, `accent`, `accent_text`, `border`, `grid`, `selection`, `related`, `matching`, and `error`. Direct edits to the JSON file also reload live, including editors that save by replacing the file. Invalid edits leave the running UI on its last valid palette and display an error; correct the file to resume reloads. An invalid file at startup produces a clear CLI error instead of overwriting it.

### Follow a theme file

To keep Sudoku synchronized with a separate theme file:

```bash
./build/sudoku theme export /tmp/sudoku-theme.json
./build/sudoku theme follow file /tmp/sudoku-theme.json
```

Edit the exported file to change colors live. Sudoku watches the source, saves the resolved colors to its own `theme.json`, and reloads the UI. It also handles atomic file replacements and symlink target changes. The source file is only read; it must be different from Sudoku's own `theme.json`.

The source binding persists across launches, and Sudoku refreshes from it on startup. Missing or invalid source updates keep the running window on its last valid colors. `theme preset`, `theme set`, and `theme import` stop following the source; use `theme follow file FILE` again to resume.

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

Tests cover all four generator tiers and uniqueness, fixed clues, notes, peer-note cleanup, undo, hints, completion, session validation, CLI dry runs, import/export, palette formats, source updates, symlink changes, atomic and direct theme edits, invalid-theme recovery, pause behavior, and compact layout geometry. `ctest` uses an offscreen platform for the UI test; the second command checks interaction in a real Wayland session.

Install the executable, desktop entry, and scalable icon for your user account:

```bash
cmake --install build --prefix "$HOME/.local"
```

Keep `~/.local/bin` on your `PATH` to launch `sudoku` from a terminal or application launcher. Configure with `-DBUILD_TESTING=OFF` if Qt Test is unavailable.
