#pragma once

#include "core/theme.h"
#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

namespace sudoku {

class ThemeWatcher : public QObject {
    Q_OBJECT
public:
    ThemeWatcher(QString path, Theme initial, QObject *parent = nullptr);
    const Theme &theme() const { return theme_; }
    const QString &path() const { return path_; }

signals:
    void changed();
    void rejected(const QString &message);

private:
    QString path_;
    Theme theme_;
    QFileSystemWatcher watcher_;
    QTimer debounce_;
    void reload();
    void updateWatches();
};

}
