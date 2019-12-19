#!/bin/sh
set -eu

if [ -z "$(command -v mcookie)" ] || \
   [ -z "$(command -v mkdir)" ] || \
   [ -z "$(command -v test)" ] || \
   [ -z "$(command -v grep)" ] || \
   [ -z "$(command -v rm)" ]; then
    exit 125;
fi

TMP="/tmp/$(mcookie)"
mkdir "${TMP}"

# shellcheck disable=SC2230
"${PROOT}" -b "$(which true):${TMP}/true" "$(which true)"

# shellcheck disable=SC2086
[ ! "$(test -e ${TMP}/true)" = "0" ]

rm -fr "${TMP}"
