#pragma once

#include "board.h"
#include "theme_watcher.h"
#include "stats_modal.h"
#include "new_puzzle_modal.h"
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

/** @brief Worker thread that generates one puzzle and reports an error if it fails. */
class PuzzleWorker : public QThread {
    Q_OBJECT
public:
    /** @brief Configure the difficulty for the generation job. */
    PuzzleWorker(Difficulty difficulty, QObject *parent);
    /** @brief Generated puzzle, available after the thread finishes successfully. */
    Puzzle result;
    /** @brief Human-readable generation failure, empty on success. */
    QString error;
protected:
    /** @brief Generate the puzzle and store either its result or an error. */
    void run() override;
private:
    Difficulty difficulty_;
};

/** @brief Main application window coordinating game state, widgets, and persistence. */
class Window : public QMainWindow {
    Q_OBJECT
public:
    /**
     * @brief Build the game window and restore or begin a puzzle.
     * @param configDirectory Directory containing game, theme, and statistics files.
     * @param theme Initial validated theme.
     * @param difficulty Optional difficulty override for the first puzzle.
     * @param parent Optional Qt parent widget.
     * @param autoPauseSeconds Delay without focus before automatic pause; zero disables it.
     */
    explicit Window(QString configDirectory, Theme theme, const QString &difficulty = {}, QWidget *parent = nullptr,
                    int autoPauseSeconds = 60);
    /** @brief Cancel and join any active puzzle generation job. */
    ~Window() override;
    /** @brief Return the current game, or null while no game is installed. */
    const Game *game() const { return game_.get(); }
    /** @brief Return the board widget. */
    Board *board() const { return board_; }
    /** @brief Return the current accepted theme. */
    const Theme &theme() const { return themeWatcher_->theme(); }
    /** @brief Return whether gameplay is currently paused. */
    bool isPaused() const { return paused_; }
    /** @brief Return elapsed gameplay seconds, excluding paused intervals. */
    qint64 elapsedSeconds() const;

public slots:
    /** @brief Enter a digit in the selected cell according to the current input mode. */
    void enterDigit(int digit);
    /** @brief Toggle the manual pause state. */
    void togglePause();
    /** @brief Toggle pencil-note entry mode. */
    void toggleNotes();
    /** @brief Restore the previous game move. */
    void undo();
    /** @brief Clear the selected cell's value or notes. */
    void erase();
    /** @brief Reveal a correct digit in the selected or next eligible cell. */
    void hint();
    /** @brief Load and display the local statistics modal. */
    void showStats();

protected:
    /** @brief Track window activation changes for automatic pause behavior. */
    bool event(QEvent *event) override;
    /** @brief Save session state and stop generation before closing. */
    void closeEvent(QCloseEvent *event) override;
    /** @brief Keep embedded modals and responsive board geometry aligned. */
    void resizeEvent(QResizeEvent *event) override;

private:
    QString configDirectory_;
    std::unique_ptr<Game> game_;
    ThemeWatcher *themeWatcher_;
    PuzzleWorker *worker_ = nullptr;
    Difficulty difficulty_ = Difficulty::Easy;
    qint64 savedSeconds_ = 0;
    QElapsedTimer clock_;
    QElapsedTimer unfocusedClock_;
    QTimer ticker_, autosave_, messageTimer_;
    QTimer focusPauseTimer_;
    int autoPauseMilliseconds_;
    bool windowFocused_ = false;
    bool automaticallyPaused_ = false;
    bool paused_ = false;
    bool pencil_ = false;
    bool loading_ = false;
    bool completing_ = false;
    Board *board_;
    QLabel *brand_, *timerLabel_, *mistakesLabel_, *remainingLabel_, *badge_, *givensLabel_, *status_, *themeLabel_;
    QProgressBar *progress_;
    QPushButton *pauseButton_, *notesButton_, *undoButton_, *eraseButton_, *hintButton_, *newButton_;
    QCheckBox *checkBox_;
    QWidget *tools_;
    QWidget *mistakeLegend_;
    StatsModal *statsModal_ = nullptr;
    NewPuzzleModal *newPuzzleModal_ = nullptr;
    std::array<QPushButton *, 4> difficultyButtons_{};
    std::array<QPushButton *, 9> digitButtons_{};
    /** @brief Create the central layouts, board, side panel, and embedded modals. */
    void buildInterface();
    /** @brief Build the brand and primary game-action header. */
    QHBoxLayout *buildHeader();
    /** @brief Create the frame and board widget used by the central layout. */
    QWidget *buildBoardCard();
    /** @brief Build the bottom status and feedback row. */
    QHBoxLayout *buildFooter();
    /** @brief Register keyboard shortcuts for common game actions. */
    void buildShortcuts();
    /** @brief Assemble the timer, difficulty picker, and game controls. */
    QWidget *buildSidebar();
    /** @brief Build the timer and pause/resume controls. */
    QWidget *buildTimerCard();
    /** @brief Build the four-tier difficulty selector. */
    QGridLayout *buildDifficultyPicker();
    /** @brief Build pencil, highlighting, and mistake controls. */
    QWidget *buildTools();
    /** @brief Build the correct and incorrect entry-color legend. */
    QWidget *buildMistakeLegend();
    /** @brief Build the on-screen digit keypad. */
    QGridLayout *buildKeypad();
    /** @brief Build undo, erase, and hint action buttons. */
    QHBoxLayout *buildActions();
    /** @brief Apply the current theme to widget palettes, icons, and styles. */
    void applyTheme();
    /** @brief Refresh game-derived labels, buttons, and board state. */
    void refresh();
    /** @brief Update the timer label from the active or frozen clock. */
    void refreshTimer();
    /** @brief Enable and synchronize mistake controls for the current tier. */
    void refreshMistakeControls();
    /** @brief Update clocks and persist after a successful player action. */
    void moved();
    /** @brief Start asynchronous generation of a puzzle at the selected tier. */
    void startPuzzle(Difficulty difficulty);
    /** @brief Prompt before replacing an unfinished game, otherwise start directly. */
    void requestPuzzle(Difficulty difficulty);
    /** @brief Return whether either embedded modal currently blocks the game. */
    bool modalVisible() const;
    /** @brief Resume the game clock after a modal closes when state allows it. */
    void resumeAfterModal();
    /** @brief Install a generated or restored game and reset its elapsed-time base. */
    void installGame(std::unique_ptr<Game> game);
    /** @brief Persist the game session and record newly eligible results. */
    void save();
    /** @brief Freeze elapsed time into savedSeconds_ and stop the running clock. */
    void freezeClock();
    /** @brief Apply manual pause state and update board, timer, and saved session. */
    void setPaused(bool paused);
    /** @brief Handle focus transitions and resume only an automatic pause. */
    void updateWindowFocus(bool focused);
    /** @brief Start or cancel the focus-loss grace timer as appropriate. */
    void refreshFocusPause();
    /** @brief Display a temporary status message, optionally styled as an error. */
    void message(const QString &text, bool error = false);
    /** @brief Record a first win or a qualifying quit for the active puzzle. */
    void recordResult(bool quitting = false);
};

}
