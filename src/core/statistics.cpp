/**
 * @file statistics.cpp
 * @brief Per-difficulty win and qualifying-quit accounting.
 *
 * Wins are identified by stable puzzle identity so undoing a completed puzzle
 * cannot record the same result twice. Average and best times are derived from
 * accumulated totals; JSON parsing rejects malformed or inconsistent values.
 */
#include "statistics.h"
#include "storage.h"

#include <QFileInfo>
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace sudoku {
namespace {

constexpr qint64 maximum = 1000000000000;

qint64 readCount(const QJsonObject &data, const QString &key) {
    const auto value = data.value(key);
    const double number = value.toDouble(-1);
    if (!value.isDouble() || number < 0 || number > maximum || std::floor(number) != number)
        throw std::runtime_error("Invalid statistics: " + key.toStdString());
    return static_cast<qint64>(number);
}

qint64 add(qint64 value, qint64 increment) {
    return std::min(maximum, value + std::clamp(increment, qint64(0), maximum));
}

}

const DifficultyStats &Statistics::at(Difficulty difficulty) const {
    return levels_.at(static_cast<std::size_t>(difficulty));
}

bool Statistics::alreadyRecorded(const Game &game) const {
    return lastGame_ == game.id();
}

bool Statistics::recordWin(const Game &game, qint64 seconds) {
    if (!game.complete() || alreadyRecorded(game) || seconds < 0 || seconds > maximum)
        return false;
    auto &level = levels_.at(static_cast<std::size_t>(game.puzzle().difficulty));
    level.bestSeconds = level.wins ? std::min(level.bestSeconds, seconds) : seconds;
    level.wins = add(level.wins, 1);
    level.totalSeconds = add(level.totalSeconds, seconds);
    level.totalMistakes = add(level.totalMistakes, game.mistakes());
    lastGame_ = game.id();
    return true;
}

bool Statistics::recordQuit(const Game &game, qint64 seconds) {
    if (game.complete() || alreadyRecorded(game) || seconds < 180 || !game.enteredValue())
        return false;
    auto &level = levels_.at(static_cast<std::size_t>(game.puzzle().difficulty));
    level.quits = add(level.quits, 1);
    lastGame_ = game.id();
    return true;
}

QJsonObject Statistics::toJson() const {
    QJsonObject levels;
    for (const auto &info : difficulties) {
        const auto &level = at(info.value);
        levels.insert(QString::fromUtf8(info.name.data()), QJsonObject{
            {"wins", level.wins}, {"quits", level.quits}, {"total_seconds", level.totalSeconds},
            {"best_seconds", level.bestSeconds}, {"total_mistakes", level.totalMistakes}});
    }
    return {{"version", 1}, {"last_game", lastGame_}, {"difficulties", levels}};
}

Statistics Statistics::fromJson(const QJsonObject &data) {
    if (data.value("version").toInt() != 1 || !data.value("difficulties").isObject()
        || !data.value("last_game").isString() || data.value("last_game").toString().size() > 128)
        throw std::runtime_error("Invalid statistics file");
    Statistics result;
    result.lastGame_ = data.value("last_game").toString();
    const auto levels = data.value("difficulties").toObject();
    for (const auto &info : difficulties) {
        const auto entry = levels.value(QString::fromUtf8(info.name.data()));
        if (!entry.isObject())
            throw std::runtime_error("Missing difficulty statistics");
        const auto object = entry.toObject();
        auto &level = result.levels_[static_cast<std::size_t>(info.value)];
        level = {readCount(object, "wins"), readCount(object, "quits"), readCount(object, "total_seconds"),
                 readCount(object, "best_seconds"), readCount(object, "total_mistakes")};
        if ((!level.wins && (level.totalSeconds || level.bestSeconds || level.totalMistakes))
            || level.bestSeconds > level.totalSeconds)
            throw std::runtime_error("Inconsistent statistics file");
    }
    return result;
}

Statistics Statistics::load(const QString &path) {
    return QFileInfo::exists(path) ? fromJson(readJson(path)) : Statistics{};
}

void Statistics::save(const QString &path) const {
    writeJson(path, toJson());
}

}
