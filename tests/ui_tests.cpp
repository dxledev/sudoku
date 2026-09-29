#include "ui/window.h"
#include "core/storage.h"
#include "core/theme_source.h"

#include <QFile>
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
