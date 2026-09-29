#pragma once

#include "game.h"
#include <QJsonObject>
#include <QString>
#include <memory>

namespace sudoku {

QString defaultConfigDirectory();
QJsonObject readJson(const QString &path);
void writeJson(const QString &path, const QJsonObject &data);
void saveSession(const QString &path, const Game &game, qint64 elapsedSeconds, bool checkMistakes);
std::unique_ptr<Game> loadSession(const QString &path, qint64 &elapsedSeconds, bool &checkMistakes);

}
