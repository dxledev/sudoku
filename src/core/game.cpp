/**
 * @file game.cpp
 * @brief In-memory game rules, pencil notes, hints, mistakes, and undo.
 *
 * A successful state-changing operation checkpoints values, notes, and hints.
 * Correct entries also remove matching peer notes; undo restores that full
 * snapshot while the lifetime mistake total and repeat-forgiveness counters
 * remain independent of the undo history.
 */
#include "game.h"
#include <algorithm>
#include <limits>
#include <QUuid>

namespace sudoku {

Game::Game(Puzzle puzzle) : puzzle_(std::move(puzzle)), values_(puzzle_.clues), lastDigits_(puzzle_.clues),
    id_(QUuid::createUuid().toString(QUuid::WithoutBraces)) {}

void Game::restoreIdentity(QString id, bool enteredValue) {
    id_ = std::move(id);
    enteredValue_ = enteredValue;
}

int Game::remaining() const {
    return static_cast<int>(std::count(values_.begin(), values_.end(), 0));
}

bool Game::complete() const {
    return values_ == puzzle_.solution;
}

bool Game::hasProgress() const {
    return values_ != puzzle_.clues || std::any_of(notes_.begin(), notes_.end(), [](auto note) { return note != 0; });
}

bool Game::editable(int index) const {
    return index >= 0 && index < 81 && !puzzle_.clues[index];
}

bool Game::wrong(int index) const {
    return index >= 0 && index < 81 && values_[index] && values_[index] != puzzle_.solution[index];
}

void Game::checkpoint() {
    if (history_.size() == 200)
        history_.erase(history_.begin());
    history_.push_back({values_, notes_, hints_});
}

bool Game::enter(int index, int digit, bool pencil) {
    if (!editable(index) || digit < 1 || digit > 9 || complete())
        return false;
    if (pencil) {
        if (values_[index])
            return false;
        checkpoint();
        notes_[index] ^= 1u << digit;
        return true;
    }
    if (values_[index] == digit)
        return false;
    checkpoint();
    values_[index] = digit;
    enteredValue_ = true;
    notes_[index] = 0;
    if (digit == puzzle_.solution[index]) {
        for (int other = 0; other < 81; ++other) {
            if (arePeers(index, other))
                notes_[other] &= ~(1u << digit);
        }
    } else if (lastDigits_[index] != digit) {
        auto &attempts = wrongAttempts_[index * 9 + digit - 1];
        if (attempts != 1 && mistakes_ < std::numeric_limits<int>::max())
            ++mistakes_;
        attempts = std::min<int>(attempts + 1, 3);
    }
    lastDigits_[index] = digit;
    return true;
}

bool Game::erase(int index) {
    if (!editable(index) || (!values_[index] && !notes_[index]))
        return false;
    checkpoint();
    values_[index] = 0;
    notes_[index] = 0;
    return true;
}

bool Game::hint(int index) {
    if (!editable(index) || values_[index] == puzzle_.solution[index]) {
        index = -1;
        for (int cell = 0; cell < 81; ++cell) {
            if (editable(cell) && values_[cell] != puzzle_.solution[cell]) {
                index = cell;
                break;
            }
        }
    }
    const bool previouslyEntered = enteredValue_;
    if (index < 0 || !enter(index, puzzle_.solution[index]))
        return false;
    enteredValue_ = previouslyEntered;
    ++hints_;
    return true;
}

bool Game::undo() {
    if (history_.empty())
        return false;
    const auto &move = history_.back();
    values_ = move.values;
    notes_ = move.notes;
    hints_ = move.hints;
    history_.pop_back();
    return true;
}

bool Game::restore(const Grid &values, const Notes &notes, int hints) {
    if (hints < 0)
        return false;
    for (int cell = 0; cell < 81; ++cell) {
        if (values[cell] < 0 || values[cell] > 9
            || (puzzle_.clues[cell] && values[cell] != puzzle_.clues[cell])
            || (notes[cell] & ~0x3feu) || (values[cell] && notes[cell]))
            return false;
    }
    values_ = values;
    notes_ = notes;
    hints_ = hints;
    mistakes_ = 0;
    wrongAttempts_.fill(0);
    lastDigits_ = values;
    history_.clear();
    enteredValue_ = false;
    for (int cell = 0; cell < 81; ++cell)
        enteredValue_ |= editable(cell) && values_[cell] != 0;
    return true;
}

bool Game::restoreMistakes(int mistakes, const WrongAttempts &attempts) {
    if (mistakes < 0)
        return false;
    int minimum = 0;
    for (int index = 0; index < static_cast<int>(attempts.size()); ++index) {
        const int count = attempts[index];
        if (count > 3 || (count && (!editable(index / 9) || puzzle_.solution[index / 9] == index % 9 + 1)))
            return false;
        minimum += count == 3 ? 2 : count != 0;
    }
    if (mistakes < minimum)
        return false;
    mistakes_ = mistakes;
    wrongAttempts_ = attempts;
    return true;
}

bool Game::restoreHistory(std::vector<Move> history) {
    if (history.size() > 200)
        return false;
    Game trial(puzzle_);
    for (const auto &move : history) {
        if (!trial.restore(move.values, move.notes, move.hints))
            return false;
    }
    history_ = std::move(history);
    return true;
}

bool Game::restoreLastDigits(const Grid &digits) {
    for (int cell = 0; cell < 81; ++cell) {
        if (digits[cell] < 0 || digits[cell] > 9
            || (puzzle_.clues[cell] && digits[cell] != puzzle_.clues[cell]))
            return false;
    }
    lastDigits_ = digits;
    return true;
}

}
