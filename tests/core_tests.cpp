#include "core/game.h"
#include "core/storage.h"
#include "core/theme.h"
#include "core/theme_source.h"

#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>

using namespace sudoku;

class CoreTests : public QObject {
    Q_OBJECT
private slots:
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
        QCOMPARE(dynamic.hex("error"), QString("#d69a8b"));

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
