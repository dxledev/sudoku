#include "window.h"
#include "core/storage.h"

#include <QApplication>
#include <QCloseEvent>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QShortcut>
#include <QSignalBlocker>
#include <QResizeEvent>
#include <QStyle>
#include <QVBoxLayout>
#include <algorithm>

namespace sudoku {
namespace {

QLabel *label(const QString &text, const QString &name, QWidget *parent = nullptr) {
    auto *widget = new QLabel(text, parent);
    widget->setObjectName(name);
    return widget;
}

QPushButton *button(const QString &text, const QString &name, const QString &tooltip = {}) {
    auto *widget = new QPushButton(text);
    widget->setObjectName(name);
    widget->setCursor(Qt::PointingHandCursor);
    widget->setFocusPolicy(Qt::NoFocus);
    widget->setToolTip(tooltip);
    return widget;
}

QString timeString(qint64 seconds) {
    if (seconds >= 3600)
        return QString("%1:%2:%3").arg(seconds / 3600).arg(seconds / 60 % 60, 2, 10, QChar('0')).arg(seconds % 60, 2, 10, QChar('0'));
    return QString("%1:%2").arg(seconds / 60, 2, 10, QChar('0')).arg(seconds % 60, 2, 10, QChar('0'));
}

QString completionText(const Game &game, qint64 seconds) {
    return QString("Puzzle complete · %1 · %2 hints · %3 %4. Ready for another?")
        .arg(timeString(seconds)).arg(game.hints()).arg(game.mistakes())
        .arg(game.mistakes() == 1 ? "mistake" : "mistakes");
}

QPixmap brandMark(const Theme &theme, qreal scale) {
    QPixmap result(qRound(40 * scale), qRound(40 * scale));
    result.setDevicePixelRatio(scale);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            painter.setBrush(theme.color((row + column) % 3 == 0 ? "accent" : "selection"));
            painter.drawRoundedRect(QRectF(column * 13 + 1, row * 13 + 1, 10, 10), 3, 3);
        }
    }
    return result;
}

}

PuzzleWorker::PuzzleWorker(Difficulty difficulty, QObject *parent) : QThread(parent), difficulty_(difficulty) {}

void PuzzleWorker::run() {
    try {
        std::mt19937 random(std::random_device{}());
        result = generatePuzzle(difficulty_, random, [this] { return isInterruptionRequested(); });
    } catch (const std::exception &exception) {
        error = QString::fromUtf8(exception.what());
    }
}

Window::Window(QString configDirectory, Theme theme, const QString &difficulty, QWidget *parent, int autoPauseSeconds)
    : QMainWindow(parent), configDirectory_(std::move(configDirectory)),
      themeWatcher_(new ThemeWatcher(configDirectory_ + "/theme.json", std::move(theme), this)),
      autoPauseMilliseconds_(std::clamp(autoPauseSeconds, 0, 86400) * 1000) {
    setWindowTitle("Sudoku");
    setObjectName("sudokuWindow");
    setMinimumSize(720, 760);
    resize(1040, 820);
    buildInterface();
    buildShortcuts();
    applyTheme();
    connect(themeWatcher_, &ThemeWatcher::changed, this, [this] {
        applyTheme();
        message("Theme updated · " + themeWatcher_->theme().name);
    });
    connect(themeWatcher_, &ThemeWatcher::rejected, this, [this](const QString &error) {
        message("Theme not loaded: " + error, true);
    });
    connect(&ticker_, &QTimer::timeout, this, &Window::refreshTimer);
    ticker_.start(1000);
    connect(&autosave_, &QTimer::timeout, this, &Window::save);
    autosave_.start(10000);
    messageTimer_.setSingleShot(true);
    connect(&messageTimer_, &QTimer::timeout, this, [this] { status_->setProperty("error", false); refresh(); });
    focusPauseTimer_.setSingleShot(true);
    focusPauseTimer_.setTimerType(Qt::PreciseTimer);
    connect(&focusPauseTimer_, &QTimer::timeout, this, &Window::refreshFocusPause);
    unfocusedClock_.start();
    if (QFileInfo::exists(configDirectory_ + "/game.json")) {
        try {
            bool check = true;
            auto restored = loadSession(configDirectory_ + "/game.json", savedSeconds_, check);
            checkBox_->setChecked(check);
            installGame(std::move(restored));
            if (difficulty.isEmpty()) {
                message("Welcome back. Your puzzle is right here.");
                return;
            }
        } catch (const std::exception &error) {
            message("Saved game could not be restored: " + QString::fromUtf8(error.what()), true);
        }
    }
    difficulty_ = difficulty.isEmpty() ? Difficulty::Easy : parseDifficulty(difficulty.toStdString());
    QTimer::singleShot(0, this, [this] { startPuzzle(difficulty_); });
}

