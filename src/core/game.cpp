#include "game.h"
#include <algorithm>

namespace sudoku {

Game::Game(Puzzle puzzle) : puzzle_(std::move(puzzle)), values_(puzzle_.clues) {}

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
    notes_[index] = 0;
    if (digit == puzzle_.solution[index]) {
        for (int other = 0; other < 81; ++other) {
            if (arePeers(index, other))
                notes_[other] &= ~(1u << digit);
        }
    }
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
    if (index < 0 || !enter(index, puzzle_.solution[index]))
        return false;
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
    history_.clear();
    return true;
}

}
