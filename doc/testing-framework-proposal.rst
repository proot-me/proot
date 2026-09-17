Testing framework proposal
===========================

This document scopes out issue `#254`_, "Implement framework for
testing", which links to `libcheck`_ with no further discussion.

.. _#254: https://github.com/proot-me/proot/issues/254
.. _libcheck: https://libcheck.github.io/check

Current state
--------------

``test/`` is a black-box integration suite: it builds a real
``test/rootfs``, compiles small static C programs, and runs them
*through* the real ``proot`` binary via ``ptrace``, checking the exit
code. 156 test files, driven entirely by ``test/GNUmakefile``.

Measured directly against the current suite:

+ **60% of test files use opaque hash names**, like
  ``test-5bed7141.c`` and ``test-33333333.c``. That's the exact
  complaint in issue `#164`_.

+ **No assertion framework.** Pass/fail/skip is the process's raw exit
  code, matched by a ``case`` statement in the Makefile::

    check = case "$$?" in
        0)   echo "  CHECK  $(1) ok";;
        125) echo "  CHECK  $(1) skipped";;
        *)   echo "  CHECK  $(1) FAILED"; touch failure ;;
    esac

  Every test hand-rolls its own ``strcmp`` + ``fprintf`` + ``exit``
  boilerplate to report a failure.

+ **No structured output.** No TAP, no JUnit XML. Just plain
  ``CHECK <name> ok/FAILED/skipped`` text. CI has nothing
  machine-parseable to work with.

+ **Known-flaky failures are invisible.** ``test-chroot01``,
  ``test-socket01``, ``test-socket02``, ``test-socket03``,
  ``test-tempdire``, and ``test-bug-138`` fail consistently as of this
  writing, masked by a blanket ``allow_failure: true`` /
  ``continue-on-error: true`` on the whole job. There is no way to
  tell "this specific test is known-broken" from "something just
  regressed".

.. _#164: https://github.com/proot-me/proot/issues/164

Where libcheck fits, and where it doesn't
--------------------------------------------

libcheck is a unit-testing framework. It calls a C function
in-process and asserts on the return value. Most of what ``test/``
checks is traced-process behavior under ``ptrace``, which requires
actually spawning and running through the real ``proot`` binary.
libcheck doesn't fit that layer. But proot has a second, currently
nonexistent layer libcheck fits well: unit tests of its own internal
functions.

What proot-rs already does
-----------------------------

proot-rs, this project's Rust implementation, already solved the same
black-box-testing problem, with `Bats`_ (bats-core). Its
``tests/README.md`` documents why: they considered ShellSpec and
shUnit2 too, and picked Bats specifically for testing a CLI program.

.. _Bats: https://github.com/bats-core/bats-core

This gives them several things this proposal was going to build from
scratch:

+ **Real structured output.** Bats produces TAP natively.

+ **``skip`` as a first-class primitive**, not a job-wide
  ``allow_failure``::

    @test "test --bind with getdents64() results" {
        skip "this is an enhancement, see https://github.com/proot-me/proot-rs/issues/43"
        ...
    }

+ **A shared helper** (``tests/helper.bash``) provides a ``runp()``
  wrapper that echoes the command, exit status, and output to stderr
  on failure, directly solving the "opaque failure" problem. It also
  provides ``compile_c_static``/``compile_c_dynamic`` and
  ``check_if_command_exists`` (skip if a dependency is missing), the
  same shape as proot's own pattern rules and dependency checks, but
  reusable instead of copy-pasted per test.

+ **Tests grouped by category**, in files like ``cli.bats``,
  ``bind.bats``, ``cwd.bats``, ``execve/``, and ``multi-tracee/``,
  each holding several named ``@test`` cases, rather than one file per
  test case.

Overlap with proot's suite is real. proot-rs's ``bind.bats`` and
``cwd.bats`` cover the same ground as proot's own
``test-305ae31d.sh``/``test-22222222.sh`` (bind) and
``chdir_getcwd.c``/``test-5bed7141.c`` (cwd). Same behavior, tested
twice, in two different styles, against two different implementations
of the same tool.

Sharing is partial, not total
--------------------------------

