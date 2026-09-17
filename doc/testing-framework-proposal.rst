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

+ **60% of test files use opaque hash names**
  (``test-5bed7141.c``, ``test-33333333.c``), the exact complaint in
  issue `#164`_.

+ **No assertion framework.** Pass/fail/skip is the process's raw exit
  code, matched by a ``case`` statement in the Makefile::

    check = case "$$?" in
        0)   echo "  CHECK  $(1) ok";;
        125) echo "  CHECK  $(1) skipped";;
        *)   echo "  CHECK  $(1) FAILED"; touch failure ;;
    esac

  Every test hand-rolls its own ``strcmp`` + ``fprintf`` + ``exit``
  boilerplate to report a failure.

+ **No structured output.** No TAP, no JUnit XML -- just
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

Why libcheck doesn't fit
--------------------------

libcheck is a unit-testing framework: call a C function in-process,
assert on its return value. proot's tests aren't testing C functions,
they're testing traced-process behavior under ``ptrace``, which
requires actually spawning and running through the real ``proot``
binary. Adopting libcheck as-is doesn't fit this suite's shape.

What proot-rs already does
-----------------------------

proot-rs (this project's Rust implementation) solved the same
black-box-testing problem already, with `Bats`_ (bats-core), and
documented why in ``tests/README.md``: they considered ShellSpec and
shUnit2 too, and picked Bats specifically for testing a CLI program.

.. _Bats: https://github.com/bats-core/bats-core

Concretely, this gives them several things this proposal was going to
build from scratch:

+ **Real structured output.** Bats produces TAP natively.

+ **``skip`` as a first-class primitive**, not a job-wide
  ``allow_failure``::

    @test "test --bind with getdents64() results" {
        skip "this is an enhancement, see https://github.com/proot-me/proot-rs/issues/43"
        ...
    }

+ **A shared helper** (``tests/helper.bash``) providing a ``runp()``
  wrapper that echoes the command, exit status, and output to stderr
  on failure -- directly solving the "opaque failure" problem --
  plus ``compile_c_static``/``compile_c_dynamic`` and
  ``check_if_command_exists`` (skip if a dependency is missing), the
  same shape as proot's own pattern rules and dependency checks, just
  reusable instead of copy-pasted per test.

+ **Tests grouped by category** (``cli.bats``, ``bind.bats``,
  ``cwd.bats``, ``execve/``, ``multi-tracee/``), each holding several
  named ``@test`` cases, rather than one file per test case.

Overlap with proot's suite is real: proot-rs's ``bind.bats`` and
``cwd.bats`` cover the same ground as proot's own
``test-305ae31d.sh``/``test-22222222.sh`` (bind) and
``chdir_getcwd.c``/``test-5bed7141.c`` (cwd) -- same behavior, tested
twice, in two different styles, against two different implementations
of the same tool.

Sharing is partial, not total
--------------------------------

proot-rs's CLI (``proot-rs/src/cli.rs``) currently implements only
``-r``/``--rootfs``, ``-b``/``--bind``, ``-w``/``--cwd``, and a bare
command -- no ``-q`` (qemu), ``-v`` (verbose), ``-0``/``-i``
(id-faking), ``-k`` (kernel-release), ``-p`` (port map), and none of
proot's extensions (``care``, ``fake_id0``, ``kompat``,
``link2symlink``, ``portmap``, the python extension). Only the
common-denominator surface -- rootfs, bind, cwd, execve/shebang
handling, fork/clone/multi-tracee path translation -- has a
proot-rs equivalent to run the same test against. The rest of
proot's suite is proot-specific by definition, and stays that way
until proot-rs implements the corresponding feature.

Proposal
--------

1. **Adopt Bats** for proot's black-box suite instead of building a
   bespoke assertion header/TAP wrapper -- proot-rs already validated
   the choice, documented the alternatives it ruled out, and a shared
   framework is a prerequisite for (4) below.

2. **Port a small number of existing tests as a proof of concept**
   first, not a big-bang rewrite -- ``cwd.bats`` and ``bind.bats``
   have the most direct 1:1 overlap with proot's own cwd/bind tests
   and are small enough to validate the approach before committing to
   it project-wide.

3. **Rename hash-named tests descriptively** as part of the same
   pass, folding in `#164`_, since grouping tests into Bats files by
   category is the natural point to also give each case a real name.

4. **Parameterize the common-denominator tests by binary** (``$PROOT``
   pointing at either implementation, mirroring proot-rs's own
   ``PROOT_RS`` override), so the same Bats file can run as a
   conformance check against both proot and proot-rs -- turning
   "these two projects should behave the same" from an assumption into
   something CI actually verifies.

Non-goals
---------

+ Replacing the black-box/``ptrace`` execution model with in-process
  unit tests -- most of what this suite checks can't be observed
  without a real traced process.

+ Fixing the 6 currently-failing tests as part of this effort. They
  should be triaged separately; this proposal only makes their status
  visible instead of silently tolerated.

+ Migrating proot-specific tests (extensions, ``-p``/``-k``/``-q``,
  etc.) to Bats in this pass -- those have no proot-rs counterpart to
  share with, so there's no shared-framework benefit driving that work
  yet. They can move later on their own merits, if any.
