How to make a release of PRoot?
===============================

This document summarizes checks that must be performed before
releasing PRoot or CARE.

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
  
Static Binaries
---------------

The following commands will generate statically-linked binaries
which can be optionally distributed for each release::

    make -C src clean loader.elf loader-m32.elf build.h
    LDFLAGS="${LDFLAGS} -static" make -C src proot care

Documentation Update
--------------------

1. Update the :code:`doc/changelog.rst` file.

2. Update the release number in the :code:`doc/proot/manual.rst` file.

3. Generate the documentation::

     make -C doc

4. Regenerate the website::

     SITE_DIR=../../proot-me.github.io
     make -eC doc dist # relative to doc directory
