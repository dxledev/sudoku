#include "core/game.h"
#include "core/storage.h"
#include "core/statistics.h"
#include "core/theme.h"
#include "core/theme_source.h"

#include <QTemporaryDir>
#include <QJsonArray>
#include <QFileInfo>
#include <QtTest>
#include <algorithm>

using namespace sudoku;

class CoreTests : public QObject {
    Q_OBJECT
private slots:
    void statisticsResults() {
        Statistics statistics;
        for (const auto &info : difficulties) {
            std::mt19937 random(91);
            Game game(generatePuzzle(info.value, random));
            const auto id = game.id();
            QVERIFY(!statistics.recordWin(game, 300));
            QVERIFY(!statistics.recordQuit(game, 180));
            const int cell = static_cast<int>(std::distance(game.values().begin(), std::find(game.values().begin(), game.values().end(), 0)));
            QVERIFY(game.enter(cell, 1, true));
            QVERIFY(!game.enteredValue());
            QVERIFY(!statistics.recordQuit(game, 180));
            QVERIFY(game.hint(cell));
            QVERIFY(!game.enteredValue());
            QVERIFY(game.undo());
            QVERIFY(game.enter(cell, game.puzzle().solution[cell] % 9 + 1));
            QVERIFY(game.enteredValue());
            QVERIFY(game.undo());
            QVERIFY(game.enteredValue());
            QVERIFY(!statistics.recordQuit(game, 179));
            QVERIFY(statistics.recordQuit(game, 180));
            QVERIFY(!statistics.recordQuit(game, 181));
            QCOMPARE(statistics.at(info.value).quits, 1);
            Game winner(generatePuzzle(info.value, random));
            QVERIFY(winner.id() != id);
            for (int index = 0; index < 81; ++index) {
                if (winner.editable(index))
                    QVERIFY(winner.enter(index, winner.puzzle().solution[index]));
            }
            QVERIFY(statistics.recordWin(winner, 240));
            QVERIFY(!statistics.recordWin(winner, 300));
            QVERIFY(winner.undo());
            QVERIFY(!statistics.recordQuit(winner, 300));
            const int reopened = static_cast<int>(std::distance(winner.values().begin(), std::find(winner.values().begin(), winner.values().end(), 0)));
            QVERIFY(winner.enter(reopened, winner.puzzle().solution[reopened]));
            QVERIFY(!statistics.recordWin(winner, 300));
            QCOMPARE(statistics.at(info.value).wins, 1);
            QCOMPARE(statistics.at(info.value).totalSeconds, 240);
            QCOMPARE(statistics.at(info.value).bestSeconds, 240);
            Game second(generatePuzzle(info.value, random));
            QVERIFY(second.restore(second.puzzle().solution, {}, 0));
            QVERIFY(statistics.recordWin(second, 120));
            QCOMPARE(statistics.at(info.value).wins, 2);
            QCOMPARE(statistics.at(info.value).totalSeconds, 360);
            QCOMPARE(statistics.at(info.value).bestSeconds, 120);
        }
        QTemporaryDir directory;
        const auto path = directory.filePath("stats.json");
        QCOMPARE(Statistics::load(path).at(Difficulty::Easy).wins, 0);
        statistics.save(path);
        const auto restored = Statistics::load(path);
        QCOMPARE(restored.toJson(), statistics.toJson());
        auto invalid = statistics.toJson();
        invalid.insert("version", 2);
        QVERIFY_EXCEPTION_THROWN(Statistics::fromJson(invalid), std::runtime_error);
        auto levels = statistics.toJson().value("difficulties").toObject();
        auto easy = levels.value("easy").toObject();
        easy.insert("wins", -1);
        levels.insert("easy", easy);
        invalid = statistics.toJson();
        invalid.insert("difficulties", levels);
        QVERIFY_EXCEPTION_THROWN(Statistics::fromJson(invalid), std::runtime_error);
    }

