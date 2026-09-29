#include "theme.h"
#include "storage.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <stdexcept>

namespace sudoku {
namespace {

const QMap<QString, QStringList> palettes{
    {"forest", {"#101819", "#182324", "#1e2d2e", "#edf3ed", "#91a6a2", "#b6e3c6", "#152c24", "#304243", "#536967", "#355c4b", "#203230", "#2b453b", "#ff9b95"}},
    {"paper", {"#f4f1e9", "#fffcf5", "#ebe7dd", "#292f2c", "#6f7970", "#315c48", "#ffffff", "#ddd8cb", "#9aab9b", "#ccdfcc", "#eeeee1", "#dfebd7", "#bd4740"}},
    {"slate", {"#131720", "#1c2230", "#252e40", "#ebeffa", "#97a3be", "#b2c7ff", "#1d2a4b", "#34405a", "#576986", "#3b4e78", "#252e44", "#30415d", "#ffa6b0"}},
    {"rose", {"#211a22", "#2c232e", "#382d3b", "#f7eaf1", "#b8a0b2", "#efb7ce", "#3a2131", "#4e3a4c", "#85687f", "#654258", "#3b2c3b", "#4c3447", "#ffad91"}},
};

[[noreturn]] void invalid(const QString &message) {
    throw std::invalid_argument(message.toStdString());
}

}

QStringList colorKeys() {
    return {"background", "surface", "surface_alt", "text", "muted", "accent", "accent_text",
            "border", "grid", "selection", "related", "matching", "error"};
}

QStringList presetNames() {
    return palettes.keys();
}

Theme presetTheme(const QString &name) {
    if (!palettes.contains(name))
        invalid("Unknown preset: " + name + ". Available: " + presetNames().join(", "));
    Theme theme{name, {}, {}, {}};
    const auto keys = colorKeys();
    const auto values = palettes.value(name);
    for (int index = 0; index < keys.size(); ++index)
        theme.colors.insert(keys[index], QColor(values[index]));
    return theme;
}

QJsonObject Theme::toJson() const {
    QJsonObject result;
    for (auto color = colors.cbegin(); color != colors.cend(); ++color)
        result.insert(color.key(), color.value().name());
    QJsonObject data{{"version", 1}, {"name", name}, {"colors", result}};
    if (!sourceKind.isEmpty())
        data.insert("source", QJsonObject{{"kind", sourceKind}, {"path", sourcePath}});
    return data;
}

Theme parseTheme(const QJsonObject &data) {
    if (data.value("version").toInt(-1) != 1)
        invalid("Theme version must be 1");
    const QString name = data.value("name").toString().trimmed();
    if (name.isEmpty() || name.size() > 60)
        invalid("Theme name must contain 1–60 characters");
    if (!data.value("colors").isObject())
        invalid("Theme must contain a colors object");
    const auto colors = data.value("colors").toObject();
    const auto keys = colorKeys();
    for (auto color = colors.constBegin(); color != colors.constEnd(); ++color) {
        if (!keys.contains(color.key()))
            invalid("Unknown color: " + color.key());
    }
    static const QRegularExpression pattern("^#[0-9a-fA-F]{6}$");
    Theme theme{name, {}, {}, {}};
    for (const auto &key : keys) {
        const QString value = colors.value(key).toString();
        if (!pattern.match(value).hasMatch())
            invalid(key + ": expected a color in #RRGGBB format");
        theme.colors.insert(key, QColor(value));
    }
    if (data.contains("source")) {
        if (!data.value("source").isObject())
            invalid("Theme source must be an object");
        const auto source = data.value("source").toObject();
        theme.sourceKind = source.value("kind").toString();
        theme.sourcePath = source.value("path").toString();
        if (!QStringList{"desktop", "static", "caelestia", "noctalia", "file"}.contains(theme.sourceKind)
            || !QFileInfo(theme.sourcePath).isAbsolute() || theme.sourcePath.isEmpty())
            invalid("Theme source needs a known kind and an absolute path");
    }
    return theme;
}

Theme loadTheme(const QString &path) {
    return parseTheme(readJson(path));
}

Theme ensureTheme(const QString &path) {
    if (QFileInfo::exists(path))
        return loadTheme(path);
    const auto theme = presetTheme("forest");
    writeJson(path, theme.toJson());
    return theme;
}

Theme setColors(Theme theme, const QStringList &assignments) {
    if (assignments.isEmpty())
        invalid("Provide at least one COLOR=#RRGGBB assignment");
    auto data = theme.toJson();
    auto colors = data.value("colors").toObject();
    for (const auto &assignment : assignments) {
        const auto separator = assignment.indexOf('=');
        const auto key = assignment.left(separator);
        if (separator < 1 || !colorKeys().contains(key))
            invalid("Expected COLOR=#RRGGBB. Colors: " + colorKeys().join(", "));
        colors.insert(key, assignment.mid(separator + 1));
    }
    data.insert("name", "custom");
    data.remove("source");
    data.insert("colors", colors);
    return parseTheme(data);
}

}
