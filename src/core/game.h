#pragma once

#include "puzzle.h"
#include <vector>
#include <QString>

namespace sudoku {

struct Move {
    Grid values;
    Notes notes;
    int hints;
};

using WrongAttempts = std::array<std::uint8_t, 81 * 9>;

class Game {
public:
    explicit Game(Puzzle puzzle);
    const Puzzle &puzzle() const { return puzzle_; }
    const Grid &values() const { return values_; }
    const Notes &notes() const { return notes_; }
    int hints() const { return hints_; }
    int mistakes() const { return mistakes_; }
    const WrongAttempts &wrongAttempts() const { return wrongAttempts_; }
    const Grid &lastDigits() const { return lastDigits_; }
    const std::vector<Move> &history() const { return history_; }
    const QString &id() const { return id_; }
    bool enteredValue() const { return enteredValue_; }
    void restoreIdentity(QString id, bool enteredValue);
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
    bool restoreMistakes(int mistakes, const WrongAttempts &attempts);
    bool restoreLastDigits(const Grid &digits);
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
    void checkpoint();
};

}
