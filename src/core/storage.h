#pragma once

#include "game.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QString>
#include <memory>

namespace sudoku {

/** @brief Resolve the configured user directory, honoring SUDOKU_CONFIG_DIR. */
QString defaultConfigDirectory();
/** @brief Read a JSON object from disk and throw on I/O, size, or format errors. */
QJsonObject readJson(const QString &path);
/** @brief Atomically write a JSON object, creating parent directories as needed. */
void writeJson(const QString &path, const QJsonObject &data, QJsonDocument::JsonFormat format = QJsonDocument::Indented);
/** @brief Persist a game session and its timer and mistake-highlighting preference. */
void saveSession(const QString &path, const Game &game, qint64 elapsedSeconds, bool checkMistakes);
/**
 * @brief Load and validate a saved session.
 * @param path Session JSON path.
 * @param elapsedSeconds Receives the saved elapsed time.
 * @param checkMistakes Receives the saved Easy-mode highlighting preference.
 * @return Restored game state; throws when the file is invalid or unreadable.
 */
std::unique_ptr<Game> loadSession(const QString &path, qint64 &elapsedSeconds, bool &checkMistakes);

}
