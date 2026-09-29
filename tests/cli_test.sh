#!/usr/bin/env bash
set -euo pipefail

sudoku_binary="$1"
test_directory="$(mktemp -d /tmp/sudoku-cli.XXXXXX)"
config_directory="${test_directory}/config"

"${sudoku_binary}" --config-dir "${config_directory}" theme preset paper --dry-run > "${test_directory}/preview.json"
test ! -e "${config_directory}"
"${sudoku_binary}" --config-dir "${config_directory}" theme path > "${test_directory}/path.txt"
test ! -e "${config_directory}"
"${sudoku_binary}" --config-dir "${config_directory}" theme preset forest > /dev/null
"${sudoku_binary}" --config-dir "${config_directory}" theme set 'accent=#123456' > /dev/null
"${sudoku_binary}" --config-dir "${config_directory}" theme export "${test_directory}/export.json" > /dev/null
cmp "${config_directory}/theme.json" "${test_directory}/export.json"
if "${sudoku_binary}" --config-dir "${config_directory}" theme set 'accent=invalid' 2> "${test_directory}/error.txt"; then
    exit 1
fi
cmp "${config_directory}/theme.json" "${test_directory}/export.json"
"${sudoku_binary}" --config-dir "${config_directory}" theme preset rose > /dev/null
"${sudoku_binary}" --config-dir "${config_directory}" theme import "${test_directory}/export.json" > /dev/null
cmp "${config_directory}/theme.json" "${test_directory}/export.json"
"${sudoku_binary}" --help > /dev/null
"${sudoku_binary}" --version > /dev/null