Window::~Window() {
    if (worker_) {
        worker_->requestInterruption();
        worker_->wait();
    }
}

void Window::buildInterface() {
    auto *root = new QWidget;
    setCentralWidget(root);
    auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(24, 22, 24, 18);
    layout->setSpacing(14);
    layout->addLayout(buildHeader());
    auto *intro = new QHBoxLayout;
    intro->addWidget(label("One square at a time.", "heading"));
    intro->addStretch();
    intro->addWidget(label("A MOMENT OF FOCUS", "eyebrow"), 0, Qt::AlignBottom);
    layout->addLayout(intro);
    auto *content = new QHBoxLayout;
    content->setSpacing(24);
    content->addWidget(buildBoardCard(), 1);
    content->addWidget(buildSidebar());
    layout->addLayout(content, 1);
    status_ = label("Choose a cell. Find your rhythm.", "status");
    status_->setWordWrap(true);
    status_->setMinimumHeight(26);
    layout->addWidget(status_);
    layout->addLayout(buildFooter());
    statsModal_ = new StatsModal(root);
    connect(statsModal_, &StatsModal::dismissed, this, &Window::resumeAfterModal);
    newPuzzleModal_ = new NewPuzzleModal(root);
    connect(newPuzzleModal_, &NewPuzzleModal::dismissed, this, &Window::resumeAfterModal);
    connect(newPuzzleModal_, &NewPuzzleModal::confirmed, this, &Window::startPuzzle);
}

QHBoxLayout *Window::buildHeader() {
    auto *header = new QHBoxLayout;
    header->setSpacing(14);
    brand_ = new QLabel;
    brand_->setFixedSize(40, 40);
    header->addWidget(brand_);
    auto *wordmark = new QVBoxLayout;
    wordmark->setSpacing(2);
    wordmark->addWidget(label("sudoku", "wordmark"));
    wordmark->addWidget(label("Make a little space.", "muted"));
    header->addLayout(wordmark);
    header->addStretch();
    auto *stats = button("Stats", "statsButton", "Your local statistics");
    stats->setAccessibleName("Open statistics");
    connect(stats, &QPushButton::clicked, this, &Window::showStats);
    header->addWidget(stats);
    newButton_ = button("＋  New puzzle", "primary", "Start a new puzzle · Ctrl+N");
    newButton_->setMinimumWidth(142);
    connect(newButton_, &QPushButton::clicked, this, [this] { requestPuzzle(difficulty_); });
    header->addWidget(newButton_);
    return header;
}

