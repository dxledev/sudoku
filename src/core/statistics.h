#pragma once

#include "game.h"
#include <array>
#include <QJsonObject>

namespace sudoku {

struct DifficultyStats {
    qint64 wins = 0;
    qint64 quits = 0;
    qint64 totalSeconds = 0;
    qint64 bestSeconds = 0;
    qint64 totalMistakes = 0;
};

class Statistics {
public:
    const DifficultyStats &at(Difficulty difficulty) const;
    bool recordWin(const Game &game, qint64 seconds);
    bool recordQuit(const Game &game, qint64 seconds);
    QJsonObject toJson() const;
    static Statistics fromJson(const QJsonObject &data);
    static Statistics load(const QString &path);
    void save(const QString &path) const;

private:
    std::array<DifficultyStats, 4> levels_{};
    QString lastGame_;
    bool alreadyRecorded(const Game &game) const;
};

}
