#!/bin/sh

# Skip test when run on Travis, as it doesn't support IPv6
if [ "${TRAVIS}" ]; then
    exit 125;
fi

cd sockets || exit 125

sh testtcpsocketipv6.sh
