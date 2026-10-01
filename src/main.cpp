/**
 * @file main.cpp
 * @brief Application entry point for the Qt game and theme command line.
 *
 * Startup parses options before creating either a core-only command process or
 * the widget application. Window-system defaults are selected before Qt loads
 * the UI, and startup failures are reported as a concise command-line error.
 */
#include "cli.h"
#include "core/storage.h"
#include "core/theme.h"
#include "ui/window.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QTextStream>
#include <memory>

int main(int argc, char **argv) {
    QCoreApplication::setApplicationName("sudoku");
    QCoreApplication::setApplicationVersion("1.0.0");
    QCoreApplication::setOrganizationName("QuietSudoku");

    try {
        QStringList arguments;
        for (int index = 1; index < argc; ++index)
            arguments.append(QString::fromLocal8Bit(argv[index]));
        const auto options = sudoku::parseOptions(arguments);
        if (options.help || options.version || !options.arguments.isEmpty()) {
            QCoreApplication app(argc, argv);
            if (options.help) {
                sudoku::printHelp();
                return 0;
            }
            if (options.version) {
                QTextStream(stdout) << "Sudoku 1.0.0\n";
                return 0;
            }
            return sudoku::runThemeCommand(options);
        }
        if (options.dryRun)
            throw std::invalid_argument("--dry-run applies to theme commands");
        if (!qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
            qputenv("QT_QPA_PLATFORM", "wayland");
        // This raster-only UI does not need a GPU driver or EGL buffers.
        if (qEnvironmentVariableIsEmpty("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION"))
            qputenv("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION", "shm");
        QApplication app(argc, argv);
        app.setStyle("Fusion");
        app.setDesktopFileName("io.github.quiet_sudoku");
        app.setWindowIcon(QIcon::fromTheme("io.github.quiet_sudoku"));
        auto theme = sudoku::ensureTheme(options.configDirectory + "/theme.json");
        sudoku::Window window(options.configDirectory, theme, options.difficulty, nullptr, options.autoPauseSeconds);
        window.show();
        return app.exec();
    } catch (const std::exception &error) {
        QTextStream(stderr) << "sudoku: " << error.what() << '\n';
        return 1;
    }
}
