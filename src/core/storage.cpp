/**
 * @file storage.cpp
 * @brief Atomic JSON persistence for game sessions and shared JSON files.
 *
 * Session loading validates puzzle uniqueness, cell ranges, notes, mistakes,
 * undo snapshots, identity, and elapsed time before exposing restored state.
 * Older session formats missing newer optional fields are reconstructed where
 * possible, including an undo step for replaying completion.
 */
#include "storage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QCryptographicHash>
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

template<class Array>
Array readArray(const QJsonValue &value, int maximum) {
    Array result{};
    if (!value.isArray() || value.toArray().size() != static_cast<int>(result.size()))
        fail("Invalid session array size");
    const auto array = value.toArray();
    for (int index = 0; index < static_cast<int>(result.size()); ++index) {
        const auto cell = array[index];
        if (!cell.isDouble() || cell.toDouble() != cell.toInt(-1)
            || cell.toInt(-1) < 0 || cell.toInt(-1) > maximum)
            fail("Invalid session cell");
        result[index] = cell.toInt();
    }
    return result;
}

Move readMove(const QJsonObject &data) {
    const auto hint = data.value("hints");
    if (!hint.isDouble() || hint.toDouble() != hint.toInt(-1) || hint.toInt(-1) < 0)
        fail("Invalid session hint count");
    return {readArray<Grid>(data.value("values"), 9),
            readArray<Notes>(data.value("notes"), 0x3fe), hint.toInt()};
}

QJsonObject moveJson(const Move &move) {
    return {{"values", jsonArray(move.values)}, {"notes", jsonArray(move.notes)}, {"hints", move.hints}};
}

void restoreHistory(Game &game, const QJsonObject &data) {
    std::vector<Move> history;
    if (data.contains("history")) {
        const auto saved = data.value("history");
        if (!saved.isArray() || saved.toArray().size() > 200)
            fail("Invalid session undo history");
        for (const auto &entry : saved.toArray()) {
            if (!entry.isObject())
                fail("Invalid session undo move");
            history.push_back(readMove(entry.toObject()));
        }
    } else if (game.complete()) {
        // Older sessions discarded undo; reopen one editable cell to allow replaying completion.
        auto values = game.values();
        for (int cell = 80; cell >= 0; --cell) {
            if (game.editable(cell)) {
                values[cell] = 0;
                history.push_back({values, {}, game.hints()});
                break;
            }
        }
    }
    if (!game.restoreHistory(std::move(history)))
        fail("Invalid session undo history");
}

void restoreMistakes(Game &game, const QJsonObject &data) {
    if (!data.contains("mistakes") && !data.contains("wrong_attempts"))
        return;
    const auto mistakes = data.value("mistakes");
    if (!mistakes.isDouble() || mistakes.toDouble() != mistakes.toInt(-1)
        || !game.restoreMistakes(mistakes.toInt(-1), readArray<WrongAttempts>(data.value("wrong_attempts"), 3)))
        fail("Invalid session mistake count");
}

void restoreLastDigits(Game &game, const QJsonObject &data) {
    auto digits = game.values();
    if (data.contains("last_digits")) {
        digits = readArray<Grid>(data.value("last_digits"), 9);
    } else {
        for (const auto &move : game.history()) {
            for (int cell = 0; cell < 81; ++cell) {
                if (!game.values()[cell] && move.values[cell])
                    digits[cell] = move.values[cell];
            }
        }
    }
    if (!game.restoreLastDigits(digits))
        fail("Invalid session last digits");
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

void writeJson(const QString &path, const QJsonObject &data, QJsonDocument::JsonFormat format) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        fail("Cannot create " + QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        fail(path + ": " + file.errorString());
    const auto bytes = QJsonDocument(data).toJson(format);
    if (file.write(bytes) != bytes.size() || !file.commit())
        fail(path + ": " + file.errorString());
}

void saveSession(const QString &path, const Game &game, qint64 elapsedSeconds, bool checkMistakes) {
    QJsonArray history;
    for (const auto &move : game.history())
        history.append(moveJson(move));
    writeJson(path, {
        {"version", 1},
        {"id", game.id()}, {"entered_value", game.enteredValue()},
        {"difficulty", QString::fromUtf8(difficultyInfo(game.puzzle().difficulty).name.data())},
        {"clues", jsonArray(game.puzzle().clues)}, {"solution", jsonArray(game.puzzle().solution)},
        {"values", jsonArray(game.values())}, {"notes", jsonArray(game.notes())},
        {"hints", game.hints()}, {"elapsed_seconds", elapsedSeconds},
        {"check_mistakes", checkMistakes && game.puzzle().difficulty == Difficulty::Easy},
        {"mistakes", game.mistakes()}, {"wrong_attempts", jsonArray(game.wrongAttempts())},
        {"last_digits", jsonArray(game.lastDigits())},
        {"history", history},
    }, QJsonDocument::Compact);
}

std::unique_ptr<Game> loadSession(const QString &path, qint64 &elapsedSeconds, bool &checkMistakes) {
    const auto data = readJson(path);
    if (data.value("version").toInt() != 1)
        fail("Unsupported session version");
    Puzzle puzzle{
        readArray<Grid>(data.value("clues"), 9), readArray<Grid>(data.value("solution"), 9),
        parseDifficulty(data.value("difficulty").toString().toStdString()),
    };
    if (!isValid(puzzle.solution, true) || !isValid(puzzle.clues) || countSolutions(puzzle.clues) != 1)
        fail("Invalid session puzzle");
    for (int index = 0; index < 81; ++index) {
        if (puzzle.clues[index] && puzzle.clues[index] != puzzle.solution[index])
            fail("Session solution does not match clues");
    }
    const auto move = readMove(data);
    auto game = std::make_unique<Game>(puzzle);
    if (!game->restore(move.values, move.notes, move.hints))
        fail("Invalid session moves");
    restoreMistakes(*game, data);
    restoreHistory(*game, data);
    restoreLastDigits(*game, data);
    QString id = data.value("id").toString();
    if (data.contains("id") && (id.isEmpty() || id.size() > 128))
        fail("Invalid session identity");
    if (data.contains("entered_value") && !data.value("entered_value").isBool())
        fail("Invalid session input flag");
    if (id.isEmpty()) {
        const QJsonObject identity{{"clues", data.value("clues")}, {"solution", data.value("solution")},
                                   {"difficulty", data.value("difficulty")}};
        id = QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(identity).toJson(QJsonDocument::Compact),
                                                       QCryptographicHash::Sha256).toHex());
    }
    bool entered = game->enteredValue();
    for (int cell = 0; cell < 81; ++cell)
        entered |= game->editable(cell) && game->lastDigits()[cell] != 0;
    game->restoreIdentity(id, data.value("entered_value").toBool(entered));
    const double elapsed = data.value("elapsed_seconds").toDouble(-1);
    if (!std::isfinite(elapsed) || elapsed < 0 || elapsed > 315360000 || std::floor(elapsed) != elapsed)
        fail("Invalid session timer");
    elapsedSeconds = static_cast<qint64>(elapsed);
    checkMistakes = puzzle.difficulty == Difficulty::Easy && data.value("check_mistakes").toBool(true);
    return game;
}

}
