#!/usr/bin/env bash
set -euo pipefail

build_directory="$1"
installer="$2"
test_directory="$(/usr/bin/mktemp -d /tmp/sudoku-install.XXXXXX)"
install_prefix="$test_directory/local"
config_directory="$test_directory/config"
apps_list="$test_directory/apps.list"
selector="$test_directory/selector"
/usr/bin/mkdir -p "$selector"
printf '%s\n' 'Singleton {' 'property color background: "#111111"' \
    'property color foreground: "#eeeeee"' 'property color primary: "#123456"' '}' > "$selector/Colors.qml"
printf '%s\n' 'example.desktop|example|Example' > "$apps_list"
arguments=(--build-dir "$build_directory" --prefix "$install_prefix" --config-dir "$config_directory" --apps-list "$apps_list" --selector "$selector")
/usr/bin/bash "$installer" "${arguments[@]}" --dry-run >/dev/null
test ! -e "$install_prefix"
test ! -e "$config_directory"
test "$(/usr/bin/wc -l < "$apps_list")" == 1
/usr/bin/bash "$installer" "${arguments[@]}" >/dev/null
test -x "$install_prefix/bin/sudoku"
test -f "$install_prefix/bin/applications/io.github.quiet_sudoku.desktop"
test -L "$install_prefix/share/applications/io.github.quiet_sudoku.desktop"
test -f "$install_prefix/share/icons/hicolor/scalable/apps/io.github.quiet_sudoku.svg"
/usr/bin/grep -Fqx "Exec=\"$install_prefix/bin/sudoku\" --config-dir \"$config_directory\"" \
    "$install_prefix/share/applications/io.github.quiet_sudoku.desktop"
if [[ -x /usr/bin/desktop-file-validate ]]; then
    /usr/bin/desktop-file-validate "$install_prefix/share/applications/io.github.quiet_sudoku.desktop"
fi
test "$(/usr/bin/wc -l < "$apps_list")" == 2
test "$(/usr/bin/head -n 1 "$apps_list")" == 'example.desktop|example|Example'
"$install_prefix/bin/sudoku" --config-dir "$config_directory" theme show > "$test_directory/theme.json"
/usr/bin/grep -Fq '"kind": "desktop"' "$test_directory/theme.json"
/usr/bin/grep -Fq '"accent": "#123456"' "$test_directory/theme.json"
/usr/bin/bash "$installer" "${arguments[@]}" >/dev/null
test "$(/usr/bin/wc -l < "$apps_list")" == 2
"$install_prefix/bin/sudoku" --config-dir "$config_directory" theme follow desktop "$selector" --dry-run > "$test_directory/preview.json"
/usr/bin/cmp "$config_directory/theme.json" "$test_directory/preview.json"
