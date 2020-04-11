Changelog
=========

All notable changes to this project will be documented in this file.

The format is based on `Keep a Changelog`_, and this project adheres to
`Semantic Versioning`_.

Unreleased
------------

Please see `Unreleased Changes`_ for more information.

5.2.0-alpha - 2020-04-14
------------------------

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

.. _Unreleased Changes: https://github.com/proot-me/proot/compare/v5.2.0-alpha...master
.. _Keep a Changelog: https://keepachangelog.com/en/1.0.0
.. _Semantic Versioning: https://semver.org/spec/v2.0.0.html
