#include "ui/window.h"
#include "ui/solved_overlay.h"
#include "core/storage.h"
#include "core/theme_source.h"

#include <QFile>
#include <QHelpEvent>
#include <QEnterEvent>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QProcess>
#include <QSaveFile>
#include <cstdio>
#include <QTemporaryDir>
#include <QtTest>
#include <unistd.h>

using namespace sudoku;

class UiTests : public QObject {
    Q_OBJECT
private slots:
    void automaticPauseFocusTransitions() {
        QTemporaryDir directory;
        std::mt19937 random(91);
        Game game(generatePuzzle(Difficulty::Easy, random));
        saveSession(directory.filePath("game.json"), game, 120, true);
        Window window(directory.path(), presetTheme("forest"), {}, nullptr, 1);
        window.show();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        QTest::qWait(1100);
        QVERIFY(!window.isPaused());
        QVERIFY(window.elapsedSeconds() > 120);

        QWidget otherWindow;
        otherWindow.show();
        otherWindow.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&otherWindow));
        QTest::qWait(350);
        QVERIFY(!window.isPaused());
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        QTest::qWait(800);
        QVERIFY(!window.isPaused());

        otherWindow.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&otherWindow));
        QTest::qWait(500);
        QVERIFY(!window.isPaused());
        QTRY_VERIFY_WITH_TIMEOUT(window.isPaused(), 1500);
        const auto seconds = window.elapsedSeconds();
        const auto values = window.game()->values();
        const auto notes = window.game()->notes();
        window.enterDigit(1);
        window.toggleNotes();
        window.erase();
        window.undo();
        window.hint();
        QVERIFY(window.game()->values() == values);
        QVERIFY(window.game()->notes() == notes);
        QCOMPARE(window.game()->hints(), 0);
        QCOMPARE(window.findChild<QPushButton *>("pause")->accessibleName(), QString("Resume game"));
        for (auto *digit : window.findChildren<QPushButton *>("digit"))
            QVERIFY(!digit->isEnabled());
        QTest::qWait(1100);
        QCOMPARE(window.elapsedSeconds(), seconds);
        qint64 savedSeconds = 0;
        bool checkMistakes = false;
        loadSession(directory.filePath("game.json"), savedSeconds, checkMistakes);
        QCOMPARE(savedSeconds, seconds);

        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        QVERIFY(!window.isPaused());
        QTRY_VERIFY(window.elapsedSeconds() > seconds);
        window.close();
    }

    void automaticPausePreservesManualPause_data() {
        QTest::addColumn<int>("delaySeconds");
        QTest::addColumn<bool>("manualPause");
        QTest::newRow("manual") << 1 << true;
        QTest::newRow("disabled") << 0 << false;
    }

    void automaticPausePreservesManualPause() {
        QFETCH(int, delaySeconds);
        QFETCH(bool, manualPause);
        QTemporaryDir directory;
        std::mt19937 random(91);
        Game game(generatePuzzle(Difficulty::Easy, random));
        saveSession(directory.filePath("game.json"), game, 120, true);
        Window window(directory.path(), presetTheme("forest"), {}, nullptr, delaySeconds);
        window.show();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        if (manualPause)
            window.togglePause();
        QWidget otherWindow;
        otherWindow.show();
        otherWindow.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&otherWindow));
        QTest::qWait(1200);
        QCOMPARE(window.isPaused(), manualPause);
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        QCOMPARE(window.isPaused(), manualPause);
        if (manualPause) {
            QCOMPARE(window.elapsedSeconds(), qint64(120));
            window.togglePause();
            QVERIFY(!window.isPaused());
        } else {
            QVERIFY(window.elapsedSeconds() > 120);
        }
        window.close();
    }

    void automaticPauseWithStatisticsOpen() {
        QTemporaryDir directory;
        std::mt19937 random(91);
        Game game(generatePuzzle(Difficulty::Easy, random));
        saveSession(directory.filePath("game.json"), game, 120, true);
        Window window(directory.path(), presetTheme("forest"), {}, nullptr, 1);
        window.show();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        window.showStats();
        const auto seconds = window.elapsedSeconds();
        auto *modal = window.findChild<StatsModal *>("statsModal");
        QWidget otherWindow;
        otherWindow.show();
        otherWindow.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&otherWindow));
        QTRY_VERIFY_WITH_TIMEOUT(window.isPaused(), 1500);
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        QVERIFY(!window.isPaused());
        QVERIFY(modal->isVisible());
        QTest::qWait(1100);
        QCOMPARE(window.elapsedSeconds(), seconds);
        QTest::mouseClick(modal->findChild<QPushButton *>("closeStats"), Qt::LeftButton);
        QVERIFY(modal->isHidden());
        QTRY_VERIFY(window.elapsedSeconds() > seconds);
        window.close();
    }

    void statisticsModal() {
        QTemporaryDir directory;
        std::mt19937 random(91);
        Game game(generatePuzzle(Difficulty::Easy, random));
        const int cell = static_cast<int>(std::distance(game.values().begin(), std::find(game.values().begin(), game.values().end(), 0)));
        auto values = game.puzzle().solution;
        values[cell] = 0;
        QVERIFY(game.restore(values, {}, 0));
        saveSession(directory.filePath("game.json"), game, 240, true);
        const auto themePath = directory.filePath("theme.json");
        writeJson(themePath, presetTheme("forest").toJson());
        Window window(directory.path(), presetTheme("forest"));
        window.show();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        window.enterDigit(game.puzzle().solution[cell] % 9 + 1);
        window.enterDigit(game.puzzle().solution[cell]);
        QVERIFY(window.game()->complete());
        QCOMPARE(Statistics::load(directory.filePath("stats.json")).at(Difficulty::Easy).wins, 1);
        auto *modal = window.findChild<StatsModal *>("statsModal");
        auto *stats = window.findChild<QPushButton *>("statsButton");
        QVERIFY(modal);
        QVERIFY(stats);
        QTest::mouseClick(stats, Qt::LeftButton);
        QVERIFY(modal->isVisible());
        QCOMPARE(modal->geometry(), window.centralWidget()->rect());
        QVERIFY(std::abs(modal->findChild<QWidget *>("statsCard")->geometry().center().x() - modal->rect().center().x()) <= 1);
        QCOMPARE(modal->findChild<QLabel *>("stats_0_0")->text(), QString("1"));
        QCOMPARE(modal->findChild<QLabel *>("stats_0_1")->text(), QString("04:00"));
        QCOMPARE(modal->findChild<QLabel *>("stats_0_3")->text(), QString("1.0"));
        QCOMPARE(modal->findChild<QLabel *>("stats_0_5")->text(), QString("100%"));
        QCOMPARE(modal->findChild<QLabel *>("stats_1_1")->text(), QString("—"));
        window.undo();
        QVERIFY(window.game()->complete());
        QTest::keyClick(modal, Qt::Key_N, Qt::ControlModifier);
        QVERIFY(window.game()->complete());
        QTest::keyClick(modal, Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), modal->findChild<QPushButton *>("closeStats"));
        const auto screenshots = qEnvironmentVariable("SUDOKU_TEST_SCREENSHOTS");
        if (!screenshots.isEmpty()) {
            QDir().mkpath(screenshots);
            QVERIFY(window.grab().save(screenshots + "/stats-forest.png"));
        }
        writeJson(themePath, presetTheme("paper").toJson());
        QTRY_COMPARE(window.theme().name, QString("paper"));
        QVERIFY(modal->isVisible());
        window.resize(720, 760);
        QTest::qWait(80);
        QCOMPARE(modal->geometry(), window.centralWidget()->rect());
        auto *card = modal->findChild<QWidget *>("statsCard");
        QVERIFY(modal->rect().contains(card->geometry()));
        QVERIFY(std::abs(card->geometry().center().x() - modal->rect().center().x()) <= 1);
        for (auto *label : card->findChildren<QLabel *>()) {
            QVERIFY(card->rect().contains(QRect(label->mapTo(card, QPoint()), label->size())));
            QVERIFY(label->height() >= label->fontMetrics().height());
        }
        if (!screenshots.isEmpty())
            QVERIFY(window.grab().save(screenshots + "/stats-paper-compact.png"));
        QTest::keyClick(modal->findChild<QPushButton *>("closeStats"), Qt::Key_Escape);
        QVERIFY(modal->isHidden());
        window.undo();
        QVERIFY(!window.game()->complete());
        window.showStats();
        const auto seconds = window.elapsedSeconds();
        QTest::qWait(1100);
        QCOMPARE(window.elapsedSeconds(), seconds);
        window.enterDigit(game.puzzle().solution[cell]);
        QVERIFY(!window.game()->complete());
        QTest::mouseClick(modal->findChild<QPushButton *>("closeStats"), Qt::LeftButton);
        QVERIFY(modal->isHidden());
        QTRY_VERIFY(window.elapsedSeconds() > seconds);
        window.enterDigit(game.puzzle().solution[cell]);
        QVERIFY(window.game()->complete());
        QCOMPARE(Statistics::load(directory.filePath("stats.json")).at(Difficulty::Easy).wins, 1);
        window.close();
        Window restored(directory.path(), presetTheme("forest"));
        restored.showStats();
        QCOMPARE(restored.findChild<QLabel *>("stats_0_0")->text(), QString("1"));
        restored.close();
    }

    void statisticsConfirmedReplacement() {
        QTemporaryDir directory;
        std::mt19937 random(91);
        Game game(generatePuzzle(Difficulty::Easy, random));
        const int cell = static_cast<int>(std::distance(game.values().begin(), std::find(game.values().begin(), game.values().end(), 0)));
        QVERIFY(game.enter(cell, game.puzzle().solution[cell] % 9 + 1));
        saveSession(directory.filePath("game.json"), game, 180, true);
        const auto themePath = directory.filePath("theme.json");
        writeJson(themePath, presetTheme("forest").toJson());
        Window window(directory.path(), presetTheme("forest"));
        window.show();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        const auto originalId = window.game()->id();
        auto *modal = window.findChild<NewPuzzleModal *>("newPuzzleModal");
        QVERIFY(modal);
        auto *keep = modal->findChild<QPushButton *>("keepPlaying");
        auto *start = modal->findChild<QPushButton *>("startNewPuzzle");
        QVERIFY(keep);
        QVERIFY(start);
        QTest::mouseClick(window.findChild<QPushButton *>("primary"), Qt::LeftButton);
        QVERIFY(modal->isVisible());
        QVERIFY(!modal->isWindow());
        QCOMPARE(modal->window(), &window);
        QVERIFY(!QApplication::activeModalWidget());
        QCOMPARE(modal->geometry(), window.centralWidget()->rect());
        QCOMPARE(QApplication::focusWidget(), keep);
        QTest::keyClick(keep, Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), start);
        QTest::keyClick(start, Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), keep);
        QTest::keyClick(keep, Qt::Key_Tab, Qt::ShiftModifier);
        QCOMPARE(QApplication::focusWidget(), start);
        const auto seconds = window.elapsedSeconds();
        QTest::qWait(1100);
        QCOMPARE(window.elapsedSeconds(), seconds);
        const auto values = window.game()->values();
        const auto notes = window.game()->notes();
        window.enterDigit(game.puzzle().solution[cell]);
        window.erase();
        window.undo();
        window.hint();
        window.toggleNotes();
        QTest::keyClick(start, Qt::Key_Space);
        QTest::keyClick(start, Qt::Key_N, Qt::ControlModifier);
        window.showStats();
        QVERIFY(window.game()->values() == values);
        QVERIFY(window.game()->notes() == notes);
        QCOMPARE(window.game()->hints(), 0);
        QVERIFY(!window.isPaused());
        QVERIFY(window.findChild<StatsModal *>("statsModal")->isHidden());
        QVERIFY(modal->isVisible());
        writeJson(themePath, presetTheme("paper").toJson());
        QTRY_COMPARE(window.theme().name, QString("paper"));
        window.resize(720, 760);
        QTest::qWait(80);
        QCOMPARE(modal->geometry(), window.centralWidget()->rect());
        auto *card = modal->findChild<QWidget *>("newPuzzleCard");
        QVERIFY(modal->rect().contains(card->geometry()));
        QVERIFY(std::abs(card->geometry().center().x() - modal->rect().center().x()) <= 1);
        QVERIFY(std::abs(card->geometry().center().y() - modal->rect().center().y()) <= 1);
        const auto screenshots = qEnvironmentVariable("SUDOKU_TEST_SCREENSHOTS");
        if (!screenshots.isEmpty()) {
            QDir().mkpath(screenshots);
            QVERIFY(window.grab().save(screenshots + "/new-puzzle-paper-compact.png"));
        }
        QTest::mouseClick(keep, Qt::LeftButton);
        QVERIFY(modal->isHidden());
        QCOMPARE(QApplication::focusWidget(), window.board());
        QCOMPARE(window.game()->id(), originalId);
        QVERIFY(!QFileInfo::exists(directory.filePath("stats.json")));
        QTRY_VERIFY(window.elapsedSeconds() > seconds);
        QTest::keyClick(window.board(), Qt::Key_N, Qt::ControlModifier);
        QVERIFY(modal->isVisible());
        QTest::keyClick(keep, Qt::Key_Return);
        QVERIFY(modal->isHidden());
        QCOMPARE(window.game()->id(), originalId);
        window.togglePause();
        QVERIFY(window.isPaused());
        QTest::keyClick(window.board(), Qt::Key_N, Qt::ControlModifier);
        QVERIFY(modal->isVisible());
        QTest::keyClick(keep, Qt::Key_Escape);
        QVERIFY(modal->isHidden());
        QVERIFY(window.isPaused());
        const auto pausedSeconds = window.elapsedSeconds();
        QTest::qWait(1100);
        QCOMPARE(window.elapsedSeconds(), pausedSeconds);
        window.togglePause();
        QPushButton *medium = nullptr;
        for (auto *button : window.findChildren<QPushButton *>("difficulty")) {
            if (button->text() == "Medium")
                medium = button;
        }
        QVERIFY(medium);
        QTest::mouseClick(medium, Qt::LeftButton);
        QVERIFY(modal->isVisible());
        QVERIFY(!medium->isChecked());
        QTest::keyClick(keep, Qt::Key_Escape);
        QCOMPARE(window.game()->puzzle().difficulty, Difficulty::Easy);
        QCOMPARE(window.game()->id(), originalId);
        QTest::mouseClick(medium, Qt::LeftButton);
        QTest::keyClick(keep, Qt::Key_Tab);
        QTest::keyClick(start, Qt::Key_Return);
        QVERIFY(modal->isHidden());
        QTRY_VERIFY_WITH_TIMEOUT(window.game()->id() != originalId, 15000);
        QCOMPARE(window.game()->puzzle().difficulty, Difficulty::Medium);
        QCOMPARE(Statistics::load(directory.filePath("stats.json")).at(Difficulty::Easy).quits, 1);
        window.close();
    }

    void statisticsAbandonment() {
        QTemporaryDir directory;
        std::mt19937 random(91);
        Game game(generatePuzzle(Difficulty::Hard, random));
        const int cell = static_cast<int>(std::distance(game.values().begin(), std::find(game.values().begin(), game.values().end(), 0)));
        QVERIFY(game.enter(cell, game.puzzle().solution[cell] % 9 + 1));
        QVERIFY(game.undo());
        saveSession(directory.filePath("game.json"), game, 180, false);
        {
            Window window(directory.path(), presetTheme("forest"));
            window.show();
            QVERIFY(QTest::qWaitForWindowActive(&window));
            window.close();
            QVERIFY(!QFileInfo::exists(directory.filePath("stats.json")));
        }
        {
            Window window(directory.path(), presetTheme("forest"), "medium");
            window.show();
            QVERIFY(QTest::qWaitForWindowActive(&window));
            QTRY_VERIFY_WITH_TIMEOUT(window.game() && window.game()->puzzle().difficulty == Difficulty::Medium, 15000);
            QCOMPARE(Statistics::load(directory.filePath("stats.json")).at(Difficulty::Hard).quits, 1);
            QCOMPARE(Statistics::load(directory.filePath("stats.json")).at(Difficulty::Medium).quits, 0);
            window.close();
        }
        Window restored(directory.path(), presetTheme("forest"));
        restored.close();
        QCOMPARE(Statistics::load(directory.filePath("stats.json")).at(Difficulty::Hard).quits, 1);
    }

    void mistakeControls_data() {
        QTest::addColumn<int>("level");
        QTest::addColumn<bool>("resume");
        for (int level = 0; level < 4; ++level) {
            QTest::newRow(difficulties[level].name.data()) << level << false;
            QTest::newRow((QString::fromUtf8(difficulties[level].name.data()) + "-resumed").toUtf8().constData()) << level << true;
        }
    }

    void mistakeControls() {
        QFETCH(int, level);
        QFETCH(bool, resume);
        QTemporaryDir directory;
        const auto difficulty = static_cast<Difficulty>(level);
        const auto path = directory.filePath("game.json");
        if (resume) {
            std::mt19937 random(91);
            Game saved(generatePuzzle(difficulty, random));
            saveSession(path, saved, 0, true);
            auto data = readJson(path);
            data.insert("check_mistakes", true);
            writeJson(path, data);
        }
        Window window(directory.path(), presetTheme("forest"), resume ? QString() : QString::fromUtf8(difficulties[level].name.data()));
        window.show();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        QTRY_VERIFY_WITH_TIMEOUT(window.game() != nullptr, 15000);
        auto *check = window.findChild<QCheckBox *>("checkMistakes");
        auto *counter = window.findChild<QLabel *>("mistakeCount");
        auto *legend = window.findChild<QWidget *>("mistakeLegend");
        auto *correctLegend = window.findChild<QLabel *>("correctLegend");
        auto *incorrectLegend = window.findChild<QLabel *>("incorrectLegend");
        QVERIFY(check);
        QVERIFY(counter);
        QVERIFY(legend);
        QVERIFY(correctLegend);
        QVERIFY(incorrectLegend);
        QCOMPARE(check->isVisible(), difficulty == Difficulty::Easy);
        QCOMPARE(legend->isVisible(), difficulty == Difficulty::Easy);
        QCOMPARE(correctLegend->text(), QString("●  Correct"));
        QCOMPARE(incorrectLegend->text(), QString("●  Incorrect"));
        QCOMPARE(correctLegend->palette().color(QPalette::WindowText), window.theme().correctColor);
        QCOMPARE(incorrectLegend->palette().color(QPalette::WindowText), window.theme().mistakeColor);
        QCOMPARE(check->isChecked(), difficulty == Difficulty::Easy);
        QCOMPARE(counter->text(), QString("Mistakes: 0"));
        const int cell = window.board()->selected();
        const int wrong = window.game()->puzzle().solution[cell] % 9 + 1;
        window.enterDigit(wrong);
        QCOMPARE(counter->text(), QString("Mistakes: 1"));
        const auto tileContainsColor = [&](const QColor &color) {
            const auto image = window.board()->grab().toImage();
            const auto grid = window.board()->boardRect();
            const qreal side = grid.width() / 9;
            const QRectF tile(grid.x() + cell % 9 * side + side * .2,
                              grid.y() + cell / 9 * side + side * .2, side * .6, side * .6);
            const auto pixels = QRectF(tile.topLeft() * image.devicePixelRatio(), tile.size() * image.devicePixelRatio()).toRect();
            for (int y = pixels.top(); y <= pixels.bottom(); ++y) {
                for (int x = pixels.left(); x <= pixels.right(); ++x) {
                    if (image.pixelColor(x, y) == color)
                        return true;
                }
            }
            return false;
        };
        QCOMPARE(tileContainsColor(window.theme().mistakeColor), difficulty == Difficulty::Easy);
        const auto screenshots = qEnvironmentVariable("SUDOKU_TEST_SCREENSHOTS");
        if (!screenshots.isEmpty() && !resume) {
            QDir().mkpath(screenshots);
            QVERIFY(window.grab().save(screenshots + "/mistakes-" + QString::fromUtf8(difficulties[level].name.data()) + ".png"));
        }
        if (difficulty == Difficulty::Easy) {
            for (const auto &name : {"paper", "rose"}) {
                const auto previousColor = window.theme().mistakeColor;
                writeJson(directory.filePath("theme.json"), presetTheme(name).toJson());
                QTRY_COMPARE(window.theme().name, QString(name));
                QVERIFY(window.theme().mistakeColor != previousColor);
                QVERIFY(tileContainsColor(window.theme().mistakeColor));
                QVERIFY(!tileContainsColor(previousColor));
                QCOMPARE(correctLegend->palette().color(QPalette::WindowText), window.theme().correctColor);
                QCOMPARE(incorrectLegend->palette().color(QPalette::WindowText), window.theme().mistakeColor);
            }
            QVERIFY(window.theme().correctColor != window.theme().color("accent"));
        }
        window.enterDigit(wrong);
        QCOMPARE(counter->text(), QString("Mistakes: 1"));
        window.erase();
        window.enterDigit(wrong);
        QCOMPARE(counter->text(), QString("Mistakes: 1"));
        window.erase();
        window.enterDigit(wrong);
        QCOMPARE(counter->text(), QString("Mistakes: 1"));
        window.enterDigit(window.game()->puzzle().solution[cell]);
        QVERIFY(tileContainsColor(correctLegend->palette().color(QPalette::WindowText)));
        if (!screenshots.isEmpty() && difficulty == Difficulty::Easy && !resume)
            QVERIFY(window.grab().save(screenshots + "/correct-rose.png"));
        window.enterDigit(wrong);
        QCOMPARE(counter->text(), QString("Mistakes: 1"));
        window.enterDigit(wrong % 9 + 1);
        QCOMPARE(counter->text(), QString("Mistakes: 2"));
        window.enterDigit(wrong);
        QCOMPARE(counter->text(), QString("Mistakes: 3"));
        window.undo();
        QCOMPARE(counter->text(), QString("Mistakes: 3"));
        if (difficulty == Difficulty::Easy) {
            QTest::mouseClick(check, Qt::LeftButton);
            QVERIFY(!check->isChecked());
            QVERIFY(!legend->isVisible());
            QTest::mouseClick(check, Qt::LeftButton);
            QVERIFY(legend->isVisible());
            QTest::mouseClick(check, Qt::LeftButton);
            QVERIFY(!legend->isVisible());
            window.enterDigit(wrong);
            QCOMPARE(counter->text(), QString("Mistakes: 3"));
            window.enterDigit(window.game()->puzzle().solution[cell]);
            window.enterDigit(wrong);
            QCOMPARE(counter->text(), QString("Mistakes: 4"));
        }
        window.close();
        QCOMPARE(readJson(path).value("check_mistakes").toBool(), false);
        Window restored(directory.path(), presetTheme("forest"));
        restored.show();
        QVERIFY(QTest::qWaitForWindowActive(&restored));
        QCOMPARE(restored.game()->mistakes(), window.game()->mistakes());
        QCOMPARE(restored.findChild<QCheckBox *>("checkMistakes")->isVisible(), difficulty == Difficulty::Easy);
        QVERIFY(!restored.findChild<QWidget *>("mistakeLegend")->isVisible());
        restored.close();
    }

    void difficultyChangesHideHighlighting() {
        QTemporaryDir directory;
        std::mt19937 random(91);
        Game game(generatePuzzle(Difficulty::Easy, random));
        auto values = game.puzzle().solution;
        const int cell = static_cast<int>(std::distance(game.values().begin(), std::find(game.values().begin(), game.values().end(), 0)));
        values[cell] = 0;
        QVERIFY(game.restore(values, {}, 0));
        saveSession(directory.filePath("game.json"), game, 0, true);
        Window window(directory.path(), presetTheme("forest"));
        window.show();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        auto *check = window.findChild<QCheckBox *>("checkMistakes");
        QVERIFY(check->isVisible());
        QVERIFY(check->isChecked());
        window.enterDigit(game.puzzle().solution[cell]);
        const auto selectDifficulty = [&window](const QString &name) {
            for (auto *button : window.findChildren<QPushButton *>("difficulty")) {
                if (button->text() == name) {
                    QTest::mouseClick(button, Qt::LeftButton);
                    return true;
                }
            }
            return false;
        };
        QVERIFY(selectDifficulty("Medium"));
        QVERIFY(check->isHidden());
        QVERIFY(!check->isChecked());
        QTRY_VERIFY_WITH_TIMEOUT(window.game()->puzzle().difficulty == Difficulty::Medium, 15000);
        for (int index = 0; index < 81; ++index) {
            if (window.game()->editable(index))
                window.hint();
        }
        QVERIFY(window.game()->complete());
        QVERIFY(selectDifficulty("Easy"));
        QTRY_VERIFY_WITH_TIMEOUT(window.game()->puzzle().difficulty == Difficulty::Easy, 15000);
        QVERIFY(check->isVisible());
        QVERIFY(!check->isChecked());
        QCOMPARE(window.findChild<QLabel *>("mistakeCount")->text(), QString("Mistakes: 0"));
        window.close();
    }

    void solvedOverlay() {
        QTemporaryDir directory;
        std::mt19937 random(91);
        Game game(generatePuzzle(Difficulty::Easy, random));
        const auto empty = std::find(game.values().begin(), game.values().end(), 0);
        const int cell = static_cast<int>(std::distance(game.values().begin(), empty));
        auto values = game.puzzle().solution;
        values[cell] = 0;
        QVERIFY(game.restore(values, {}, 0));
        saveSession(directory.filePath("game.json"), game, 2098, true);
        const auto themePath = directory.filePath("theme.json");
        auto initial = presetTheme("forest");
        writeJson(themePath, initial.toJson());
        Window window(directory.path(), initial);
        window.show();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        auto *board = window.board();
        auto *overlay = board->findChild<SolvedOverlay *>("solvedOverlay");
        QVERIFY(overlay);
        QVERIFY(overlay->isHidden());

        window.enterDigit(game.puzzle().solution[cell] % 9 + 1);
        QCOMPARE(window.game()->remaining(), 0);
        QVERIFY(!window.game()->complete());
        QVERIFY(overlay->isHidden());
        window.enterDigit(game.puzzle().solution[cell]);
        QVERIFY(window.game()->complete());
        QVERIFY(overlay->isVisible());
        QVERIFY(window.findChild<QLabel *>("status")->text().contains("1 mistake. Ready for another?"));
        QCOMPARE(overlay->progress(), 0.0);
        QTest::qWait(100);
        QVERIFY(overlay->progress() > 0 && overlay->progress() < 1);
        QTRY_COMPARE_WITH_TIMEOUT(overlay->progress(), 1.0, 1000);
        const auto seconds = window.elapsedSeconds();
        const auto selected = board->selected();
        QTest::keyClick(board, Qt::Key_J);
        QCOMPARE(board->selected(), selected);
        QHelpEvent help(QEvent::ToolTip, QPoint(20, 20), board->mapToGlobal(QPoint(20, 20)));
        QApplication::sendEvent(board, &help);
        QVERIFY(!board->findChild<QLabel *>("boardTooltip")->isVisible());

        const auto screenshots = qEnvironmentVariable("SUDOKU_TEST_SCREENSHOTS");
        if (!screenshots.isEmpty()) {
            QDir().mkpath(screenshots);
            QVERIFY(window.grab().save(screenshots + "/solved-forest.png"));
        }
        writeJson(themePath, presetTheme("paper").toJson());
        QTRY_COMPARE(window.theme().name, QString("paper"));
        QCOMPARE(overlay->progress(), 1.0);
        QTest::qWait(80);
        if (!screenshots.isEmpty())
            QVERIFY(window.grab().save(screenshots + "/solved-paper.png"));
        window.resize(720, 760);
        QTest::qWait(80);
        QCOMPARE(overlay->geometry(), board->boardRect().toRect());
        QCOMPARE(window.elapsedSeconds(), seconds);
        if (!screenshots.isEmpty())
            QVERIFY(window.grab().save(screenshots + "/solved-compact.png"));

        QPushButton *undo = nullptr;
        for (auto *button : window.findChildren<QPushButton *>("tool")) {
            if (button->accessibleName() == "Undo last move")
                undo = button;
        }
        QVERIFY(undo);
        QVERIFY(undo->isEnabled());
        QTest::mouseClick(undo, Qt::LeftButton);
        QVERIFY(!window.game()->complete());
        QVERIFY(overlay->isHidden());
        QCOMPARE(board->selected(), cell);
        QTest::qWait(1100);
        QVERIFY(window.elapsedSeconds() > seconds);
        window.enterDigit(game.puzzle().solution[cell]);
        QVERIFY(overlay->isVisible());
        QCOMPARE(overlay->progress(), 0.0);
        window.close();
        Window restored(directory.path(), presetTheme("paper"));
        restored.show();
        QVERIFY(QTest::qWaitForWindowActive(&restored));
        QVERIFY(restored.game()->complete());
        auto *restoredOverlay = restored.board()->findChild<SolvedOverlay *>("solvedOverlay");
        QVERIFY(restoredOverlay->isVisible());
        QTest::keyClick(restored.board(), Qt::Key_Z, Qt::ControlModifier);
        QVERIFY(!restored.game()->complete());
        QVERIFY(restoredOverlay->isHidden());
        QCOMPARE(restored.board()->selected(), cell);
        restored.enterDigit(game.puzzle().solution[cell]);
        QVERIFY(restored.game()->complete());
        QVERIFY(restoredOverlay->isVisible());
        QCOMPARE(restoredOverlay->progress(), 0.0);
        QTest::mouseClick(restored.findChild<QPushButton *>("primary"), Qt::LeftButton);
        QVERIFY(restoredOverlay->isHidden());
        QTRY_VERIFY_WITH_TIMEOUT(restored.game() && !restored.game()->complete(), 15000);
        QVERIFY(restoredOverlay->isHidden());
        restored.close();
    }

    void legacySolvedUndo() {
        QTemporaryDir directory;
        std::mt19937 random(91);
        Game game(generatePuzzle(Difficulty::Easy, random));
        QVERIFY(game.restore(game.puzzle().solution, {}, 0));
        const auto path = directory.filePath("game.json");
        saveSession(path, game, 2098, true);
        auto legacy = readJson(path);
        legacy.remove("history");
        legacy.remove("mistakes");
        legacy.remove("wrong_attempts");
        writeJson(path, legacy);
        Window window(directory.path(), presetTheme("forest"));
        window.show();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        QVERIFY(window.game()->complete());
        auto *overlay = window.board()->findChild<SolvedOverlay *>("solvedOverlay");
        QVERIFY(overlay->isVisible());
        QTest::keyClick(window.board(), Qt::Key_Z, Qt::ControlModifier);
        QVERIFY(!window.game()->complete());
        QCOMPARE(window.game()->remaining(), 1);
        QVERIFY(overlay->isHidden());
        const int cell = window.board()->selected();
        QVERIFY(window.game()->editable(cell));
        QCOMPARE(window.game()->values()[cell], 0);
        window.enterDigit(window.game()->puzzle().solution[cell]);
        QVERIFY(window.game()->complete());
        QVERIFY(overlay->isVisible());
        QCOMPARE(overlay->progress(), 0.0);
        window.close();
    }

    void desktopTransitions() {
        QTemporaryDir directory;
        const auto themes = directory.filePath("themes");
        const auto selector = themes + "/.caelestia-use";
        const auto current = themes + "/current";
        const auto themePath = directory.filePath("theme.json");
        const auto writePalette = [](const QString &path, const QString &accent) {
            QDir().mkpath(QFileInfo(path).absolutePath());
            QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly))
                return false;
            file.write(QString("Singleton {\nproperty color background: \"#111111\"\n"
                               "property color foreground: \"#eeeeee\"\n"
                               "property color primary: \"%1\"\n}\n").arg(accent).toUtf8());
            return file.commit();
        };
        const auto replaceLink = [](const QString &target, const QString &link) {
            const auto temporary = link + ".next";
            return ::symlink(QFile::encodeName(target).constData(), QFile::encodeName(temporary).constData()) == 0
                && ::rename(QFile::encodeName(temporary).constData(), QFile::encodeName(link).constData()) == 0;
        };
        QVERIFY(writePalette(themes + "/first/Colors.qml", "#112233"));
        QVERIFY(writePalette(themes + "/second/Colors.qml", "#223344"));
        QVERIFY(writePalette(themes + "/.dynamic/caelestia/Colors.qml", "#334455"));
        QVERIFY(writePalette(themes + "/.dynamic/noctalia/Colors.qml", "#445566"));
        QVERIFY(replaceLink(themes + "/first", current));
        QVERIFY(replaceLink("current", selector));
        const auto initial = followTheme("desktop", selector);
        writeJson(themePath, initial.toJson());
        ThemeWatcher watcher(themePath, initial);
        QSignalSpy changes(&watcher, &ThemeWatcher::changed);
        QSignalSpy rejections(&watcher, &ThemeWatcher::rejected);
        QTest::qWait(80);
        QCOMPARE(changes.count(), 0);

        const auto scripts = qEnvironmentVariable("SUDOKU_DESKTOP_SCRIPTS_DIR");
        const auto activate = [&](const QString &mode) {
            if (scripts.isEmpty())
                return replaceLink(mode == "system" ? QString("current") : ".dynamic/" + mode, selector);
            const auto provider = mode == "system" ? QString("caelestia") : mode;
            const auto dynamic = themes + "/.dynamic/" + provider;
            QDir().mkpath(dynamic + "/spicetify");
            for (const auto &name : {"/spicetify/color.ini", "/spicetify/user.css"}) {
                QFile file(dynamic + name);
                if (!file.open(QIODevice::WriteOnly))
                    return false;
            }
            QFile marker(dynamic + "/." + provider + "-generated");
            if (!marker.open(QIODevice::WriteOnly))
                return false;
            marker.close();
            QProcess command;
            auto environment = QProcessEnvironment::systemEnvironment();
            environment.insert("THEMES_DIR", themes);
            environment.insert("APPLICATION_THEME_LINK", selector);
            environment.insert("DYNAMIC_THEME_DIR", themes + "/.dynamic/caelestia");
            environment.insert("NOCTALIA_DYNAMIC_THEME_DIR", themes + "/.dynamic/noctalia");
            environment.insert("SPICETIFY_DYNAMIC_LINK", directory.filePath("spicetify-link"));
            command.setProcessEnvironment(environment);
            QStringList arguments{"--activate-only", mode == "system" ? "system" : "dynamic"};
            if (mode != "system")
                arguments.append(mode);
            command.start(scripts + "/theme-apply", arguments);
            const auto finished = command.waitForFinished();
            if (!finished || command.exitCode() != 0)
                qWarning().noquote() << command.errorString() << command.readAllStandardError();
            return finished && command.exitCode() == 0;
        };

        // The coordinator publishes these selectors for all six shell transitions.
        for (const auto &mode : {"caelestia", "noctalia", "caelestia", "system", "noctalia", "system", "caelestia"}) {
            QVERIFY(activate(mode));
            const auto accent = QString(mode) == "system" ? "#112233"
                : QString(mode) == "caelestia" ? "#334455" : "#445566";
            QTRY_COMPARE(watcher.theme().hex("accent"), QString(accent));
            QCOMPARE(loadTheme(themePath).sourceKind, QString("desktop"));
            QCOMPARE(loadTheme(themePath).hex("accent"), QString(accent));
        }
        QVERIFY(writePalette(themes + "/.dynamic/caelestia/Colors.qml", "#556677"));
        QTRY_COMPARE(watcher.theme().hex("accent"), QString("#556677"));
        QVERIFY(activate("system"));
        QTRY_COMPARE(watcher.theme().hex("accent"), QString("#112233"));
        QVERIFY(replaceLink(themes + "/second", current));
        QTRY_COMPARE(watcher.theme().hex("accent"), QString("#223344"));
        if (!scripts.isEmpty()) {
            QProcess command;
            auto environment = QProcessEnvironment::systemEnvironment();
            environment.insert("THEMES_DIR", themes);
            command.setProcessEnvironment(environment);
            command.start(scripts + "/theme", {"--current-only", "first"});
            QVERIFY(command.waitForFinished());
            QCOMPARE(command.exitCode(), 0);
            QTRY_COMPARE(watcher.theme().hex("accent"), QString("#112233"));
        }
        QVERIFY(replaceLink(themes + "/first", current));
        QTRY_COMPARE(watcher.theme().hex("accent"), QString("#112233"));
        for (const auto &mode : {"noctalia", "caelestia"}) {
            QVERIFY(activate(mode));
            QVERIFY(writePalette(themes + "/.dynamic/" + mode + "/Colors.qml", "#667788"));
            QTRY_COMPARE(watcher.theme().hex("accent"), QString("#667788"));
            QVERIFY(activate("system"));
            QTRY_COMPARE(watcher.theme().hex("accent"), QString("#112233"));
        }
        const auto before = changes.count();
        QVERIFY(activate("system"));
        QTest::qWait(100);
        QCOMPARE(changes.count(), before);
        QCOMPARE(rejections.count(), 0);
        QVERIFY(replaceLink(themes + "/missing", selector));
        QTRY_VERIFY(rejections.count() > 0);
        QCOMPARE(watcher.theme().hex("accent"), QString("#112233"));
        QVERIFY(writePalette(themes + "/missing/Colors.qml", "#778899"));
        QTRY_COMPARE(watcher.theme().hex("accent"), QString("#778899"));
    }

    void sourceFollowingAndSymlinkChanges() {
        QTemporaryDir directory;
        const auto first = directory.filePath("first");
        const auto second = directory.filePath("second");
        QDir().mkpath(first);
        QDir().mkpath(second);
        const auto sourcePath = first + "/Colors.qml";
        const auto nextPath = second + "/Colors.qml";
        const auto writePalette = [](const QString &path, const QByteArray &accent) {
            QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly))
                return false;
            file.write("Singleton {\nproperty color background: \"#111111\"\nproperty color foreground: \"#eeeeee\"\nproperty color primary: \"");
            file.write(accent);
            file.write("\"\n}\n");
            return file.commit();
        };
        QVERIFY(writePalette(sourcePath, "#123456"));
        QVERIFY(writePalette(nextPath, "#abcdef"));
        const auto current = directory.filePath("current");
        QVERIFY(QFile::link(first, current));
        const auto themePath = directory.filePath("theme.json");
        const auto initial = followTheme("static", current);
        writeJson(themePath, initial.toJson());
        ThemeWatcher watcher(themePath, initial);
        QSignalSpy changes(&watcher, &ThemeWatcher::changed);
        QCOMPARE(watcher.theme().hex("accent"), QString("#123456"));
        QVERIFY(writePalette(sourcePath, "#654321"));
        QTRY_COMPARE(watcher.theme().hex("accent"), QString("#654321"));
        QCOMPARE(loadTheme(themePath).hex("accent"), QString("#654321"));
        const auto replacement = directory.filePath("replacement");
        QVERIFY(QFile::link(second, replacement));
        QVERIFY(::rename(QFile::encodeName(replacement).constData(), QFile::encodeName(current).constData()) == 0);
        QTRY_COMPARE(watcher.theme().hex("accent"), QString("#abcdef"));
        QCOMPARE(watcher.theme().name, QString("static: second"));
        QVERIFY(writePalette(nextPath, "#fedcba"));
        QTRY_COMPARE(watcher.theme().hex("accent"), QString("#fedcba"));
        QVERIFY(changes.count() >= 3);
        writeJson(themePath, presetTheme("rose").toJson());
        QTRY_COMPARE(watcher.theme().name, QString("rose"));
        QVERIFY(writePalette(nextPath, "#111122"));
        QTest::qWait(100);
        QCOMPARE(watcher.theme().name, QString("rose"));
    }

    void liveThemeAndInteraction() {
        QTemporaryDir directory;
        const QString path = directory.filePath("theme.json");
        auto initial = ensureTheme(path);
        Window window(directory.path(), initial);
        window.show();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        QTRY_VERIFY_WITH_TIMEOUT(window.game() != nullptr, 15000);
        auto *board = window.board();
        const int cell = board->selected();
        const auto *game = window.game();
        const auto clues = game->puzzle().clues;
        QTest::keyClick(board, Qt::Key_H);
        QCOMPARE(board->selected(), cell / 9 * 9 + (cell % 9 + 8) % 9);
        QCOMPARE(game->hints(), 0);
        QTest::keyClick(board, Qt::Key_J);
        QCOMPARE(board->selected(), (cell / 9 * 9 + (cell % 9 + 8) % 9 + 9) % 81);
        QTest::keyClick(board, Qt::Key_K);
        QTest::keyClick(board, Qt::Key_L);
        QCOMPARE(board->selected(), cell);
        QHelpEvent tooltip(QEvent::ToolTip, QPoint(20, 20), board->mapToGlobal(QPoint(20, 20)));
        QApplication::sendEvent(board, &tooltip);
        auto *tooltipLabel = board->findChild<QLabel *>("boardTooltip");
        QVERIFY(tooltipLabel);
        auto *opacity = qobject_cast<QGraphicsOpacityEffect *>(tooltipLabel->graphicsEffect());
        QVERIFY(opacity);
        QVERIFY(tooltipLabel->isVisible());
        QCOMPARE(opacity->opacity(), 0.0);
        QTest::qWait(75);
        QVERIFY(opacity->opacity() > 0 && opacity->opacity() < 1);
        QTest::qWait(125);
        QCOMPARE(opacity->opacity(), 1.0);
        const auto tooltipImage = window.grab().toImage();
        const auto samplePoint = tooltipLabel->mapTo(&window, QPoint(3, 3));
        const auto sample = tooltipImage.pixelColor(qRound(samplePoint.x() * tooltipImage.devicePixelRatio()),
                                                   qRound(samplePoint.y() * tooltipImage.devicePixelRatio()));
        QCOMPARE(sample.alpha(), 255);
        QCOMPARE(sample, window.theme().color("surface_alt"));
        QTest::qWait(2200);
        QVERIFY(tooltipLabel->isVisible());
        QCOMPARE(opacity->opacity(), 1.0);
        QMouseEvent movement(QEvent::MouseMove, QPointF(30, 20),
                             board->mapToGlobal(QPoint(30, 20)), Qt::NoButton,
                             Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(board, &movement);
        QTRY_VERIFY_WITH_TIMEOUT(!tooltipLabel->isVisible(), 400);
        QApplication::sendEvent(board, &tooltip);
        QVERIFY(!tooltipLabel->isVisible());
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(board, &leave);
        QEnterEvent enter(QPointF(20, 20), QPointF(20, 20), board->mapToGlobal(QPoint(20, 20)));
        QApplication::sendEvent(board, &enter);
        QApplication::sendEvent(board, &tooltip);
        QVERIFY(tooltipLabel->isVisible());
        QApplication::sendEvent(board, &leave);
        window.activateWindow();
        board->setFocus();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        QTest::keyClick(board, Qt::Key_N);
        QTest::keyClick(board, Qt::Key_2);
        QTest::keyClick(board, Qt::Key_7);
        QCOMPARE(game->values()[cell], 0);
        QCOMPARE(game->notes()[cell], std::uint16_t((1u << 2) | (1u << 7)));
        QTest::keyClick(board, Qt::Key_2);
        QCOMPARE(game->notes()[cell], std::uint16_t(1u << 7));
        QTest::keyClick(board, Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(game->notes()[cell], std::uint16_t((1u << 2) | (1u << 7)));

        QProcess command;
        const auto binary = QCoreApplication::applicationDirPath() + "/sudoku";
        command.start(binary, {"--config-dir", directory.path(), "theme", "preset", "paper"});
        QVERIFY(command.waitForFinished());
        QCOMPARE(command.exitCode(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(window.theme().name, QString("paper"), 3000);
        QCOMPARE(window.game(), game);
        QVERIFY(game->puzzle().clues == clues);
        QCOMPARE(game->notes()[cell], std::uint16_t((1u << 2) | (1u << 7)));

        for (const auto &name : {"rose", "slate", "forest"}) {
            writeJson(path, presetTheme(name).toJson());
            QTRY_COMPARE_WITH_TIMEOUT(window.theme().name, QString(name), 3000);
        }
        const auto last = window.theme();
        QFile corrupt(path);
        QVERIFY(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate));
        corrupt.write("{broken");
        corrupt.close();
        QTest::qWait(120);
        QVERIFY(window.theme() == last);
        QCOMPARE(window.findChild<QLabel *>("status")->palette().color(QPalette::WindowText), last.mistakeColor);
        writeJson(path, presetTheme("paper").toJson());
        QTRY_COMPARE_WITH_TIMEOUT(window.theme().name, QString("paper"), 3000);

        window.activateWindow();
        board->setFocus();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        QTest::mouseClick(window.findChild<QPushButton *>("pause"), Qt::LeftButton);
        QVERIFY(window.isPaused());
        const auto seconds = window.elapsedSeconds();
        QTest::qWait(1100);
        QCOMPARE(window.elapsedSeconds(), seconds);
        QTest::keyClick(board, Qt::Key_4);
        QCOMPARE(game->notes()[cell], std::uint16_t((1u << 2) | (1u << 7)));
        QTest::mouseClick(window.findChild<QPushButton *>("pause"), Qt::LeftButton);
        QVERIFY(!window.isPaused());

        window.resize(720, 760);
        QTest::qWait(80);
        if (QGuiApplication::platformName() == "offscreen")
            QCOMPARE(window.size(), QSize(720, 760));
        const auto rectangle = board->boardRect();
        QVERIFY(qAbs(rectangle.width() - rectangle.height()) < 1);
        QVERIFY(rectangle.width() >= 280);
        for (auto *child : window.findChildren<QPushButton *>()) {
            if (child->isVisible()) {
                const QRect bounds(child->mapTo(&window, QPoint()), child->size());
                QVERIFY2(window.rect().contains(bounds), qPrintable(child->objectName()));
            }
        }
        const auto digitButtons = window.findChildren<QPushButton *>("digit");
        for (auto *first : digitButtons) {
            for (auto *second : digitButtons) {
                if (first != second)
                    QVERIFY(!first->geometry().intersects(second->geometry()));
            }
        }
        window.resize(1040, 820);
        QTest::qWait(100);
        const QString screenshotDirectory = qEnvironmentVariable("SUDOKU_TEST_SCREENSHOTS");
        if (!screenshotDirectory.isEmpty()) {
            QDir().mkpath(screenshotDirectory);
            QVERIFY(window.grab().save(screenshotDirectory + "/paper.png"));
            writeJson(path, presetTheme("forest").toJson());
            QTRY_COMPARE(window.theme().name, QString("forest"));
            QTest::qWait(80);
            QVERIFY(window.grab().save(screenshotDirectory + "/forest.png"));
            window.resize(720, 760);
            QTest::qWait(80);
            QVERIFY(window.grab().save(screenshotDirectory + "/compact.png"));
        }
        QFile processStatus("/proc/self/status");
        QVERIFY(processStatus.open(QIODevice::ReadOnly));
        const auto processLines = QString::fromUtf8(processStatus.readAll()).split('\n');
        for (const auto &line : processLines) {
            if (line.startsWith("VmRSS:") || line.startsWith("VmHWM:"))
                qInfo().noquote() << QGuiApplication::platformName() << line;
        }
        window.close();
        qint64 elapsed = 0;
        bool check = true;
        auto restored = loadSession(directory.filePath("game.json"), elapsed, check);
        QCOMPARE(restored->notes()[cell], std::uint16_t((1u << 2) | (1u << 7)));
    }
};

QTEST_MAIN(UiTests)
#include "ui_tests.moc"
