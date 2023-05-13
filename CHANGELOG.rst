Changelog
=========

All notable changes to this project will be documented in this file.

The format is based on `Keep a Changelog`_, and this project adheres to
`Semantic Versioning`_.

Unreleased
----------

Please see `Unreleased Changes`_ for more information.

5.4.0 - 2023-05-13
------------------

Added
~~~~~

- faccessat2 syscall
- Enable SonarCloud for GitHub Actions
- Include uthash v2.3.0 as submodule
- Disable mixed execution with new --mixed-mode option

Changed
~~~~~~~

- Rename test-0cf405b0.c to fix_memory_corruption_execve_proc_self_exe.c

Fixed
~~~~~

- Android compatibility with cwd
- Running test-0cf405b0 for newer versions of glibc
- Running test-25069c12 and test-25069c13 on newer kernels

5.3.1 - 2022-04-24
------------------

Changed
~~~~~~~

- Error out when trying to set PTRACE_O_TRACESECCOMP under ptrace emulation.
- Set the restart_how field in a newly created child tracee.

Removed
~~~~~~~

- Unnecessary dependency of PRoot on libarchive.
- Changelog target from doc makefile.

Fixed
~~~~~

- Incorrect year for 5.3.0 release in changelog and manual.

5.3.0 - 2022-01-04
------------------

Added
~~~~~

- Link to repository on website.

- Support for utimensat_time64 on 32bit architectures.

- Install LZOP on CI for CARE archive extraction.

- Enable GitHub Actions for testing.

- Message for stopping and starting of tracees.

- Python 3 support in tests.

- Support for statx syscall.

- Test case for sysexit handler.

Changed
~~~~~~~

- Update wording in manual regarding rootfs.

- Change restart_original_syscall to not use chained syscall.

- Access sockfd in the chained getsocketname via the original version.

- Pin Debian 8 for docker image.

- Make sure not to fake too old an kernel release.

- Ensure the stack is aligned for AArch64 and X86 for SIMD code.

- Include /bin in PATH during tests.

- Kernel version detection for kernels 5.0 and newer.

- Allow a higher initial heap size in test.

- Allow the value of AT_HWCAP to be empty.

- Do not unconditionally use PTRACE_CONT when recieving a useless SECCOMP event.

- Do not treat libarchive warnings as errors.

- canon: call bindings substitution on '/' component of user path.

Removed
~~~~~~~

- Remove special handling of syscall avoider number on ARM.

- Delete roadmap.rst file.

- Remove Travis CI configuration.

- Remove preprocessor directives and associated code.

Fixed
~~~~~

- Fchmod permissions for loader.

- Test compilation on ARM.

- Includes in tests.

- Handling of receiving seccomp after normal ptrace event.

- Waitpid on zombies.

- Extraction of wrapped file.

- Archive suffix handling.

- Improve docker test skip detection.

- Event handling on newer kernels.

- Command line handler for the python extension.

- Linking against the swig generated symbol for the python extension.

- Linking on python 3.8 and newer.

- Regression in socket name shortening.

- Test caused by shell optimization.

- Test failure due to increased shebang limit.

- Handling of fstatat on new kernels.

- Seccomp event handling logic causing sysexit events to be missed.

- fake_id0: Fix POKE_MEM_ID to call poke_uint32 instead of poke_uint16.

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

.. _Unreleased Changes: https://github.com/proot-me/proot/compare/v5.4.0...master
.. _Keep a Changelog: https://keepachangelog.com/en/1.0.0
.. _Semantic Versioning: https://semver.org/spec/v2.0.0.html
