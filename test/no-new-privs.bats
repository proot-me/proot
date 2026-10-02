#!/usr/bin/env bats
#
# PRoot sets the no_new_privs flag itself to install its seccomp
# filter, so the kernel's answer to prctl(PR_GET_NO_NEW_PRIVS) is
# PRoot's. A program has to see what it would see without PRoot: the
# flag it inherited, or the one it set itself.

load helper


@test "PR_GET_NO_NEW_PRIVS reports the flag the program inherited" {
    expected="$("$ROOTFS/bin/no-new-privs")"
    runp proot -r "$ROOTFS" -w / /bin/no-new-privs
    [ "$status" -eq 0 ]
    [ "$output" = "$expected" ]
}


@test "PR_GET_NO_NEW_PRIVS reports the flag the program set, in its children and after execve(2)" {
    runp proot -r "$ROOTFS" -w / /bin/no-new-privs set
    [ "$status" -eq 0 ]
    [ "$output" = "$(printf '1\n1\n1')" ]
}
