#pragma once

#include "board.h"
#include "theme_watcher.h"
#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QElapsedTimer>
#include <QLabel>
#include <QMainWindow>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <memory>

namespace sudoku {

class PuzzleWorker : public QThread {
    Q_OBJECT
public:
    PuzzleWorker(Difficulty difficulty, QObject *parent);
    Puzzle result;
    QString error;
protected:
    void run() override;
private:
    Difficulty difficulty_;
};

class Window : public QMainWindow {
    Q_OBJECT
public:
    explicit Window(QString configDirectory, Theme theme, const QString &difficulty = {}, QWidget *parent = nullptr);
    ~Window() override;
    const Game *game() const { return game_.get(); }
    Board *board() const { return board_; }
    const Theme &theme() const { return themeWatcher_->theme(); }
    bool isPaused() const { return paused_; }
    qint64 elapsedSeconds() const;

public slots:
    void enterDigit(int digit);
    void togglePause();
    void toggleNotes();
    void undo();
    void erase();
    void hint();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QString configDirectory_;
    std::unique_ptr<Game> game_;
    ThemeWatcher *themeWatcher_;
    PuzzleWorker *worker_ = nullptr;
    Difficulty difficulty_ = Difficulty::Easy;
    qint64 savedSeconds_ = 0;
    QElapsedTimer clock_;
    QTimer ticker_, autosave_, messageTimer_;
    bool paused_ = false;
    bool pencil_ = false;
    bool loading_ = false;
    bool completing_ = false;
    Board *board_;
    QLabel *brand_, *timerLabel_, *remainingLabel_, *badge_, *givensLabel_, *status_, *themeLabel_;
    QProgressBar *progress_;
    QPushButton *pauseButton_, *notesButton_, *undoButton_, *eraseButton_, *hintButton_, *newButton_;
    QCheckBox *checkBox_;
    QWidget *tools_;
    std::array<QPushButton *, 4> difficultyButtons_{};
    std::array<QPushButton *, 9> digitButtons_{};
    void buildInterface();
    QHBoxLayout *buildHeader();
    QWidget *buildBoardCard();
    QHBoxLayout *buildFooter();
    void buildShortcuts();
    QWidget *buildSidebar();
    QWidget *buildTimerCard();
    QGridLayout *buildDifficultyPicker();
    QWidget *buildTools();
    QGridLayout *buildKeypad();
    QHBoxLayout *buildActions();
    void applyTheme();
    void refresh();
    void refreshTimer();
    void moved();
    void startPuzzle(Difficulty difficulty);
    void requestPuzzle(Difficulty difficulty);
    void installGame(std::unique_ptr<Game> game);
    void save();
    void freezeClock();
    void message(const QString &text, bool error = false);
};

}
