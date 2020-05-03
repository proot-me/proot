How to make a release of PRoot?
===============================

This document summarizes checks that must be performed before
releasing PRoot or CARE.

Checks
------

+ Sanity checks:

  * on ARM and *all* OBS distros (x86 and x86_64): `make -C test`

  * on x86_64, with *and* without seccomp:

    - :code:`make -C test` on *all* OBS distros
    - :code:`make -C test memcheck`
    - :code:`CFLAGS=-fsanitize=address LDFLAGS=-lasan`
    - :code:`make -C test V=1 2>&1 | grep talloc`

+ Functional checks: no regressions must appear with respect to
  :code:`test/validation.mk` and to the configurations tested in the previous
  release (:code:`git tag -l`).

+ Performance checks: the following command must not suffer from
  unexpected performance regression::

    time proot -R / perl -e 'system("/usr/bin/true") for (1..10000)'

  where :code:`/usr/bin/true` is a symlink to :code:`/bin/true`.

+ Static analysis: Gcov/Lcov and Clang Scan Build must not report
  new issues. All shell scripts must pass shellcheck.
  
Static Binaries
---------------

The following commands will generate statically-linked binaries
which can be optionally distributed for each release::

    make -C src loader.elf loader-m32.elf build.h
    LDFLAGS="${LDFLAGS} -static" make -C proot

Documentation Update
--------------------

1. Update the :code:`doc/changelog.rst` file.

2. Update the release number in the :code:`doc/proot/manual.rst` file.

3. Generate the documentation::

     make -C doc

4. Regenerate the website::

     SITE_DIR=../../proot-me.github.io
     make -eC doc dist # relative to doc directory