    void statisticsSessionTracking() {
        QTemporaryDir directory;
        std::mt19937 random(91);
        Game game(generatePuzzle(Difficulty::Easy, random));
        const int cell = static_cast<int>(std::distance(game.values().begin(), std::find(game.values().begin(), game.values().end(), 0)));
        QVERIFY(game.enter(cell, game.puzzle().solution[cell]));
        QVERIFY(game.undo());
        saveSession(directory.filePath("game.json"), game, 180, true);
        qint64 seconds = 0;
        bool check = false;
        auto restored = loadSession(directory.filePath("game.json"), seconds, check);
        QCOMPARE(restored->id(), game.id());
        QVERIFY(restored->enteredValue());
        Statistics statistics;
        QVERIFY(statistics.recordQuit(*restored, seconds));
        const auto loaded = Statistics::fromJson(statistics.toJson());
        auto again = loaded;
        QVERIFY(!again.recordQuit(*restored, seconds));
        auto legacy = readJson(directory.filePath("game.json"));
        legacy.remove("id");
        legacy.remove("entered_value");
        writeJson(directory.filePath("game.json"), legacy);
        auto old = loadSession(directory.filePath("game.json"), seconds, check);
        auto oldAgain = loadSession(directory.filePath("game.json"), seconds, check);
        QCOMPARE(old->id(), oldAgain->id());
        QVERIFY(old->enteredValue());
    }

    void generation_data() {
        QTest::addColumn<int>("level");
        for (int level = 0; level < 4; ++level)
            QTest::newRow(difficulties[level].name.data()) << level;
    }

    void generation() {
        QFETCH(int, level);
        const auto difficulty = static_cast<Difficulty>(level);
        const auto &info = difficultyInfo(difficulty);
        for (unsigned seed = 0; seed < 8; ++seed) {
            std::mt19937 random(seed + 100 * level);
            const auto puzzle = generatePuzzle(difficulty, random);
            QVERIFY(isValid(puzzle.solution, true));
            QVERIFY(isValid(puzzle.clues));
            QCOMPARE(countSolutions(puzzle.clues), 1);
            const int clues = 81 - static_cast<int>(std::count(puzzle.clues.begin(), puzzle.clues.end(), 0));
            QVERIFY(clues >= info.targetClues);
            QVERIFY(clues <= info.maximumClues);
            for (int index = 0; index < 81; ++index)
                QVERIFY(!puzzle.clues[index] || puzzle.clues[index] == puzzle.solution[index]);
            auto trial = puzzle.clues;
            if (difficulty == Difficulty::Easy)
                QVERIFY(solveSingles(trial));
            if (difficulty == Difficulty::Expert)
                QVERIFY(!solveSingles(trial));
        }
    }

    void solverRejectsInvalid() {
        Grid invalid{};
        invalid[0] = invalid[1] = 7;
        QCOMPARE(countSolutions(invalid), 0);
        invalid[0] = -1;
        QVERIFY(!isValid(invalid));
        QCOMPARE(countSolutions(Grid{}), 2);
    }

    void generationCancellation() {
        std::mt19937 random(9);
        QVERIFY_EXCEPTION_THROWN(generatePuzzle(Difficulty::Expert, random, [] { return true; }), std::runtime_error);
    }

    void pencilNotesAndUndo() {
        std::mt19937 random(91);
        Game game(generatePuzzle(Difficulty::Easy, random));
        const int cell = static_cast<int>(std::distance(game.values().begin(), std::find(game.values().begin(), game.values().end(), 0)));
        QVERIFY(game.enter(cell, 2, true));
        QVERIFY(game.enter(cell, 7, true));
        QCOMPARE(game.values()[cell], 0);
        QCOMPARE(game.notes()[cell], std::uint16_t((1u << 2) | (1u << 7)));
        QVERIFY(game.enter(cell, 2, true));
        QCOMPARE(game.notes()[cell], std::uint16_t(1u << 7));
        QVERIFY(game.undo());
        QCOMPARE(game.notes()[cell], std::uint16_t((1u << 2) | (1u << 7)));
        QVERIFY(game.enter(cell, game.puzzle().solution[cell]));
        QCOMPARE(game.notes()[cell], std::uint16_t(0));
        QVERIFY(!game.enter(cell, 1, true));
        QVERIFY(game.undo());
        QCOMPARE(game.values()[cell], 0);
        QCOMPARE(game.notes()[cell], std::uint16_t((1u << 2) | (1u << 7)));
        QVERIFY(game.erase(cell));
        QCOMPARE(game.notes()[cell], std::uint16_t(0));
        QVERIFY(game.undo());
        QVERIFY(game.notes()[cell] != 0);
        for (int index = 0; index < 81; ++index) {
            if (game.puzzle().clues[index]) {
                QVERIFY(!game.enter(index, 4));
                QVERIFY(!game.erase(index));
            }
        }
    }