QWidget *Window::buildBoardCard() {
    auto *boardCard = new QFrame;
    boardCard->setObjectName("boardCard");
    auto *boardLayout = new QVBoxLayout(boardCard);
    boardLayout->setContentsMargins(18, 16, 18, 16);
    boardLayout->setSpacing(12);
    auto *boardHeader = new QHBoxLayout;
    badge_ = label("EASY", "badge");
    boardHeader->addWidget(badge_);
    boardHeader->addStretch();
    givensLabel_ = label("A unique solution", "muted");
    boardHeader->addWidget(givensLabel_);
    boardLayout->addLayout(boardHeader);
    board_ = new Board;
    boardLayout->addWidget(board_, 1);
    connect(board_, &Board::digitRequested, this, &Window::enterDigit);
    connect(board_, &Board::eraseRequested, this, &Window::erase);
    connect(board_, &Board::selectionChanged, this, &Window::refresh);
    auto *boardFooter = new QHBoxLayout;
    remainingLabel_ = label("Finding a fresh puzzle…", "muted");
    boardFooter->addWidget(remainingLabel_);
    boardFooter->addStretch();
    progress_ = new QProgressBar;
    progress_->setRange(0, 81);
    progress_->setTextVisible(false);
    progress_->setFixedSize(90, 4);
    boardFooter->addWidget(progress_);
    boardLayout->addLayout(boardFooter);
    return boardCard;
}

QHBoxLayout *Window::buildFooter() {
    auto *footer = new QHBoxLayout;
    footer->addWidget(label("1–9  numbers     N  notes     ⌫  erase     Ctrl+Z  undo", "shortcut"));
    footer->addStretch();
    themeLabel_ = label("●  forest", "shortcut");
    themeLabel_->setToolTip("Colors update live from " + themeWatcher_->path());
    footer->addWidget(themeLabel_);
    return footer;
}

QWidget *Window::buildSidebar() {
    auto *sidebar = new QWidget;
    sidebar->setFixedWidth(244);
    auto *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(buildTimerCard());
    layout->addWidget(label("DIFFICULTY", "eyebrow"));
    layout->addLayout(buildDifficultyPicker());
    layout->addWidget(buildTools(), 1);
    return sidebar;
}

QWidget *Window::buildTimerCard() {
    auto *timeCard = new QFrame;
    timeCard->setObjectName("timeCard");
    auto *timeLayout = new QVBoxLayout(timeCard);
    timeLayout->setContentsMargins(18, 12, 18, 12);
    timeLayout->setSpacing(6);
    timeLayout->addWidget(label("YOUR TIME", "eyebrow"));
    auto *timerRow = new QHBoxLayout;
    timerLabel_ = label("00:00", "timer");
    timerLabel_->setAccessibleName("Elapsed time");
    timerRow->addWidget(timerLabel_, 1);
    pauseButton_ = button("Ⅱ", "pause", "Pause or resume · Space");
    pauseButton_->setAccessibleName("Pause game");
    pauseButton_->setFixedSize(42, 38);
    connect(pauseButton_, &QPushButton::clicked, this, &Window::togglePause);
    timerRow->addWidget(pauseButton_);
    timeLayout->addLayout(timerRow);
    mistakesLabel_ = label("Mistakes: 0", "mistakeCount");
    mistakesLabel_->setAccessibleName("Mistake count");
    timeLayout->addWidget(mistakesLabel_);
    return timeCard;
}

QGridLayout *Window::buildDifficultyPicker() {
    auto *difficultyGrid = new QGridLayout;
    difficultyGrid->setSpacing(7);
    for (int index = 0; index < 4; ++index) {
        const auto &info = difficulties[index];
        QString name = QString::fromUtf8(info.name.data());
        name[0] = name[0].toUpper();
        auto *item = button(name, "difficulty");
        item->setCheckable(true);
        item->setMinimumHeight(36);
        item->setAccessibleName(name + " difficulty");
        connect(item, &QPushButton::clicked, this, [this, level = info.value] { requestPuzzle(level); refresh(); });
        difficultyButtons_[index] = item;
        difficultyGrid->addWidget(item, index / 2, index % 2);
    }
    return difficultyGrid;
}

