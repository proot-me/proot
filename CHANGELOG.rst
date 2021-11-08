Changelog
=========

All notable changes to this project will be documented in this file.

The format is based on `Keep a Changelog`_, and this project adheres to
`Semantic Versioning`_.

Unreleased
----------

5.3.0 - 2021-11-07
------------------

Please see `Unreleased Changes`_ for more information.

fake_id0: Fix POKE_MEM_ID to call poke_uint32 instead of poke_uint16

Fix indentation

Add a testcase.

Fix an issue in seccomp event handling logic, that could cause

Make kernel version detection work for kernels 5.0+ and fix indentation.

Remove preprocessor directives and associated code,

Pin Debian 8 for Docker

Remove Travis CI (#270)

Add support for statx syscall (#272)

Make sure /bin is in PATH during test (#274)

Fix handling of fstatat on new kernels (#275)

Fix test failure due to increased shebang limit (#276)

Fix test caused by shell optimization (#277)

Fix regression in socket name shortening (#278)

Allow a higher initial heap size in test (#279)

Python 3 support in test (#280)

Make sure the stack is aligned (#282)

Make sure not to fake too old an kernel release (#281)

Access sockfd in the chained getsocketname via the original version (#…

canon: call bindings substitution on '/' component of user path (#100)

A few fixes for the python extension (#285)

Delete roadmap.rst

Fix event handling on newer kernels (#288)

Fix and improve docker test skip detection (#289)

Enable github action for testing (#287)

Do not treat libarchive warnings as errors (#293)

Install LZOP on CI for CARE archive extraction (#296)

Fix archive suffix handling (#294)

Fix extraction of wrapped file (#295)

Fix waitpid on zombies (#291)

Change restart_original_syscall to not use chained syscall (#297)

Fix handling of receiving seccomp after normal ptrace event (#298)

Fix include in tests (#299)

Allow the value of AT_HWCAP to be empty (#305)

Remove special handling of syscall avoider number on ARM (#304)

Support utimensat_time64 on 32bit architectures (#303)

Fix test compilation on ARM (#302)

Do not unconditionally use PTRACE_CONT when recieving a useless SECCO…

Some printing tweaks (#300)

Fix fchmod permissions for loader (#268)

Add link to repository on website (#273)

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
