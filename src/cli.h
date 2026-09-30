#pragma once

#include <QStringList>

namespace sudoku {

struct Options {
    QString configDirectory;
    QString difficulty;
    QStringList arguments;
    int autoPauseSeconds = 60;
    bool dryRun = false;
    bool help = false;
    bool version = false;
};

Options parseOptions(QStringList arguments);
void printHelp();
int runThemeCommand(const Options &options);

}