    void peerNotesAndHints() {
        std::mt19937 random(97);
        Game game(generatePuzzle(Difficulty::Medium, random));
        int cell = -1, peer = -1;
        for (int first = 0; first < 81 && cell < 0; ++first) {
            for (int second = 0; second < 81; ++second) {
                if (game.editable(first) && game.editable(second) && arePeers(first, second)) {
                    cell = first;
                    peer = second;
                    break;
                }
            }
        }
        QVERIFY(cell >= 0);
        const int digit = game.puzzle().solution[cell];
        QVERIFY(game.enter(peer, digit, true));
        QVERIFY(game.hint(cell));
        QCOMPARE(game.hints(), 1);
        QCOMPARE(game.values()[cell], digit);
        QCOMPARE(game.notes()[peer], std::uint16_t(0));
        QVERIFY(game.undo());
        QCOMPARE(game.hints(), 0);
        QCOMPARE(game.notes()[peer], std::uint16_t(1u << digit));
        QVERIFY(game.enter(cell, digit % 9 + 1));
        QVERIFY(game.wrong(cell));
        QCOMPARE(game.notes()[peer], std::uint16_t(1u << digit));
    }

    void completionAndUndo() {
        std::mt19937 random(43);
        Game game(generatePuzzle(Difficulty::Easy, random));
        for (int cell = 0; cell < 81; ++cell) {
            if (game.editable(cell))
                QVERIFY(game.enter(cell, game.puzzle().solution[cell]));
        }
        QVERIFY(game.complete());
        QCOMPARE(game.remaining(), 0);
        QVERIFY(game.undo());
        QVERIFY(!game.complete());
        QCOMPARE(game.remaining(), 1);
    }

    void mistakeCounting() {
        std::mt19937 random(91);
        Game game(generatePuzzle(Difficulty::Easy, random));
        std::vector<int> editable;
        for (int cell = 0; cell < 81; ++cell) {
            if (game.editable(cell))
                editable.push_back(cell);
        }
        const int cell = editable[0];
        const int correct = game.puzzle().solution[cell];
        const int wrong = correct % 9 + 1;
        const int otherWrong = wrong % 9 + 1;
        QVERIFY(game.enter(cell, wrong, true));
        QCOMPARE(game.mistakes(), 0);
        QVERIFY(game.enter(cell, wrong));
        QCOMPARE(game.mistakes(), 1);
        QVERIFY(!game.enter(cell, wrong));
        QCOMPARE(game.mistakes(), 1);
        QVERIFY(game.erase(cell));
        QVERIFY(game.enter(cell, wrong));
        QCOMPARE(game.mistakes(), 1);
        for (int repeat = 0; repeat < 5; ++repeat) {
            QVERIFY(game.erase(cell));
            QVERIFY(game.enter(cell, wrong, true));
            QVERIFY(game.enter(cell, wrong));
            QCOMPARE(game.mistakes(), 1);
        }
        QVERIFY(game.enter(cell, correct));
        QVERIFY(game.enter(cell, wrong));
        QCOMPARE(game.mistakes(), 1);
        QVERIFY(game.enter(cell, otherWrong));
        QCOMPARE(game.mistakes(), 2);
        QVERIFY(game.enter(cell, wrong));
        QCOMPARE(game.mistakes(), 3);
        QVERIFY(game.undo());
        QCOMPARE(game.mistakes(), 3);
        QVERIFY(game.enter(cell, wrong));
        QCOMPARE(game.mistakes(), 3);
        QVERIFY(game.enter(editable[1], game.puzzle().solution[editable[1]] % 9 + 1));
        QCOMPARE(game.mistakes(), 4);
        QVERIFY(game.hint(cell));
        QCOMPARE(game.mistakes(), 4);
        QVERIFY(game.enter(cell, wrong));
        QCOMPARE(game.mistakes(), 5);
        QVERIFY(!game.enter(-1, wrong));
        QVERIFY(!game.enter(cell, 0));
        QCOMPARE(game.mistakes(), 5);
    }

