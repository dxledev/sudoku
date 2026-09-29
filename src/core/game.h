#pragma once

#include "puzzle.h"
#include <vector>

namespace sudoku {

struct Move {
    Grid values;
    Notes notes;
    int hints;
};

class Game {
public:
    explicit Game(Puzzle puzzle);
    const Puzzle &puzzle() const { return puzzle_; }
    const Grid &values() const { return values_; }
    const Notes &notes() const { return notes_; }
    int hints() const { return hints_; }
    int remaining() const;
    bool complete() const;
    bool hasProgress() const;
    bool editable(int index) const;
    bool wrong(int index) const;
    bool canUndo() const { return !history_.empty(); }
    bool enter(int index, int digit, bool pencil = false);
    bool erase(int index);
    bool hint(int index);
    bool undo();
    bool restore(const Grid &values, const Notes &notes, int hints);

private:
    Puzzle puzzle_;
    Grid values_;
    Notes notes_{};
    int hints_ = 0;
    std::vector<Move> history_;
    void checkpoint();
};

}
