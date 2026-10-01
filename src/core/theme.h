#pragma once

#include <QColor>
#include <QJsonObject>
#include <QMap>
#include <QStringList>

namespace sudoku {

/** @brief Validated palette, source binding, and derived entry colors. */
struct Theme {
    QString name; ///< Display name or preset identifier.
    QMap<QString, QColor> colors; ///< Complete map of configurable palette keys.
    QString sourceKind; ///< External source type, empty when following no source.
    QString sourcePath; ///< Path or identifier used by the external source.
    QColor mistakeColor = {}; ///< Cached readable color for incorrect entries.
    QColor correctColor = {}; ///< Cached readable color for correct entries.
    /** @brief Read a configured color by key. */
    QColor color(const QString &key) const { return colors.value(key); }
    /** @brief Return a configured color as a #RRGGBB string. */
    QString hex(const QString &key) const { return color(key).name(); }
    /** @brief Serialize the configurable palette and source metadata. */
    QJsonObject toJson() const;
    /** @brief Compare all theme fields, including derived colors. */
    bool operator==(const Theme &) const = default;
};

/** @brief Return the required configurable palette keys in canonical order. */
QStringList colorKeys();
/** @brief Return the names of built-in themes. */
QStringList presetNames();
/** @brief Construct a built-in palette by name. */
Theme presetTheme(const QString &name);
/** @brief Parse and validate a complete theme object. */
Theme parseTheme(const QJsonObject &data);
/** @brief Load and validate a theme JSON file. */
Theme loadTheme(const QString &path);
/** @brief Load an existing theme or atomically create the default theme file. */
Theme ensureTheme(const QString &path);
/** @brief Apply key=value palette assignments to a copy of a theme. */
Theme setColors(Theme theme, const QStringList &assignments);

}