    void savedMistakesAndUndo() {
        QTemporaryDir directory;
        const auto path = directory.filePath("game.json");
        std::mt19937 random(91);
        Game game(generatePuzzle(Difficulty::Easy, random));
        const int cell = static_cast<int>(std::distance(game.values().begin(), std::find(game.values().begin(), game.values().end(), 0)));
        const int wrong = game.puzzle().solution[cell] % 9 + 1;
        QVERIFY(game.enter(cell, wrong));
        QVERIFY(game.erase(cell));
        saveSession(path, game, 42, true);
        qint64 elapsed = 0;
        bool check = false;
        auto restored = loadSession(path, elapsed, check);
        QCOMPARE(restored->mistakes(), 1);
        QVERIFY(restored->enter(cell, wrong));
        QCOMPARE(restored->mistakes(), 1);
        QVERIFY(restored->erase(cell));
        saveSession(path, *restored, 42, true);
        restored = loadSession(path, elapsed, check);
        QVERIFY(restored->enter(cell, wrong));
        QCOMPARE(restored->mistakes(), 1);
        QVERIFY(restored->enter(cell, restored->puzzle().solution[cell]));
        QVERIFY(restored->enter(cell, wrong));
        QCOMPARE(restored->mistakes(), 1);
        QVERIFY(restored->enter(cell, wrong % 9 + 1));
        QCOMPARE(restored->mistakes(), 2);
        QVERIFY(restored->enter(cell, wrong));
        QCOMPARE(restored->mistakes(), 3);
        for (int index = 0; index < 81; ++index) {
            if (restored->editable(index))
                QVERIFY(restored->enter(index, restored->puzzle().solution[index]));
        }
        saveSession(path, *restored, 42, true);
        restored = loadSession(path, elapsed, check);
        QVERIFY(restored->complete());
        QVERIFY(restored->undo());
        QVERIFY(!restored->complete());
        QCOMPARE(restored->mistakes(), 3);
        QVERIFY(restored->canUndo());
        auto data = readJson(path);
        data.insert("mistakes", -1);
        writeJson(path, data);
        QVERIFY_EXCEPTION_THROWN(loadSession(path, elapsed, check), std::runtime_error);
        data.insert("mistakes", 3);
        data.insert("wrong_attempts", QJsonArray{4});
        writeJson(path, data);
        QVERIFY_EXCEPTION_THROWN(loadSession(path, elapsed, check), std::runtime_error);
        data = readJson(path);
        data.remove("mistakes");
        data.remove("wrong_attempts");
        data.insert("history", QJsonArray{QJsonObject{}});
        writeJson(path, data);
        QVERIFY_EXCEPTION_THROWN(loadSession(path, elapsed, check), std::runtime_error);
    }

    void boundedSavedHistoryAndLegacyCompletion() {
        QTemporaryDir directory;
        const auto path = directory.filePath("game.json");
        std::mt19937 random(91);
        Game game(generatePuzzle(Difficulty::Easy, random));
        const int cell = static_cast<int>(std::distance(game.values().begin(), std::find(game.values().begin(), game.values().end(), 0)));
        for (int attempt = 0; attempt < 225; ++attempt)
            QVERIFY(game.enter(cell, 1, true));
        saveSession(path, game, 0, true);
        QVERIFY(QFileInfo(path).size() < 1024 * 1024);
        qint64 elapsed = 0;
        bool check = false;
        auto restored = loadSession(path, elapsed, check);
        int undone = 0;
        while (restored->undo())
            ++undone;
        QCOMPARE(undone, 200);
        QVERIFY(game.restore(game.puzzle().solution, {}, 0));
        saveSession(path, game, 2098, true);
        auto legacy = readJson(path);
        legacy.remove("history");
        legacy.remove("mistakes");
        legacy.remove("wrong_attempts");
        writeJson(path, legacy);
        restored = loadSession(path, elapsed, check);
        QVERIFY(restored->complete());
        QVERIFY(restored->undo());
        QCOMPARE(restored->remaining(), 1);
        const auto empty = std::find(restored->values().begin(), restored->values().end(), 0);
        const int reopened = static_cast<int>(std::distance(restored->values().begin(), empty));
        QVERIFY(restored->enter(reopened, restored->puzzle().solution[reopened]));
        QVERIFY(restored->complete());
    }

