How to make a release of PRoot?
===============================

This document summarizes checks that must be performed before
releasing PRoot or CARE, and the steps to actually publish a release.

Checks
------

+ Sanity checks:

  * All supported atchitectures and distributions
    both with and without seccomp support enabled:

      make -C test
      make -C test memcheck
      CFLAGS=-fsanitize=address LDFLAGS=-lasan
      make -C test V=1 2>&1 | grep talloc

+ Functional checks:

  * No regressions must appear with respect to :code:`test/validation.mk`
    and to the configurations tested in the previous
    release (:code:`git tag -l`).

+ Performance checks:

  * The following command must not suffer from
    unexpected performance regression::

      time proot -R / perl -e 'system("/usr/bin/true") for (1..10000)'

    where :code:`/usr/bin/true` is a symlink to :code:`/bin/true`.

+ Static analysis: :code:`gcov`/:code:`lcov` and clang :code:`scan-build`
  must not report new issues. All shell scripts must pass :code:`shellcheck`.
  The :code:`pull-request` and :code:`CodeQL` GitHub Actions workflows and
  the SonarCloud quality gate must be green on the release branch.

Version Bump
------------

The release version is recorded in three places, and all three must
agree before publishing (the ``release`` GitHub Actions workflow
enforces the first two automatically, see `Publishing the Release`_
below, but it can't catch a missed changelog entry):

1. :code:`doc/proot/manual.rst`: update the :code:`:Date:` and
   :code:`:Version:` fields.

2. :code:`src/cli/proot.h`: update the :code:`#define VERSION "..."`
   fallback to match. This file is nominally "automatically generated
   from the documentation", but in practice this line is still hand
   edited to match step 1 -- there's no build step that regenerates
   and copies it over automatically yet.

3. :code:`CHANGELOG.rst` (top level, *not* :code:`doc/changelog.rst`):
   move the relevant entries out of ``Unreleased`` into a new
   dated section for the release, following `Keep a Changelog`_.

If CARE changed since its last release, its own version needs the
same treatment in :code:`doc/care/manual.rst` and :code:`src/cli/care.h`;
CARE and PRoot are versioned independently.

Commit these as a single "prepare for release" commit/PR and get it
merged to :code:`master` before tagging.

.. _Keep a Changelog: https://keepachangelog.com/en/1.0.0

Publishing the Release
-----------------------

Once the version-bump PR is merged to :code:`master`:

1. On GitHub, draft a new release targeting :code:`master`, creating a
   new tag :code:`vX.Y.Z` (matching the :code:`:Version:` from step 1
   above, with a ``v`` prefix).

2. Publish it. Publishing (not just creating the tag) is what triggers
   the :code:`release` GitHub Actions workflow
   (:code:`.github/workflows/release.yml`).

3. The workflow will:

   * fail immediately if the tag doesn't match the version recorded in
     :code:`doc/proot/manual.rst` or :code:`src/cli/proot.h` -- this is
     the automated check for the version-bump step above;
   * build static :code:`proot` and :code:`care` binaries, equivalent to::

       make -C src clean loader.elf loader-m32.elf build.h
       LDFLAGS="${LDFLAGS} -static" make -C src proot care

   * verify the built :code:`proot --version` output actually reports
     the tagged version;
   * attach :code:`proot`, :code:`care`, and a :code:`SHA256SUMS` file
     to the GitHub release as downloadable assets.

4. Check the workflow run and confirm the assets show up on the
   release page before announcing anything.

Website
-------

Regenerating the documentation website is separate from the binary
release above::

    make -C doc
    SITE_DIR=../../proot-me.github.io
    make -eC doc dist # relative to doc directory
