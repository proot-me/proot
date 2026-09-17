#!/bin/bash

export LC_ALL=C

# The root directory of the test files.
TEST_ROOT=$(dirname "$(readlink -f "$BASH_SOURCE")")

# The root directory of this project.
PROJECT_ROOT="$TEST_ROOT/.."

# Path to the proot binary, matching the $PROOT convention already used
# by test/GNUmakefile.
if [ -z "${PROOT}" ]; then
    PROOT="$PROJECT_ROOT/src/proot"
fi

# Path to the test rootfs built by `make -C test setup`, matching the
# $ROOTFS convention already used by test/GNUmakefile.
if [ -z "${ROOTFS}" ]; then
    ROOTFS="$TEST_ROOT/rootfs"
fi

# A wrapper for bats' built-in `run` command. Prints the command,
# exit status, and output to stderr, so a failing test shows what
# actually happened instead of just "not ok".
function runp() {
    run "$@"
    echo "command: $@" >&2
    echo "status:  $status" >&2
    echo "output:  $output" >&2
}

# A wrapper function for the proot binary.
function proot() {
    "$PROOT" "$@"
}

# Compile a single C source file ($2) to a statically linked binary ($1).
function compile_c_static() {
    local target_path="$1"
    local source_path="$2"
    gcc -static -o "$target_path" "$source_path"
}

# Same as compile_c_static(), but the final binary is dynamically linked.
function compile_c_dynamic() {
    local target_path="$1"
    local source_path="$2"
    gcc -o "$target_path" "$source_path"
}

# Ensure that the command exists, or skip the test.
function check_if_command_exists() {
    command -v "$1" 1>&- 2>&- || { skip "The command \`$1\` is required but is not installed."; }
}