proot-rs's CLI (``proot-rs/src/cli.rs``) currently implements only
``-r``/``--rootfs``, ``-b``/``--bind``, ``-w``/``--cwd``, and a bare
command. It has no ``-q`` (qemu), ``-v`` (verbose), ``-0``/``-i``
(id-faking), ``-k`` (kernel-release), ``-p`` (port map), and none of
proot's extensions (``care``, ``fake_id0``, ``kompat``,
``link2symlink``, ``portmap``, the python extension). Only the
common-denominator surface has a proot-rs equivalent to run the same
test against: rootfs, bind, cwd, execve/shebang handling, and
fork/clone/multi-tracee path translation. The rest of proot's suite is
proot-specific by definition, and stays that way until proot-rs
implements the corresponding feature.

Unit testing: the actual gap
-------------------------------

proot-rs has this layer already, via ``cargo test`` on its Rust unit
tests. proot(C) has zero unit-level coverage of its own internal
functions. Every check in ``test/`` goes through the full
ptrace/rootfs machinery, even for logic that doesn't need any of it.

Several of the core path-handling functions are already plain,
non-static, and dependency-free, real unit-test candidates today with
no mocking required::

    int join_paths(int number_paths, char result[PATH_MAX], ...);
    Comparison compare_paths(const char *path1, const char *path2);

This isn't hypothetical. This session's own investigation into
`#438`_ was a ``compare_paths()``/``readlink_proc()``
comparison-logic bug. Finding it meant manually tracing
``base``/``comparison`` values through a debug build under Docker,
since there was no faster way to check whether
``compare_paths("/proc", "/proc")`` returns what it should than a
full end-to-end ptrace run. A unit test calling ``compare_paths()``
directly, or ``readlink_proc()`` with a crafted ``base``, would have
answered that in milliseconds. It would have also caught the #384
regression at the exact point the two paths were compared, instead of
needing a full local test-suite run to notice 7 tests broke.

.. _#438: https://github.com/proot-me/proot/pull/438

libcheck fits here for a reason beyond being the issue's own
suggestion. It isolates each test in a forked child process by
default, so a function under test that hits one of this codebase's
many ``assert()`` calls aborts that test, not the whole suite. That's
exactly the failure mode ``readlink_proc()``'s assertion crash in
#438 was. It's available as the Debian/Ubuntu ``check`` package, the
same kind of lightweight apt-installable dependency proot already has
(``libtalloc-dev``, ``libarchive-dev``, ...).

Not every internal function is this easy to reach in isolation.
Anything taking a ``Tracee *`` and touching its fields needs at least
a minimal stub struct, sometimes more. Scoping which functions are
worth unit-testing, starting with the pure ones above, is separate
work from picking the framework.

Proposal
--------

1. **Adopt Bats** for proot's black-box suite instead of building a
   bespoke assertion header/TAP wrapper. proot-rs already validated
   the choice, documented the alternatives it ruled out, and a shared
   framework is a prerequisite for (4) below.

2. **Port a small number of existing tests as a proof of concept**
   first, not a big-bang rewrite. ``cwd.bats`` and ``bind.bats`` have
   the most direct 1:1 overlap with proot's own cwd/bind tests and are
   small enough to validate the approach before committing to it
   project-wide.

3. **Rename hash-named tests descriptively** as part of the same
   pass, folding in `#164`_, since grouping tests into Bats files by
   category is the natural point to also give each case a real name.

4. **Parameterize the common-denominator tests by binary**, with
   ``$PROOT`` pointing at either implementation, mirroring proot-rs's
   own ``PROOT_RS`` override, so the same Bats file can run as a
   conformance check against both proot and proot-rs. That turns
   "these two projects should behave the same" from an assumption
   into something CI actually verifies.

5. **Add libcheck as a second, separate test binary** for proot's
   internal functions, starting with the already-pure candidates
   (``compare_paths()``, ``join_paths()``). This is a second layer
   alongside (1), not a replacement, catching a different class of
   bug faster.

Non-goals
---------

+ Converting the black-box/``ptrace`` suite into unit tests. Most of
  what it checks can't be observed without a real traced process;
  unit tests are a second layer alongside it, not a replacement.

+ Fixing the 6 currently-failing tests as part of this effort. They
  should be triaged separately; this proposal only makes their status
  visible instead of silently tolerated.

+ Migrating proot-specific tests, like the extensions or
  ``-p``/``-k``/``-q``, to Bats in this pass. Those have no proot-rs
  counterpart to share with, so there's no shared-framework benefit
  driving that work yet. They can move later on their own merits, if
  any.
