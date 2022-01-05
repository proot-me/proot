Changelog
=========

All notable changes to this project will be documented in this file.

The format is based on `Keep a Changelog`_, and this project adheres to
`Semantic Versioning`_.

Unreleased
----------

Please see `Unreleased Changes`_ for more information.

5.3.0 - 2021-11-07
------------------

Added
~~~~~

Changed
~~~~~~~

Removed
~~~~~~~

Fixed
~~~~~

fake_id0: Fix POKE_MEM_ID to call poke_uint32 instead of poke_uint16

Fix indentation

Add a testcase.

Fix an issue in seccomp event handling logic, that could cause

Make kernel version detection work for kernels 5.0+ and fix indentation.

Remove preprocessor directives and associated code,

Pin Debian 8 for Docker

Remove Travis CI

Add support for statx syscall

Make sure /bin is in PATH during test

Fix handling of fstatat on new kernels

Fix test failure due to increased shebang limit

Fix test caused by shell optimization

Fix regression in socket name shortening

Allow a higher initial heap size in test

Python 3 support in test

Make sure the stack is aligned

Make sure not to fake too old an kernel release

Access sockfd in the chained getsocketname via the original version

canon: call bindings substitution on '/' component of user path

A few fixes for the python extension

Delete roadmap.rst

Fix event handling on newer kernels

Fix and improve docker test skip detection

Enable github action for testing

Do not treat libarchive warnings as errors

Install LZOP on CI for CARE archive extraction

Fix archive suffix handling

Fix extraction of wrapped file

Fix waitpid on zombies

Change restart_original_syscall to not use chained syscall

Fix handling of receiving seccomp after normal ptrace event

Fix include in tests

Allow the value of AT_HWCAP to be empty

Remove special handling of syscall avoider number on ARM

Support utimensat_time64 on 32bit architectures

Fix test compilation on ARM

Do not unconditionally use PTRACE_CONT when recieving a useless SECCO…

Some printing tweaks

Fix fchmod permissions for loader

Add link to repository on website

Update wording in manual regarding rootfs

5.2.0 - 2021-09-01
------------------

Added
~~~~~

-  GitLab CI/CD pipelines for static binaries.

-  Python extension.

-  Secure disclosure instructions.

-  Vagrantfiles for kernel-specific testing.

-  Support for Musl libc.

-  Use shellcheck for scripts.

-  link2symlink extension.

-  Contributor scripts care2docker.sh, and care_rearchiver.sh

-  Clang scan-build and gcov/lcov for source code analysis.

-  Trivial chroot using relative paths.

-  port_mapper extension.

-  Commandline option --kill-on-exit.

-  Hidden PROOT_TMPDIR option.

-  Support for sudo via fake_id0 extension.

Changed
~~~~~~~

-  Started using top-level changelog instead of individual ones.

-  Limit testsuite to five minutes.

-  Updated release instructions.

-  Renamed tests to test.

-  Replace .exe file extension with .elf for loader binaries.

-  Use LC_ALL instead of LANG.

-  Semantics for HOST_PATH extension event arguments.

Removed
~~~~~~~

-  Disabled, deprecated, or unreliable tests.

-  Drop Coverity from Travis CI.

-  Cross-compiling scripts for Slackware.

-  FHS assumptions from tests.

-  References to proot.me domain.

Fixed
~~~~~

-  Error-code handling in substitute_binding_stat.

-  Prevent tracees from becoming undumpable.

-  Merged patches for detecting kernels >= 4.8.

-  GIT_VERSION for development binaries.

-  Replace mktemp with mkstemp.

-  File permissions for test scripts.

-  Filter renamteat2 syscall.

-  Honor GNU standards regarding DESTDIR variable.

-  Cleanup tmp on non-ext file systems.

-  Reallocation of heap for CLONE_VM on execve syscall.

-  Non-executable stack for binaries.

.. _Unreleased Changes: https://github.com/proot-me/proot/compare/v5.2.0...master
.. _Keep a Changelog: https://keepachangelog.com/en/1.0.0
.. _Semantic Versioning: https://semver.org/spec/v2.0.0.html