QWidget *Window::buildTools() {
    tools_ = new QWidget;
    auto *toolsLayout = new QVBoxLayout(tools_);
    toolsLayout->setContentsMargins(0, 0, 0, 0);
    toolsLayout->setSpacing(8);
    auto *inputHeader = new QHBoxLayout;
    inputHeader->addWidget(label("YOUR NEXT MOVE", "eyebrow"));
    inputHeader->addStretch();
    toolsLayout->addLayout(inputHeader);
    toolsLayout->addLayout(buildKeypad(), 1);
    notesButton_ = button("✎  Notes", "notes", "Toggle candidate notes · N");
    notesButton_->setAccessibleName("Pencil notes");
    notesButton_->setCheckable(true);
    notesButton_->setMinimumHeight(34);
    connect(notesButton_, &QPushButton::clicked, this, &Window::toggleNotes);
    toolsLayout->addWidget(notesButton_);
    toolsLayout->addLayout(buildActions());
    checkBox_ = new QCheckBox("Highlight mistakes");
    checkBox_->setObjectName("checkMistakes");
    checkBox_->setChecked(true);
    checkBox_->setFocusPolicy(Qt::NoFocus);
    connect(checkBox_, &QCheckBox::toggled, this, [this] {
        refreshMistakeControls();
        save();
    });
    toolsLayout->addWidget(checkBox_);
    mistakeLegend_ = buildMistakeLegend();
    toolsLayout->addWidget(mistakeLegend_);
    auto *tip = label("Use notes to keep your options open.", "tip");
    tip->setWordWrap(true);
    toolsLayout->addWidget(tip);
    return tools_;
}

QWidget *Window::buildMistakeLegend() {
    auto *legend = new QWidget;
    legend->setObjectName("mistakeLegend");
    auto *layout = new QHBoxLayout(legend);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    layout->addWidget(label("●  Correct", "correctLegend"));
    layout->addWidget(label("●  Incorrect", "incorrectLegend"));
    layout->addStretch();
    return legend;
}

QGridLayout *Window::buildKeypad() {
    auto *digits = new QGridLayout;
    digits->setSpacing(7);
    for (int digit = 1; digit <= 9; ++digit) {
        auto *item = button(QString::number(digit), "digit");
        item->setMinimumHeight(42);
        item->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        item->setAccessibleName(QString("Enter %1").arg(digit));
        connect(item, &QPushButton::clicked, this, [this, digit] { enterDigit(digit); board_->setFocus(); });
        digitButtons_[digit - 1] = item;
        digits->addWidget(item, (digit - 1) / 3, (digit - 1) % 3);
    }
    return digits;
}

QHBoxLayout *Window::buildActions() {
    auto *actions = new QHBoxLayout;
    actions->setSpacing(7);
    undoButton_ = button("↶  Undo", "tool", "Undo the last move · Ctrl+Z");
    undoButton_->setAccessibleName("Undo last move");
    eraseButton_ = button("⌫  Erase", "tool", "Clear this cell · Backspace");
    hintButton_ = button("◇  Hint", "tool", "Reveal one correct number · Ctrl+H");
    connect(undoButton_, &QPushButton::clicked, this, &Window::undo);
    connect(eraseButton_, &QPushButton::clicked, this, &Window::erase);
    connect(hintButton_, &QPushButton::clicked, this, &Window::hint);
    actions->addWidget(undoButton_);
    actions->addWidget(eraseButton_);
    actions->addWidget(hintButton_);
    return actions;
}

void Window::buildShortcuts() {
    const auto shortcut = [this](const QKeySequence &sequence, auto action) {
        auto *key = new QShortcut(sequence, this);
        connect(key, &QShortcut::activated, this, action);
    };
    shortcut(QKeySequence(Qt::Key_N), &Window::toggleNotes);
    shortcut(QKeySequence(Qt::Key_Space), &Window::togglePause);
    shortcut(QKeySequence::Undo, &Window::undo);
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_H), &Window::hint);
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_N), [this] { requestPuzzle(difficulty_); });
}