    void sessionRoundTripAndValidation() {
        QTemporaryDir directory;
        const auto path = directory.filePath("game.json");
        std::mt19937 random(24);
        Game game(generatePuzzle(Difficulty::Hard, random));
        const int cell = static_cast<int>(std::distance(game.values().begin(), std::find(game.values().begin(), game.values().end(), 0)));
        QVERIFY(game.enter(cell, 3, true));
        QVERIFY(game.enter(cell, 8, true));
        saveSession(path, game, 123, false);
        qint64 elapsed = 0;
        bool check = true;
        const auto restored = loadSession(path, elapsed, check);
        QVERIFY(restored->values() == game.values());
        QVERIFY(restored->notes() == game.notes());
        QCOMPARE(elapsed, 123);
        QVERIFY(!check);
        auto data = readJson(path);
        data.insert("elapsed_seconds", -1);
        writeJson(path, data);
        QVERIFY_EXCEPTION_THROWN(loadSession(path, elapsed, check), std::runtime_error);
    }

    void themeValidationAndRoundTrip() {
        QTemporaryDir directory;
        const auto path = directory.filePath("theme.json");
        const auto forest = ensureTheme(path);
        QVERIFY(loadTheme(path) == forest);
        const auto custom = setColors(forest, {"accent=#12ABEF", "background=#010203"});
        QCOMPARE(custom.hex("accent"), QString("#12abef"));
        writeJson(path, custom.toJson());
        QVERIFY(loadTheme(path) == custom);
        QVERIFY_EXCEPTION_THROWN(setColors(forest, {"accent=red"}), std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(setColors(forest, {"unknown=#ffffff"}), std::invalid_argument);
        auto broken = forest.toJson();
        broken.insert("colors", QJsonObject{});
        QVERIFY_EXCEPTION_THROWN(parseTheme(broken), std::invalid_argument);
        for (const auto &name : presetNames())
            QVERIFY(parseTheme(presetTheme(name).toJson()) == presetTheme(name));
    }

    void automaticMistakeColor() {
        QVERIFY(!colorKeys().contains("error"));
        QSet<QString> generated;
        for (const auto &name : presetNames()) {
            const auto theme = presetTheme(name);
            const auto color = theme.mistakeColor;
            QVERIFY(color.isValid());
            QVERIFY(color.hslHueF() < .04 || color.hslHueF() > .96);
            QVERIFY(color.red() > color.green());
            QVERIFY(color.red() > color.blue());
            QVERIFY(!theme.colors.contains("error"));
            QVERIFY(!theme.toJson().value("colors").toObject().contains("error"));
            QCOMPARE(parseTheme(theme.toJson()).mistakeColor, color);
            generated.insert(color.name());

            auto legacy = theme.toJson();
            auto colors = legacy.value("colors").toObject();
            colors.insert("error", "#00ff00");
            legacy.insert("colors", colors);
            QCOMPARE(parseTheme(legacy), theme);
            QVERIFY_EXCEPTION_THROWN(setColors(theme, {"error=#ff0000"}), std::invalid_argument);
        }
        QCOMPARE(generated.size(), presetNames().size());
        const auto forest = presetTheme("forest");
        QVERIFY(setColors(forest, {"accent=#ff0000"}).mistakeColor != forest.mistakeColor);
        const auto light = setColors(presetTheme("paper"), {"accent=#777777"});
        const auto dark = setColors(forest, {"accent=#777777"});
        QVERIFY(light.mistakeColor.lightnessF() < dark.mistakeColor.lightnessF());
    }

    void correctColorPreservesNonRedAccents() {
        for (const auto &name : {"forest", "paper", "slate"}) {
            const auto theme = presetTheme(name);
            QCOMPARE(theme.correctColor, theme.color("accent"));
        }
        for (const auto &accent : {"#00a777", "#99aaff", "#ffdd00", "#9966cc", "#777777", "#ffffff", "#000000"}) {
            const auto theme = setColors(presetTheme("forest"), {QString("accent=") + accent});
            QCOMPARE(theme.correctColor, QColor(accent));
        }
    }

    void correctColorAvoidsRedAccents_data() {
        QTest::addColumn<QString>("preset");
        QTest::addColumn<QString>("accent");
        for (const auto &preset : {"forest", "paper", "rose"}) {
            for (const auto &accent : {"#ff0000", "#ffb4a4", "#efb7ce", "#c96476", "#ff6000"})
                QTest::newRow(qPrintable(QString(preset) + accent)) << QString(preset) << QString(accent);
        }
    }

    void correctColorAvoidsRedAccents() {
        QFETCH(QString, preset);
        QFETCH(QString, accent);
        const auto theme = setColors(presetTheme(preset), {"accent=" + accent});
        const auto correct = theme.correctColor;
        QVERIFY(correct.isValid());
        QVERIFY(correct.hslHueF() > .125 && correct.hslHueF() < .875);
        QVERIFY(correct.green() > correct.red());
        QVERIFY(correct != theme.mistakeColor);
        QCOMPARE(theme.hex("accent"), accent);
        QCOMPARE(parseTheme(theme.toJson()), theme);
        QVERIFY(!theme.toJson().value("colors").toObject().contains("correct"));
        QVERIFY(theme.mistakeColor.hslHueF() < .04 || theme.mistakeColor.hslHueF() > .96);
    }

    void externalPalettes() {
        QTemporaryDir directory;
        const auto qml = directory.filePath("Colors.qml");
        QFile source(qml);
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("Singleton {\n"
            "readonly property color background: Qt.rgba(14 / 255, 30 / 255, 54 / 255, 0.88)\n"
            "readonly property color backgroundAlt: \"#0e1e36\"\n"
            "readonly property color foreground: \"#f4b999\"\n"
            "readonly property color primary: \"#9279aa\"\n"
            "readonly property color danger: \"#d2495b\"\n}\n");
        source.close();
        const auto theme = followTheme("static", directory.path());
        QCOMPARE(theme.hex("background"), QString("#0e1e36"));
        QCOMPARE(theme.hex("accent"), QString("#9279aa"));
        QCOMPARE(theme.hex("text"), QString("#f4b999"));
        QCOMPARE(theme.sourceKind, QString("static"));
        QVERIFY(parseTheme(theme.toJson()) == theme);
        QVERIFY(sourceWatchPaths(theme).contains(qml));
        const auto custom = setColors(theme, {"accent=#ffffff"});
        QVERIFY(custom.sourceKind.isEmpty());
        QVERIFY(!custom.toJson().contains("source"));

        const auto caelestia = directory.filePath("caelestia.json");
        writeJson(caelestia, {{"colours", QJsonObject{
            {"background", "09191a"}, {"onBackground", "ddcbb1"},
            {"accent", "89b7be"}, {"red", "d69a8b"},
        }}});
        const auto dynamic = followTheme("caelestia", caelestia);
        QCOMPARE(dynamic.hex("background"), QString("#09191a"));
        QCOMPARE(dynamic.hex("accent"), QString("#89b7be"));
        QVERIFY(dynamic.mistakeColor != QColor("#d69a8b"));
        auto palette = readJson(caelestia).value("colours").toObject();
        palette.insert("red", "00ff00");
        writeJson(caelestia, {{"colours", palette}});
        QCOMPARE(followTheme("caelestia", caelestia).mistakeColor, dynamic.mistakeColor);

        const auto noctalia = directory.filePath("noctalia.json");
        writeJson(noctalia, {{"dark", QJsonObject{
            {"surface", "#121318"}, {"on_surface", "#e3e1e9"},
            {"primary", "#b5c4ff"}, {"on_primary", "#1c2d61"},
            {"surface_container_low", "#1a1b21"},
        }}});
        const auto native = followTheme("noctalia", noctalia);
        QCOMPARE(native.hex("background"), QString("#121318"));
        QCOMPARE(native.hex("surface"), QString("#1a1b21"));
        QCOMPARE(native.hex("accent_text"), QString("#1c2d61"));
    }
};

QTEST_GUILESS_MAIN(CoreTests)
#include "core_tests.moc"
