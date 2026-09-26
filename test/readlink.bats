#!/usr/bin/env bats
#
# readlink(2) of a guest path whose host path is longer. The kernel
# cuts the host path to the caller's buffer, and callers that start
# with a small buffer (libglnx starts at 100 bytes) grow it only when
# readlink(2) fills it, so what PRoot reports has to be either the
# whole guest path or a full buffer.

load helper


@test "readlink(2) with a short buffer reports the whole guest path" {
    host="$BATS_TEST_TMPDIR/bound"
    name=file-whose-host-path-overflows-a-64-byte-buffer
    mkdir -p "$host"
    touch "$host/$name"
    runp proot -b /proc -b "$host:/b" -r "$ROOTFS" -w / /bin/readlink-fd "/b/$name" 64
    [ "$status" -eq 0 ]
    [ "$output" = "/b/$name" ]
}
