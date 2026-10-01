#pragma once

#include "theme.h"

namespace sudoku {

/** @brief Import a palette file, optionally assigning a display name. */
Theme importPalette(const QString &path, const QString &name = {});
/** @brief Bind to a supported external theme source and resolve its current palette. */
Theme followTheme(const QString &kind, const QString &path = {});
/** @brief Re-read the source bound to a theme and return its resolved colors. */
Theme refreshSource(const Theme &theme);
/** @brief Return files and directories that must be watched for source changes. */
QStringList sourceWatchPaths(const Theme &theme);

}
