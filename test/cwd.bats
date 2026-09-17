#!/usr/bin/env bats
#
# Proof of concept for the Bats-based suite proposed in #439. Covers
# the same ground as proot-rs's tests/cwd.bats (-w/--cwd, then a
# chdir(2) inside the traced process), adapted to use this project's
# own test/pwd.c and test/chdir_getcwd.c instead of /bin/sh, since the
# minimal test/rootfs has no shell.

load helper


@test "test -w sets the initial working directory" {
    run proot -r "$ROOTFS" -w /bin /bin/pwd
    [ "$status" -eq 0 ]
    [ "$output" = "/bin" ]
}


@test "test chdir(2) inside the traced process" {
    run proot -r "$ROOTFS" -w / /bin/chdir_getcwd /bin
    [ "$status" -eq 0 ]
    [ "$output" = "/bin" ]
}
