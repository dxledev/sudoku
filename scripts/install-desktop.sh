#!/usr/bin/env bash
set -euo pipefail

project_directory="$(cd -- "${BASH_SOURCE[0]%/*}/.." && pwd)"
build_directory="$project_directory/build"
install_prefix="$HOME/.local"
config_directory="${XDG_CONFIG_HOME:-$HOME/.config}/sudoku"
apps_list="${XDG_CONFIG_HOME:-$HOME/.config}/apps.list"
selector="${XDG_CONFIG_HOME:-$HOME/.config}/themes/.caelestia-use"
dry_run=0
desktop_id=io.github.quiet_sudoku

usage() {
    printf '%s\n' 'Usage: install-desktop.sh [--dry-run] [--build-dir DIR] [--prefix DIR]' \
        '       [--config-dir DIR] [--apps-list FILE] [--selector DIR]'
}

fail() {
    printf 'install-desktop: %s\n' "$*" >&2
    exit 1
}

parse_options() {
    while (( $# )); do
        case "$1" in
            --dry-run) dry_run=1; shift ;;
            --help|-h) usage; exit 0 ;;
            --build-dir|--prefix|--config-dir|--apps-list|--selector)
                (( $# >= 2 )) || fail "Missing value for $1"
                [[ "$2" == /* && "$2" != *$'\n'* && "$2" != *'|'* && "$2" != *'"'* && "$2" != *'%'* && "$2" != *'\\'* ]] || fail "Expected an absolute path for $1"
                case "$1" in
                    --build-dir) build_directory="${2%/}" ;;
                    --prefix) install_prefix="${2%/}" ;;
                    --config-dir) config_directory="${2%/}" ;;
                    --apps-list) apps_list="$2" ;;
                    --selector) selector="${2%/}" ;;
                esac
                shift 2 ;;
            *) fail "Unknown option: $1" ;;
        esac
    done
}

validate_installation() {
    [[ -x "$build_directory/sudoku" ]] || fail "Build the game first: $build_directory/sudoku"
    [[ "$install_prefix" != / && -n "$install_prefix" ]] || fail 'Invalid installation prefix'
    [[ ! -e "$apps_list" || -f "$apps_list" ]] || fail "App list is not a regular file: $apps_list"
    [[ ! -L "$apps_list" ]] || fail "Refusing to replace a symlinked app list: $apps_list"
    [[ ! -L "$install_prefix/bin/sudoku" ]] || fail 'Refusing to replace a symlinked executable'
    local registered="$install_prefix/share/applications/$desktop_id.desktop"
    local desktop="$install_prefix/bin/applications/$desktop_id.desktop"
    [[ ! -e "$registered" && ! -L "$registered" || -L "$registered" && "$(/usr/bin/readlink -- "$registered")" == "$desktop" ]] ||
        fail "Existing desktop registration is unmanaged: $registered"
    [[ ! -L "$desktop" ]] || fail "Refusing to replace a symlinked desktop entry: $desktop"
    "$build_directory/sudoku" --config-dir "$config_directory" theme follow desktop "$selector" --dry-run >/dev/null
}

backup_file() {
    [[ -f "$1" ]] || return 0
    /usr/bin/cp -p -- "$1" "$1.sudoku-backup.$(/usr/bin/date +%Y%m%dT%H%M%S).$$"
}

install_files() {
    local desktop="$install_prefix/bin/applications/$desktop_id.desktop"
    local registered="$install_prefix/share/applications/$desktop_id.desktop"
    /usr/bin/mkdir -p -- "$install_prefix/bin"
    local executable_temporary
    executable_temporary="$(/usr/bin/mktemp "$install_prefix/bin/.sudoku-binary.XXXXXX")"
    /usr/bin/install -m755 -- "$build_directory/sudoku" "$executable_temporary"
    /usr/bin/mv -T -- "$executable_temporary" "$install_prefix/bin/sudoku"
    /usr/bin/install -Dm644 -- "$project_directory/assets/$desktop_id.svg" \
        "$install_prefix/share/icons/hicolor/scalable/apps/$desktop_id.svg"
    /usr/bin/mkdir -p -- "${desktop%/*}" "${registered%/*}"
    backup_file "$desktop"
    local temporary
    temporary="$(/usr/bin/mktemp "${desktop%/*}/.sudoku-desktop.XXXXXX")"
    /usr/bin/awk -v executable="$install_prefix/bin/sudoku" -v config="$config_directory" \
        '/^Exec=/ { print "Exec=\"" executable "\" --config-dir \"" config "\""; next } { print }' \
        "$project_directory/assets/$desktop_id.desktop" > "$temporary"
    /usr/bin/chmod 644 -- "$temporary"
    /usr/bin/mv -T -- "$temporary" "$desktop"
    [[ -L "$registered" ]] || /usr/bin/ln -s -- "$desktop" "$registered"
    if [[ -x /usr/bin/update-desktop-database ]]; then
        /usr/bin/update-desktop-database "$install_prefix/share/applications"
    fi
}

rename_app_label() {
    /usr/bin/awk -F '|' -v id="$desktop_id.desktop" \
        '$1 == id && $3 == "Quiet Sudoku" { found = 1 } END { exit !found }' "$apps_list" || return 0
    backup_file "$apps_list"
    local temporary
    temporary="$(/usr/bin/mktemp "${apps_list%/*}/.sudoku-apps.XXXXXX")"
    /usr/bin/awk -F '|' -v OFS='|' -v id="$desktop_id.desktop" \
        '$1 == id && $3 == "Quiet Sudoku" { $3 = "Sudoku" } { print }' "$apps_list" > "$temporary"
    /usr/bin/chmod --reference="$apps_list" "$temporary"
    /usr/bin/mv -T -- "$temporary" "$apps_list"
}

register_app() {
    local entry="$desktop_id.desktop|$install_prefix/share/icons/hicolor/scalable/apps/$desktop_id.svg|Sudoku"
    if [[ -f "$apps_list" ]] && /usr/bin/awk -F '|' -v id="$desktop_id.desktop" \
        '$1 == id { found = 1 } END { exit !found }' "$apps_list"; then
        rename_app_label
        return
    fi
    /usr/bin/mkdir -p -- "${apps_list%/*}"
    backup_file "$apps_list"
    local temporary
    temporary="$(/usr/bin/mktemp "${apps_list%/*}/.sudoku-apps.XXXXXX")"
    if [[ -f "$apps_list" ]]; then
        /usr/bin/awk '{ print }' "$apps_list" > "$temporary"
        /usr/bin/chmod --reference="$apps_list" "$temporary"
    fi
    printf '%s\n' "$entry" >> "$temporary"
    /usr/bin/mv -T -- "$temporary" "$apps_list"
}

configure_theme() {
    backup_file "$config_directory/theme.json"
    "$install_prefix/bin/sudoku" --config-dir "$config_directory" theme follow desktop "$selector" >/dev/null
}

main() {
    parse_options "$@"
    validate_installation
    if (( dry_run )); then
        printf 'Install executable: %s/bin/sudoku\nDesktop entry: %s/bin/applications/%s.desktop\nRegister desktop: %s/share/applications/%s.desktop\nApp list: %s\nFollow palette: %s/Colors.qml\nSave theme: %s/theme.json\n' \
            "$install_prefix" "$install_prefix" "$desktop_id" "$install_prefix" "$desktop_id" \
            "$apps_list" "$selector" "$config_directory"
        return
    fi
    install_files
    register_app
    configure_theme
    printf 'Installed Sudoku; following the active desktop palette.\n'
}

main "$@"
