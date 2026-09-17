#!/usr/bin/env bats
#
# Proof of concept for the Bats-based suite proposed in #439. Ported
# from proot-rs's tests/bind.bats: same behavior, same tool, two
# implementations.

load helper


@test "test bind dir to dir" {
    # bind /etc to /home
    proot -b "/etc:/home" /bin/sh -c "diff /etc /home"
}


@test "test bind file to file" {
    # bind /etc/group to /etc/passwd
    proot -b "/etc/group:/etc/passwd" /bin/sh -c "diff /etc/group /etc/passwd"
}


@test "test bind dir to file" {
    # bind /home to /etc/passwd. This may seem odd, but it is allowed.
    proot -b "/home:/etc/passwd" /bin/sh -c "diff /home /etc/passwd"
}


@test "test bind file to dir" {
    # bind /etc/passwd to /home. This may seem odd, but it is allowed.
    proot -b "/etc/passwd:/home" /bin/sh -c "diff /etc/passwd /home"
}
