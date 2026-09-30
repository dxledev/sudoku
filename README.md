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

The last game resumes automatically. Start a fresh game with `./build/sudoku --difficulty expert`, or choose a difficulty in the window. Replacing an unfinished game through the UI asks for confirmation in a modal inside the window. Choose **Keep playing** or press **Escape** to cancel, or choose **New puzzle** to confirm. The timer pauses while the modal is open.

The native window can be tiled, floated, maximized, and resized by the compositor. Its minimum size is 720 × 760 logical pixels to keep the board and controls usable. Wayland is selected automatically when `WAYLAND_DISPLAY` is present; `QT_QPA_PLATFORM` can override this.

After one continuous minute without window focus, the game automatically pauses, hides the board, and stops the timer. Returning to the window resumes an automatic pause; a manual pause stays paused. Brief trips away reset the countdown when you return. Change the delay with `./build/sudoku --auto-pause-seconds 120`, or disable it with `--auto-pause-seconds 0`. The initial minute away still counts as playing time.

## Pencil notes / scratchpad

Select an empty tile and click **Notes** or press **N**. Enter several possible digits with the keypad or keyboard; small digits appear inside that tile without setting its final number. Enter the same digit again to remove that note. Press **N** again to enter a final number.

Entering a final number clears that tile's notes. A correct final number also removes that digit from the notes in its row, column, and box. Undo restores both numbers and affected notes. Erase clears a tile's number or all its notes. Fixed clues cannot be edited.

| Control | Action |
| --- | --- |
| Click / arrow keys / hjkl | Select or move between tiles (h left, j down, k up, l right) |
| 1–9 / keypad | Enter a final number or toggle a pencil note |
| N | Toggle notes mode |
| Backspace / Delete / 0 | Erase the selected tile |
| Ctrl+Z | Undo |
| Ctrl+H | Reveal one correct number |
| Space | Pause / resume; the board is hidden while paused |
| Ctrl+N | New puzzle at the current difficulty |

Mistake highlighting is available only on Easy and can be switched off. It is hidden and disabled on Medium, Hard, and Expert. The mistake counter works on every difficulty, including when highlighting is off, and also appears in the completion message. For each wrong digit in each tile, the first entry counts, the second separate attempt is forgiven, and the third and later separate attempts count again. A repeated digit becomes a separate attempt only when a different final number was entered in that tile in between. Erasing, undoing, or adding pencil notes does not reset that protection. Undo keeps the accumulated mistake count.

When highlighting is enabled, a Correct / Incorrect legend below the checkbox shows the colors used for entered digits. Correct entries use the theme's accent unless it is red, pink, or a nearby warm hue; those accents generate a complementary non-red color with similar saturation and brightness, adjusted for readability. Incorrect entries use the generated red. Both entry colors are cached in the loaded theme and update with theme changes.

Hints reveal the selected editable tile, or another unsolved tile when the selection is fixed or already correct. A solved puzzle displays an animated “Solved” overlay over the blurred grid. Undo reopens the changed tile and resumes the timer, so completing it again replays the animation.

## Local statistics

The **Stats** button beside **New puzzle** opens a modal inside the window. Close it with **Close** or **Escape**. The game timer pauses while statistics are open, and the modal follows live theme changes.

Each difficulty shows wins, average solve time, best solve time, average mistakes on wins, quits, and win rate. Times exclude pauses. Win rate is wins divided by wins plus qualifying quits; unfinished games still in progress do not affect it. Each puzzle records its first win once, including when you undo and solve it again.

A quit is recorded when you replace an unfinished puzzle after at least three minutes of play and manually enter at least one final number. The entry still qualifies after erase or undo; pencil notes and hints alone do not qualify. Closing the app keeps the puzzle available to resume and does not count as a quit. A puzzle already recorded as a win cannot later count as a quit.

Statistics are saved atomically in `stats.json` beside `game.json`, with a small set of totals for each difficulty. They stay on your device. Tracking starts with this feature; previous games have no recorded history, but a saved solved puzzle is counted once when restored.

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

See [themes/forest.json](themes/forest.json) for the complete schema. All twelve colors are independently configurable: `background`, `surface`, `surface_alt`, `text`, `muted`, `accent`, `accent_text`, `border`, `grid`, `selection`, `related`, and `matching`. Mistake highlighting automatically generates a red tone from the theme's surface and accent colors, adjusting brightness for readability across tile backgrounds. The color is cached in the loaded theme and reused during painting. It is not configurable through JSON or the CLI; legacy `error` values are ignored and omitted when saving. Direct edits to the JSON file also reload live, including editors that save by replacing the file. Invalid edits leave the running UI on its last valid palette and display an error; correct the file to resume reloads. An invalid file at startup produces a clear CLI error instead of overwriting it.

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

Moves, pencil notes, elapsed time, hint count, mistake count, repeat tracking, and the highlighting preference are saved atomically to `game.json` alongside the theme. Moves save immediately; time saves every ten seconds and on a normal close. Undo holds up to 200 moves and persists across launches, including the final move of a solved puzzle. Older solved saves without undo history can reopen one editable tile to replay completion. Closing during generation requests cancellation and joins the worker safely.

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
