#!/bin/sh
set -eu

"${PROOT}" -0 -r "${ROOTFS}" /bin/touch /empty
