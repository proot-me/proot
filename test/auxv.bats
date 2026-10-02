#!/usr/bin/env bats
#
# AT_EXECFN has to name the program PRoot runs, whichever way that
# program reads its auxiliary vector. The kernel executes PRoot's
# loader, so its own copy of the vector -- what prctl(PR_GET_AUXV) and
# /proc/self/auxv show -- names the loader's temporary file unless
# PRoot fixes it up. A multi-call binary that dispatches on that name,
# such as the Rust coreutils Ubuntu ships since 25.10, fails otherwise.

load helper


@test "AT_EXECFN names the program in getauxval(3)" {
    runp proot -b /proc -r "$ROOTFS" -w / /bin/execfn getauxval
    [ "$status" -eq 0 ]
    [ "$output" = "/bin/execfn" ]
}


@test "AT_EXECFN names the program in prctl(PR_GET_AUXV)" {
    runp proot -b /proc -r "$ROOTFS" -w / /bin/execfn prctl
    [ "$status" -ne 125 ] || skip "PR_GET_AUXV needs Linux 6.4 or later"
    [ "$status" -eq 0 ]
    [ "$output" = "/bin/execfn" ]
}


@test "AT_EXECFN names the program in /proc/self/auxv" {
    runp proot -b /proc -r "$ROOTFS" -w / /bin/execfn file
    [ "$status" -eq 0 ]
    [ "$output" = "/bin/execfn" ]
}


@test "AT_EXECFN names a 32-bit program in prctl(PR_GET_AUXV)" {
    [ -e "$ROOTFS/bin/execfn-m32" ] || skip "no 32-bit build of execfn"
    runp proot -b /proc -r "$ROOTFS" -w / /bin/execfn-m32 prctl
    [ "$status" -ne 125 ] || skip "PR_GET_AUXV needs Linux 6.4 or later"
    [ "$status" -eq 0 ]
    [ "$output" = "/bin/execfn-m32" ]
}


@test "AT_EXECFN names a 32-bit program in /proc/self/auxv" {
    [ -e "$ROOTFS/bin/execfn-m32" ] || skip "no 32-bit build of execfn"
    runp proot -b /proc -r "$ROOTFS" -w / /bin/execfn-m32 file
    [ "$status" -eq 0 ]
    [ "$output" = "/bin/execfn-m32" ]
}
