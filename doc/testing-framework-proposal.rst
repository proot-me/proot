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

Proposal
--------

Three separable, incremental pieces:

1. **A small local assertion-helper header**, not a full framework, to
   cut the repetitive ``strcmp`` + ``fprintf`` + ``exit`` boilerplate
   every test currently hand-rolls.

2. **Structured (TAP-style) output** from the test runner, so CI gets
   per-test pass/fail/skip instead of grepping text, and known-flaky
   tests can be marked explicitly instead of hidden behind a job-wide
   ``allow_failure``.

3. **Rename hash-named tests descriptively**, folding in `#164`_, since
   touching every test file for (1)/(2) is the natural point to also
   give it a real name.

Non-goals
---------

+ Replacing the black-box/``ptrace`` execution model with in-process
  unit tests -- most of what this suite checks can't be observed
  without a real traced process.

+ Fixing the 6 currently-failing tests as part of this effort. They
  should be triaged separately; this proposal only makes their status
  visible instead of silently tolerated.
