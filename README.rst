PRoot
=====

*chroot, mount --bind, and binfmt_misc without privilege/setup for Linux*

Build status
============

**Please take the PRoot Usage Survey for 2023!**

.. image:: https://img.shields.io/badge/survey-2023-green?style=flat-square
   :target: https://www.surveymonkey.com/r/7GVXS7W
   
--

.. image:: https://img.shields.io/gitlab/pipeline/proot/proot.svg?label=gitlab-ci&style=flat-square
   :target: https://gitlab.com/proot/proot/pipelines

.. image:: https://img.shields.io/badge/scan--build-latest-yellow.svg?style=flat-square
   :target: https://proot.gitlab.io/proot/reports/scan-build

.. image:: https://img.shields.io/badge/lcov-latest-6688D4.svg?style=flat-square
   :target: https://proot.gitlab.io/proot/reports/lcov

.. image:: https://img.shields.io/cii/summary/2444.svg?label=cii-best-practices&style=flat-square
   :target: https://bestpractices.coreinfrastructure.org/projects/2444

.. image:: https://img.shields.io/badge/DOI-10.5281%2Fzenodo.5371409-blue?style=flat-square
   :target: https://doi.org/10.5281/zenodo.5371409

Compiling
=========

The following commands can be used to compile PRoot and CARE::

    git submodule update --init lib/uthash   # required for CARE (vendored headers)
    make -C src loader.elf loader-m32.elf build.h # first build the config and loader
    make -C src proot care # then compile PRoot and CARE
    make -C test # run test suite

|asciicast|

.. |asciicast| image:: https://asciinema.org/a/315367.svg
   :target: https://asciinema.org/a/315367

Dependencies
============

- `libarchive <https://libarchive.org>`_
- `libtalloc <https://talloc.samba.org>`_
- `uthash <https://troydhanson.github.io/uthash>`_ (for CARE: shipped as git submodule ``lib/uthash``, not the distro package)

On Debian/Ubuntu, install the development packages (the runtime package alone is not enough)::

    sudo apt install build-essential pkg-config libtalloc-dev
    sudo apt install libarchive-dev   # only if you build CARE

Static linking
==============

To link ``proot`` (or ``care``) as a fully static executable, pass ``STATIC=1``.
When ``pkg-config`` is available, static dependency flags are taken from it::

    make -C src clean
    make -C src loader.elf loader-m32.elf build.h
    make -C src proot STATIC=1
    # optional: make -C src care STATIC=1

Glibc fully-static binaries can be fragile for DNS/NSS; if linking fails with
missing symbols, try a `musl` cross toolchain (e.g. ``x86_64-linux-musl-gcc``)
via ``make -C src CC=x86_64-linux-musl-gcc proot STATIC=1``, or link
dynamically as usual without ``STATIC=1``.

Manuals
=======

- `PRoot <https://github.com/proot-me/proot/blob/master/doc/proot/manual.rst#proot>`_

- `CARE <https://github.com/proot-me/proot/blob/master/doc/care/manual.rst#care>`_

Support
=======

- `Mailing List <mailto:proot_me@googlegroups.com>`_
- `Forum <https://groups.google.com/forum/?fromgroups#!forum/proot_me>`_
- `Chat <https://gitter.im/proot-me/devs>`_

License
=======

SPDX-License-Identifier: `GPL-2.0-or-later <COPYING>`_
