#pragma once

#include "game.h"
#include <array>
#include <QJsonObject>

namespace sudoku {

/** @brief Aggregate result totals for one difficulty tier. */
struct DifficultyStats {
    qint64 wins = 0; ///< Number of distinct puzzles won.
    qint64 quits = 0; ///< Number of qualifying abandoned puzzles.
    qint64 totalSeconds = 0; ///< Sum of elapsed seconds across recorded wins.
    qint64 bestSeconds = 0; ///< Fastest recorded win, or zero when there are none.
    qint64 totalMistakes = 0; ///< Sum of mistakes across recorded wins.
};

/** @brief Per-difficulty result tracking with JSON persistence. */
class Statistics {
public:
    /** @brief Read totals for a difficulty tier. */
    const DifficultyStats &at(Difficulty difficulty) const;
    /** @brief Record a puzzle's first win; return false if it was already recorded. */
    bool recordWin(const Game &game, qint64 seconds);
    /** @brief Record a qualifying quit; return false if it does not qualify. */
    bool recordQuit(const Game &game, qint64 seconds);
    /** @brief Serialize statistics in the versioned JSON format. */
    QJsonObject toJson() const;
    /** @brief Parse and validate statistics from a JSON object. */
    static Statistics fromJson(const QJsonObject &data);
    /** @brief Load statistics from disk, returning empty totals when the file is absent. */
    static Statistics load(const QString &path);
    /** @brief Atomically save statistics to disk. */
    void save(const QString &path) const;

private:
    std::array<DifficultyStats, 4> levels_{};
    QString lastGame_;
    /** @brief Check whether this game's stable identity was already recorded. */
    bool alreadyRecorded(const Game &game) const;
};

}