void Window::applyTheme() {
    const auto &theme = themeWatcher_->theme();
    QString style = QString::fromUtf8(R"(
        QMainWindow, QWidget { background: %1; color: %4; font-family: "Adwaita Sans"; font-size: 13px; }
        QLabel { background: transparent; }
        QLabel#wordmark { font-size: 23px; font-weight: 600; }
        QLabel#heading { font-size: 29px; font-weight: 500; }
        QLabel#muted, QLabel#tip, QLabel#shortcut, QLabel#mistakeCount { color: %5; }
        QFrame#newPuzzleCard QLabel#muted { font-size: 19.5px; }
        QLabel#tip { font-size: 12px; }
        QLabel#correctLegend { color: %correct; font-size: 12px; }
        QLabel#incorrectLegend { color: %mistake; font-size: 12px; }
        QLabel#shortcut { font-size: 11px; }
        QLabel#eyebrow { color: %5; font-size: 10px; font-weight: 600; letter-spacing: 1.5px; }
        QLabel#badge { color: %6; background: %10; border-radius: 5px; padding: 5px 9px; font-size: 10px; font-weight: 600; letter-spacing: 1px; }
        QLabel#timer { font-size: 34px; font-weight: 500; }
        QLabel#status { color: %5; font-size: 12px; }
        QLabel#status[error="true"] { color: %mistake; }
        QFrame#boardCard, QFrame#timeCard, QFrame#statsCard, QFrame#newPuzzleCard { background: %2; border: 1px solid %8; border-radius: 15px; }
        QFrame#boardCard QWidget, QFrame#timeCard QWidget, QFrame#statsCard QLabel, QFrame#newPuzzleCard QLabel { background: transparent; }
        QLabel#statsHeading { color: %5; font-size: 11px; }
        QLabel#statsDifficulty { font-weight: 600; }
        QPushButton { border: 1px solid %8; border-radius: 8px; background: %2; color: %4; padding: 10px 8px; }
        QPushButton:hover { background: %3; border-color: %9; }
        QPushButton:pressed { background: %10; }
        QPushButton:disabled { color: %5; border-color: %8; }
        QPushButton#primary, QPushButton#startNewPuzzle { background: %6; color: %7; border: none; font-weight: 600; padding: 12px 18px; }
        QPushButton#primary:hover, QPushButton#startNewPuzzle:hover { background: %12; color: %4; }
        QPushButton#difficulty { padding: 8px; font-size: 12px; }
        QPushButton#difficulty:checked { background: %10; color: %6; border-color: %6; }
        QPushButton#digit { font-size: 23px; font-weight: 500; padding: 5px; }
        QPushButton#notes:checked { color: %6; background: %10; border-color: %6; }
        QPushButton#tool { padding: 8px 4px; font-size: 11px; }
        QPushButton#pause { font-size: 16px; padding: 4px; background: %3; }
        QCheckBox { color: %5; spacing: 9px; background: transparent; font-size: 12px; }
        QCheckBox::indicator { width: 14px; height: 14px; border-radius: 4px; border: 1px solid %9; background: %2; }
        QCheckBox::indicator:checked { background: %6; border-color: %6; }
        QProgressBar { background: %8; border: none; border-radius: 2px; }
        QProgressBar::chunk { background: %6; border-radius: 2px; }
        QToolTip, QLabel#boardTooltip { background: %3; color: %4; border: 1px solid %8; padding: 7px; }
    )");
    style.replace("%mistake", theme.mistakeColor.name());
    style.replace("%correct", theme.correctColor.name());
    const auto keys = colorKeys();
    for (qsizetype index = keys.size(); index > 0; --index)
        style.replace("%" + QString::number(index), theme.hex(keys[index - 1]));
    setStyleSheet(style);
    QPalette palette;
    palette.setColor(QPalette::Window, theme.color("background"));
    palette.setColor(QPalette::WindowText, theme.color("text"));
    palette.setColor(QPalette::Base, theme.color("surface"));
    palette.setColor(QPalette::Text, theme.color("text"));
    palette.setColor(QPalette::Button, theme.color("surface"));
    palette.setColor(QPalette::ButtonText, theme.color("text"));
    palette.setColor(QPalette::Highlight, theme.color("accent"));
    palette.setColor(QPalette::HighlightedText, theme.color("accent_text"));
    setPalette(palette);
    board_->setTheme(theme);
    statsModal_->setTheme(theme);
    newPuzzleModal_->setTheme(theme);
    brand_->setPixmap(brandMark(theme, devicePixelRatioF()));
    themeLabel_->setText("●  " + theme.name);
    status_->style()->unpolish(status_);
    status_->style()->polish(status_);
}

