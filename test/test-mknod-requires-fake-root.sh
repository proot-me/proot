#!/bin/sh
# shellcheck disable=SC2086
set -eu

if [ -z "$(command -v mknod)" ] || \
   [ "$(id -u)" -eq 0 ]; then
    exit 125
fi

TMP="/tmp/$(mcookie)"

[ ! "$(${PROOT} mknod ${TMP} b 1 1)" = "0" ]

[ ! "$(${PROOT} -i 123:456 mknod ${TMP} b 1 1)" = "0" ]

"${PROOT}" -0 mknod "${TMP}" b 1 1
