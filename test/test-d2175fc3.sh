#!/bin/sh

if [ ! -x  "${ROOTFS}/bin/readlink" ] || [ -z "$(command -v grep)" ]; then
    exit 125;
fi

"${PROOT}" -r "${ROOTFS}" /bin/readlink /bin/abs-true | grep "$(command -v true)"
"${PROOT}" -r "${ROOTFS}" /bin/readlink /bin/rel-true | grep '^\./true$'

"${PROOT}" -b /:/host-rootfs -r "${ROOTFS}" /bin/readlink /bin/abs-true | grep "$(command -v true)"
"${PROOT}" -b /:/host-rootfs -r "${ROOTFS}" /bin/readlink /bin/rel-true | grep '^./true$'