qint64 Window::elapsedSeconds() const {
    return savedSeconds_ + (clock_.isValid() ? clock_.elapsed() / 1000 : 0);
}

void Window::freezeClock() {
    savedSeconds_ = elapsedSeconds();
    clock_.invalidate();
}

void Window::refreshTimer() {
    timerLabel_->setText(timeString(elapsedSeconds()));
}

void Window::refreshMistakeControls() {
    const bool easy = difficulty_ == Difficulty::Easy;
    checkBox_->setVisible(easy);
    checkBox_->setEnabled(easy && !paused_ && !loading_);
    if (!easy) {
        const QSignalBlocker blocker(checkBox_);
        checkBox_->setChecked(false);
    }
    mistakeLegend_->setVisible(easy && checkBox_->isChecked());
    board_->setCheckMistakes(easy && checkBox_->isChecked());
}

void Window::refresh() {
    const bool active = game_ && !paused_ && !loading_;
    const bool editable = active && game_->editable(board_->selected()) && !game_->complete();
    for (int index = 0; index < 4; ++index) {
        difficultyButtons_[index]->setChecked(difficulties[index].value == difficulty_);
        difficultyButtons_[index]->setEnabled(!loading_);
    }
    for (int index = 0; index < 9; ++index) {
        auto *item = digitButtons_[index];
        item->setEnabled(editable && (!pencil_ || !game_->values()[board_->selected()]));
        item->setToolTip(QString(pencil_ ? "Toggle note %1" : "Enter %1").arg(index + 1));
    }
    newButton_->setEnabled(!loading_);
    pauseButton_->setEnabled(game_ && !loading_ && !game_->complete());
    pauseButton_->setText(paused_ ? "▶" : "Ⅱ");
    pauseButton_->setAccessibleName(paused_ ? "Resume game" : "Pause game");
    notesButton_->setChecked(pencil_);
    notesButton_->setEnabled(active && !game_->complete());
    undoButton_->setEnabled(active && game_->canUndo());
    eraseButton_->setEnabled(editable && (game_->values()[board_->selected()] || game_->notes()[board_->selected()]));
    hintButton_->setEnabled(active && !game_->complete());
    refreshMistakeControls();
    mistakesLabel_->setText(QString("Mistakes: %1").arg(game_ ? game_->mistakes() : 0));
    badge_->setText(QString::fromUtf8(difficultyInfo(difficulty_).name.data()).toUpper());
    if (game_) {
        const int givens = 81 - static_cast<int>(std::count(game_->puzzle().clues.begin(), game_->puzzle().clues.end(), 0));
        givensLabel_->setText(QString("%1 givens · one solution").arg(givens));
        remainingLabel_->setText(game_->complete() ? "All 81 cells. Beautifully done." : QString("%1 cells to discover").arg(game_->remaining()));
        progress_->setValue(81 - game_->remaining());
    }
    if (!messageTimer_.isActive()) {
        status_->setText(loading_ ? "Making sure your puzzle has exactly one solution…"
            : automaticallyPaused_ ? "Paused while you were away. Return to this window to continue."
            : paused_ ? "No rush. Your game is paused."
            : game_ && game_->complete() ? completionText(*game_, savedSeconds_)
            : pencil_ ? "Notes on · toggle candidate numbers with 1–9. Press N for a final number."
            : "Choose a cell. Find your rhythm.");
    }
    board_->refreshCompletion();
    board_->update();
    refreshTimer();
}

