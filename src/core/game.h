#pragma once

#include "puzzle.h"
#include <vector>
#include <QString>

namespace sudoku {

/** @brief Snapshot of the mutable part of a game, used as an undo checkpoint. */
struct Move {
    Grid values; ///< Player values at this checkpoint.
    Notes notes; ///< Pencil notes at this checkpoint.
    int hints; ///< Cumulative hints at this checkpoint.
};

/** @brief Per cell-and-digit counters used to forgive every second repeated error. */
using WrongAttempts = std::array<std::uint8_t, 81 * 9>;

/** @brief Mutable game state and operations, independent of the user interface. */
class Game {
public:
    /** @brief Create a fresh game from a puzzle, copying its clues into current values. */
    explicit Game(Puzzle puzzle);
    /** @brief Access the immutable puzzle definition. */
    const Puzzle &puzzle() const { return puzzle_; }
    /** @brief Read the current values, including fixed clues. */
    const Grid &values() const { return values_; }
    /** @brief Read the current per-cell pencil notes. */
    const Notes &notes() const { return notes_; }
    /** @brief Number of hints used in this game. */
    int hints() const { return hints_; }
    /** @brief Number of mistakes charged so far. */
    int mistakes() const { return mistakes_; }
    /** @brief Read per-cell-and-digit wrong-entry repeat counters. */
    const WrongAttempts &wrongAttempts() const { return wrongAttempts_; }
    /** @brief Read the last manually entered digit for each cell. */
    const Grid &lastDigits() const { return lastDigits_; }
    /** @brief Read undo checkpoints, oldest first. */
    const std::vector<Move> &history() const { return history_; }
    /** @brief Stable identity used to avoid recording the same puzzle result twice. */
    const QString &id() const { return id_; }
    /** @brief Whether the player has manually entered a final value in an editable cell. */
    bool enteredValue() const { return enteredValue_; }
    /** @brief Restore the stable puzzle identity and manual-entry marker. */
    void restoreIdentity(QString id, bool enteredValue);
    /** @brief Count cells that do not yet contain their solution digit. */
    int remaining() const;
    /** @brief Return true when every cell matches the solution. */
    bool complete() const;
    /** @brief Return true when the player has entered values or used hints. */
    bool hasProgress() const;
    /** @brief Return whether a cell is editable rather than a fixed clue. */
    bool editable(int index) const;
    /** @brief Return whether the current value in a cell differs from the solution. */
    bool wrong(int index) const;
    /** @brief Return whether an undo checkpoint is available. */
    bool canUndo() const { return !history_.empty(); }
    /** @brief Enter a digit or toggle it as a pencil note; reject invalid cells or digits. */
    bool enter(int index, int digit, bool pencil = false);
    /** @brief Clear a cell's value or notes, preserving fixed clues. */
    bool erase(int index);
    /** @brief Reveal the correct digit in an editable cell. */
    bool hint(int index);
    /** @brief Restore the latest checkpoint, if one exists. */
    bool undo();
    /** @brief Restore values, notes, and hints after validating their ranges and invariants. */
    bool restore(const Grid &values, const Notes &notes, int hints);
    /** @brief Restore mistake counters after checking their consistency. */
    bool restoreMistakes(int mistakes, const WrongAttempts &attempts);
    /** @brief Restore last-entered digits after validating clues and digit ranges. */
    bool restoreLastDigits(const Grid &digits);
    /** @brief Restore at most 200 undo checkpoints after replay validation. */
    bool restoreHistory(std::vector<Move> history);

private:
    Puzzle puzzle_;
    Grid values_;
    Notes notes_{};
    int hints_ = 0;
    int mistakes_ = 0;
    WrongAttempts wrongAttempts_{};
    Grid lastDigits_{};
    std::vector<Move> history_;
    QString id_;
    bool enteredValue_ = false;
    /** @brief Store the current undoable state, keeping history within its limit. */
    void checkpoint();
};

}
