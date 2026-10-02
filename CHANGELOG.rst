Changelog
=========

All notable changes to this project will be documented in this file.

The format is based on `Keep a Changelog`_, and this project adheres to
`Semantic Versioning`_.

Unreleased
----------

Please see `Unreleased Changes`_ for more information.

5.5.0 - 2026-10-02
------------------

Added
~~~~~

- fchmodat2 syscall support (#408)
- A Bats-based black-box test suite (``bind.bats``, ``cwd.bats``,
  ``auxv.bats``, ``readlink.bats``, ``no-new-privs.bats``), parameterized
  to run the same ``bind``/``cwd`` cases as a conformance check against
  both proot and proot-rs
- A libcheck-based unit test layer for internal functions such as
  ``compare_paths()`` and ``join_paths()``
- A shellcheck CI job covering every changed shell script
- Docker test images for every supported distro, published to the
  GitHub Container Registry
- CI coverage for Ubuntu 26.04 and an aarch64 cross-compile smoke test

Changed
~~~~~~~

- Renamed the 122 hash-named test files (``test-5bed7141.c``,
  ``test-33333333.c``, etc.) to names that describe what they actually
  test (#164)
- GitLab CI updated to a current gcc image, with pass/fail behavior
  brought back in line with GitHub Actions

Fixed
~~~~~

- ``errno`` not cleared before ``PTRACE_PEEKDATA``, which could
  misreport a valid ``-1`` result as an error
- ``readlink(2)`` crashing on a top-level ``/proc`` entry (#438)
- ``readlink(2)`` silently truncating and reporting a wrong guest path
  when the host-side target is longer than the caller's buffer
- ``AT_EXECFN`` naming the loader instead of the traced program, which
  broke Rust coreutils (uutils) under PRoot
- ``PR_GET_NO_NEW_PRIVS`` reporting PRoot's own flag instead of the
  traced program's, which broke ``sudo-rs``
- The ``-P`` python extension failing to load; ``WITHOUT_PYTHON`` not
  fully disabling it in some build configurations
- ``pkg-config`` and ``ld`` not respecting ``CROSS_COMPILE``
- A missing ``basename`` include breaking the build against glibc's
  ``libgen.h`` (#378)
- Python 2's ``imp`` import, incompatible with Python 3 (#398)
- Outdated documentation links

5.4.1 - 2026-09-07
------------------

Added
~~~~~

- clone3 syscall support
- CodeQL static analysis workflow and Dependabot version tracking

Changed
~~~~~~~

- Reformatted all sources with indent -kr
- Modernized Docker test images: replaced EOL centos/debian bases, moved
  built images to ghcr.io, dropped to a non-root build user
- Migrated SonarCloud scanning to its automatic PR analysis and pinned
  GitHub Actions to commit SHA hashes

Fixed
~~~~~

- readlinkat(2) with an empty pathname on a dirfd opened with
  O_PATH|O_NOFOLLOW no longer aborts the tracer (#182)
- Broken Ubuntu rootfs link in the docs
- Assorted CodeQL/SonarCloud findings: unchecked write_data() return in
  the portmap extension, deprecated bzero, an invalid %z format
  specifier, and stale comments
- Static release build failing to link proot/care due to the Python
  extension and libarchive's transitive static dependencies

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

.. _Unreleased Changes: https://github.com/proot-me/proot/compare/v5.5.0...master
.. _Keep a Changelog: https://keepachangelog.com/en/1.0.0
.. _Semantic Versioning: https://semver.org/spec/v2.0.0.html