void Window::installGame(std::unique_ptr<Game> game) {
    game_ = std::move(game);
    difficulty_ = game_->puzzle().difficulty;
    loading_ = false;
    paused_ = false;
    automaticallyPaused_ = false;
    completing_ = game_->complete();
    board_->setLoading(false);
    board_->setPaused(false);
    refreshMistakeControls();
    board_->setGame(game_.get());
    if (!game_->complete() && !modalVisible())
        clock_.start();
    refreshFocusPause();
    recordResult();
    refresh();
    if (!modalVisible())
        board_->setFocus();
}

void Window::requestPuzzle(Difficulty difficulty) {
    if (loading_ || modalVisible())
        return;
    if (game_ && !game_->complete() && (game_->hasProgress() || elapsedSeconds() > 0)) {
        freezeClock();
        newPuzzleModal_->setGeometry(centralWidget()->rect());
        newPuzzleModal_->showConfirmation(difficulty);
        refreshTimer();
        return;
    }
    startPuzzle(difficulty);
}

bool Window::modalVisible() const {
    return statsModal_->isVisible() || newPuzzleModal_->isVisible();
}

void Window::resumeAfterModal() {
    if (game_ && !paused_ && !loading_ && !game_->complete())
        clock_.start();
    refreshTimer();
    board_->setFocus();
}

void Window::startPuzzle(Difficulty difficulty) {
    if (worker_)
        return;
    freezeClock();
    loading_ = true;
    difficulty_ = difficulty;
    board_->setLoading(true);
    refresh();
    worker_ = new PuzzleWorker(difficulty, this);
    connect(worker_, &QThread::finished, this, [this] {
        auto *completed = worker_;
        worker_ = nullptr;
        loading_ = false;
        if (completed->error.isEmpty()) {
            recordResult(true);
            savedSeconds_ = 0;
            pencil_ = false;
            board_->setPencil(false);
            messageTimer_.stop();
            installGame(std::make_unique<Game>(completed->result));
            save();
        } else {
            board_->setLoading(false);
            if (game_) {
                difficulty_ = game_->puzzle().difficulty;
                if (!paused_ && !game_->complete() && !modalVisible())
                    clock_.start();
            }
            message(completed->error, true);
            refreshFocusPause();
            refresh();
        }
        completed->deleteLater();
    });
    worker_->start();
}

void Window::enterDigit(int digit) {
    if (!modalVisible() && !paused_ && !loading_ && game_ && game_->enter(board_->selected(), digit, pencil_))
        moved();
}

void Window::erase() {
    if (!modalVisible() && !paused_ && !loading_ && game_ && game_->erase(board_->selected()))
        moved();
}

void Window::undo() {
    if (modalVisible() || paused_ || loading_ || !game_)
        return;
    const auto previous = game_->values();
    if (!game_->undo())
        return;
    for (int cell = 0; cell < 81; ++cell) {
        if (previous[cell] != game_->values()[cell]) {
            board_->selectCell(cell);
            break;
        }
    }
    moved();
    board_->setFocus();
}

void Window::hint() {
    if (!modalVisible() && !paused_ && !loading_ && game_ && game_->hint(board_->selected())) {
        moved();
        message("One number revealed. You've got the rest.");
    }
}

void Window::toggleNotes() {
    if (modalVisible() || paused_ || loading_ || !game_ || game_->complete())
        return;
    pencil_ = !pencil_;
    board_->setPencil(pencil_);
    messageTimer_.stop();
    refresh();
    board_->setFocus();
}

