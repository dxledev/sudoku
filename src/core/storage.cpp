#include "storage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <cmath>
#include <stdexcept>

namespace sudoku {
namespace {

[[noreturn]] void fail(const QString &message) {
    throw std::runtime_error(message.toStdString());
}

template<class Array>
QJsonArray jsonArray(const Array &array) {
    QJsonArray result;
    for (auto value : array)
        result.append(static_cast<int>(value));
    return result;
}

Grid readGrid(const QJsonValue &value, int maximum) {
    if (!value.isArray() || value.toArray().size() != 81)
        fail("Session arrays must have 81 cells");
    Grid result{};
    const auto array = value.toArray();
    for (int index = 0; index < 81; ++index) {
        const auto cell = array[index];
        if (!cell.isDouble() || cell.toDouble() != cell.toInt(-1)
            || cell.toInt(-1) < 0 || cell.toInt(-1) > maximum)
            fail("Invalid session cell");
        result[index] = cell.toInt();
    }
    return result;
}

}

QString defaultConfigDirectory() {
    const QString override = qEnvironmentVariable("SUDOKU_CONFIG_DIR");
    if (!override.isEmpty())
        return QDir(override).absolutePath();
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/sudoku";
}

QJsonObject readJson(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        fail(path + ": " + file.errorString());
    if (file.size() > 1024 * 1024)
        fail(path + ": JSON file exceeds 1 MiB");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        fail(path + ": expected a valid JSON object (" + error.errorString() + ")");
    return document.object();
}

void writeJson(const QString &path, const QJsonObject &data) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        fail("Cannot create " + QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        fail(path + ": " + file.errorString());
    const auto bytes = QJsonDocument(data).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.commit())
        fail(path + ": " + file.errorString());
}

void saveSession(const QString &path, const Game &game, qint64 elapsedSeconds, bool checkMistakes) {
    writeJson(path, {
        {"version", 1},
        {"difficulty", QString::fromUtf8(difficultyInfo(game.puzzle().difficulty).name.data())},
        {"clues", jsonArray(game.puzzle().clues)}, {"solution", jsonArray(game.puzzle().solution)},
        {"values", jsonArray(game.values())}, {"notes", jsonArray(game.notes())},
        {"hints", game.hints()}, {"elapsed_seconds", elapsedSeconds}, {"check_mistakes", checkMistakes},
    });
}

std::unique_ptr<Game> loadSession(const QString &path, qint64 &elapsedSeconds, bool &checkMistakes) {
    const auto data = readJson(path);
    if (data.value("version").toInt() != 1)
        fail("Unsupported session version");
    Puzzle puzzle{
        readGrid(data.value("clues"), 9), readGrid(data.value("solution"), 9),
        parseDifficulty(data.value("difficulty").toString().toStdString()),
    };
    if (!isValid(puzzle.solution, true) || !isValid(puzzle.clues) || countSolutions(puzzle.clues) != 1)
        fail("Invalid session puzzle");
    for (int index = 0; index < 81; ++index) {
        if (puzzle.clues[index] && puzzle.clues[index] != puzzle.solution[index])
            fail("Session solution does not match clues");
    }
    const auto values = readGrid(data.value("values"), 9);
    const auto noteValues = readGrid(data.value("notes"), 0x3fe);
    Notes notes{};
    for (int index = 0; index < 81; ++index)
        notes[index] = static_cast<std::uint16_t>(noteValues[index]);
    auto game = std::make_unique<Game>(puzzle);
    const auto hintValue = data.value("hints");
    if (!hintValue.isDouble() || hintValue.toDouble() != hintValue.toInt(-1)
        || !game->restore(values, notes, hintValue.toInt(-1)))
        fail("Invalid session moves");
    const double elapsed = data.value("elapsed_seconds").toDouble(-1);
    if (!std::isfinite(elapsed) || elapsed < 0 || elapsed > 315360000 || std::floor(elapsed) != elapsed)
        fail("Invalid session timer");
    elapsedSeconds = static_cast<qint64>(elapsed);
    checkMistakes = data.value("check_mistakes").toBool(true);
    return game;
}

}
