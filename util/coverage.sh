#!/bin/sh
set -eu

# build configuration and loaders
make -C src loader.elf loader-m32.elf build.h

# compile with required flags
CFLAGS='-Wall -Werror -O0 --coverage' LDFLAGS='-ltalloc -Wl,-z,noexecstack --coverage' make -eC src proot care

# run testsuite
make -C test || true # ignore failing tests (for now)

# capture coverage data
lcov --capture --directory src --output-file coverage.info

# generate coverage report
genhtml coverage.info --output-directory gcov-latest