void Window::togglePause() {
    if (modalVisible() || loading_ || !game_ || game_->complete())
        return;
    automaticallyPaused_ = false;
    setPaused(!paused_);
    refreshFocusPause();
    board_->setFocus();
}

void Window::setPaused(bool paused) {
    if (paused_ == paused)
        return;
    if (paused)
        freezeClock();
    else if (game_ && !loading_ && !game_->complete() && !modalVisible())
        clock_.start();
    paused_ = paused;
    board_->setPaused(paused_);
    messageTimer_.stop();
    refresh();
    save();
}

void Window::updateWindowFocus(bool focused) {
    windowFocused_ = focused;
    if (focused) {
        focusPauseTimer_.stop();
        unfocusedClock_.invalidate();
        if (automaticallyPaused_) {
            automaticallyPaused_ = false;
            setPaused(false);
        }
    } else {
        if (!unfocusedClock_.isValid())
            unfocusedClock_.start();
        refreshFocusPause();
    }
}

void Window::refreshFocusPause() {
    focusPauseTimer_.stop();
    if (windowFocused_ || autoPauseMilliseconds_ == 0 || !game_ || loading_ || paused_ || game_->complete())
        return;
    const qint64 remaining = autoPauseMilliseconds_ - unfocusedClock_.elapsed();
    if (remaining > 0) {
        focusPauseTimer_.start(static_cast<int>(remaining));
        return;
    }
    automaticallyPaused_ = true;
    setPaused(true);
}

void Window::moved() {
    if (game_->complete()) {
        freezeClock();
        completing_ = true;
    } else if (completing_) {
        completing_ = false;
        clock_.start();
    }
    refreshFocusPause();
    messageTimer_.stop();
    refresh();
    save();
}

void Window::save() {
    if (!game_ || loading_)
        return;
    try {
        recordResult();
        saveSession(configDirectory_ + "/game.json", *game_, elapsedSeconds(), checkBox_->isChecked());
    } catch (const std::exception &error) {
        message("Could not save game: " + QString::fromUtf8(error.what()), true);
    }
}

void Window::recordResult(bool quitting) {
    if (!game_ || (!quitting && !game_->complete()))
        return;
    try {
        const auto path = configDirectory_ + "/stats.json";
        auto statistics = Statistics::load(path);
        const bool changed = quitting ? statistics.recordQuit(*game_, elapsedSeconds())
                                      : statistics.recordWin(*game_, elapsedSeconds());
        if (changed)
            statistics.save(path);
    } catch (const std::exception &error) {
        message("Could not save statistics: " + QString::fromUtf8(error.what()), true);
    }
}

void Window::showStats() {
    if (modalVisible())
        return;
    try {
        const auto statistics = Statistics::load(configDirectory_ + "/stats.json");
        freezeClock();
        statsModal_->setGeometry(centralWidget()->rect());
        statsModal_->showStats(statistics);
        refreshTimer();
    } catch (const std::exception &error) {
        message("Could not load statistics: " + QString::fromUtf8(error.what()), true);
    }
}

void Window::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (statsModal_)
        statsModal_->setGeometry(centralWidget()->rect());
    if (newPuzzleModal_)
        newPuzzleModal_->setGeometry(centralWidget()->rect());
}

bool Window::event(QEvent *event) {
    const bool handled = QMainWindow::event(event);
    if (event->type() == QEvent::WindowActivate)
        updateWindowFocus(true);
    else if (event->type() == QEvent::WindowDeactivate)
        updateWindowFocus(false);
    return handled;
}

void Window::message(const QString &text, bool error) {
    status_->setText(text);
    status_->setProperty("error", error);
    status_->style()->unpolish(status_);
    status_->style()->polish(status_);
    messageTimer_.start(error ? 12000 : 4000);
}

void Window::closeEvent(QCloseEvent *event) {
    save();
    if (worker_)
        worker_->requestInterruption();
    QMainWindow::closeEvent(event);
}

}
