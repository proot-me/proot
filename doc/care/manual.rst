======
 CARE
======

-------------------------------------------------
Comprehensive Archiver for Reproducible Execution
-------------------------------------------------

:Date: 2023-05-13
:Version: 2.3.0
:Manual section: 1


Synopsis
========

**care** [*option*] ... *command*


Description
===========

CARE monitors the execution of the specified command to create an
*archive* that contains all the material required to *re-execute* it
in the same context.  That way, the command will be reproducible
everywhere, even on Linux systems that are supposed to be not
compatible with the original Linux system.  CARE is typically useful
to get reliable bug reports, demonstrations, `artifact evaluation`_,
tutorials, portable applications, minimal rootfs, file-system
coverage, ...

By design, CARE does not record events at all.  Instead, it archives
environment variables and accessed file-system components -- before
modification -- during the so-called *initial* execution.  Then, to
reproduce this execution, the ``re-execute.sh`` script embedded into
the archive restores the environment variables and relaunches the
command confined into the saved file-system.  That way, both *initial*
and *reproduced* executions should produce the same results as they
use the same context, assuming they do not rely on external events --
like key strokes or network packets -- or that these external events
are replayed manually or automatically, using umockdev_ for instance.
That means it is possible to alter explicitly the reproduced
executions by changing content of the saved file-system, or by
replaying different external events.

.. _umockdev: https://github.com/martinpitt/umockdev

.. _artifact evaluation: http://www.artifact-eval.org

Privacy
-------

To ensure that no sensitive file can possibly leak into the archive,
CARE *conceals* recursively the content of ``$HOME`` and ``/tmp``,
that is, they appear empty during the original execution.  Although,
for consistency reasons, the content of ``$PWD`` is *revealed* even if
it is nested into the two previous paths.

As a consequence, a program executed under CARE may behave
unexpectedly because a required path is not accessible anymore.  In
this case, such a path has to be revealed explicitly.  For details,
see the options ``--concealed-path`` and ``--revealed-path``, and the
file ``concealed-accesses.txt`` as well.

It is advised to inspect the archived content before sharing it.


Options
=======

The command-line interface is composed of two parts: first CARE's
options, then the command to launch.  This section describes the
options supported by CARE, that is, the first part of its command-line
interface.

-o path, --output=path
    Archive in *path*, its suffix specifies the format.

    The suffix of *path* is used to select the archive format, it can
    be one of the following:

    =========  ========================================================
    suffix     comment
    =========  ========================================================
    /          don't archive, copy into the specified directory instead
    .tar       most common archive format
    .cpio      most portable archive format, it can archive sockets too
    ?.gz       most common compression format, but slow
    ?.lzo      fast compression format, but uncommon
    ?.bin      see ``Self-extracting format`` section
    ?.?.bin    see ``Self-extracting format`` section
    .bin       see ``Self-extracting format`` section
    .raw       recommended archive format, use `care -x` to extract
    =========  ========================================================

    where "?" means the suffix must be combined with another one.  For
    examples: ".tar.lzo", ".cpio.gz", ".tar.bin", ".cpio.lzo.bin", ...
    If this option is not specified, the default output path is
    ``care-<DATE>.bin`` or ``care-<DATE>.raw``, depending on whether
    CARE was built with self-extracting format support or not.

-c path, --concealed-path=path
    Make *path* content appear empty during the original execution.

    Some paths may contain sensitive data that should never be
    archived.  This is typically the case for most of the files in:

    * $HOME
    * /tmp

    That's why these directories are recursively *concealed* from the
    original execution, unless the ``-d`` option is specified.
    Concealed paths appear empty during the original execution, as a
    consequence their original content can't be accessed nor archived.

-r path, --revealed-path=path
    Make *path* content accessible when nested in a concealed path.

    Concealed paths might make the original execution with CARE behave
    differently from an execution without CARE.  For example, a lot of
    ``No such file or directory`` errors might appear.  The solution
    is to *reveal* recursively any required paths that would be nested
    into a *concealed* path.  Note that ``$PWD`` is *revealed*, unless
    the ``-d`` option is specified.

-p path, --volatile-path=path
    Don't archive *path* content, reuse actual *path* instead.

    Some paths contain only communication means with programs that
    can't be monitored by CARE, like the kernel or a remote server.
    Such paths are said *volatile*; they shouldn't be archived,
    instead they must be accessed from the *actual* rootfs during the
    re-execution.  This is typically the case for the following pseudo
    file-systems, sockets, and authority files:

    * /dev
    * /proc
    * /sys
    * /run/shm
    * /tmp/.X11-unix
    * /tmp/.ICE-unix
    * $XAUTHORITY
    * $ICEAUTHORITY
    * /var/run/dbus/system_bus_socket
    * /var/tmp/kdecache-$LOGNAME

    This is also typically the case for any other fifos or sockets.
    These paths are considered *volatile*, unless the ``-d`` option is
    specified.

-e name, --volatile-env=name
    Don't archive *name* env. variable, reuse actual value instead.

    Some environment variables are used to communicate with programs
    that can't be monitored by CARE, like remote servers.  Such
    environment variables are said *volatile*; they shouldn't be
    archived, instead they must be accessed from the *actual*
    environment during the re-execution.  This is typically the case
    for the following ones:

    * DISPLAY
    * http_proxy
    * https_proxy
    * ftp_proxy
    * all_proxy
    * HTTP_PROXY
    * HTTPS_PROXY
    * FTP_PROXY
    * ALL_PROXY
    * DBUS_SESSION_BUS_ADDRESS
    * SESSION_MANAGER
    * XDG_SESSION_COOKIE

    These environment variables are considered *volatile*, unless the
    ``-d`` option is specified.

-m value, --max-archivable-size=value
    Set the maximum size of archivable files to *value* megabytes.

    To keep the CPU time and the disk space used by the archiver
    reasonable, files whose size exceeds *value* megabytes are
    truncated down to 0 bytes.  The default is 1GB, unless the ``-d``
    option is specified.  A negative *value* means no limit.

-d, --ignore-default-config
    Don't use the default options.

-x file, --extract=file
    Extract content of the archive *file*, then exit.

    It is recommended to use this option to extract archives created
    by CARE because most extracting tools -- that are not based on
    libarchive -- are too limited to extract them correctly.

-v value, --verbose=value
    Set the level of debug information to *value*.

    The higher the integer *value* is, the more detailed debug
    information is printed to the standard error stream.  A negative
    *value* makes CARE quiet except on fatal errors.

-V, --version, --about
    Print version, copyright, license and contact, then exit.

-h, --help, --usage
    Print the user manual, then exit.


Exit Status
===========

If an internal error occurs, ``care`` returns a non-zero exit status,
otherwise it returns the exit status of the last terminated program.
When an error has occurred, the only way to know if it comes from the
last terminated program or from ``care`` itself is to have a look at
the error message.


Files
=====

The output archive contains the following files:

``re-execute.sh``
    start the re-execution of the initial command as originally
    specified.  It is also possible to specify an alternate command.
    For example, assuming ``gcc`` was archived, it can be re-invoked
    differently:

        $ ./re-execute.sh gcc --version
        gcc (Ubuntu/Linaro 4.5.2-8ubuntu4) 4.5.2

        $ echo 'int main(void) { return puts(\"OK\"); }' > rootfs/foo.c
        $ ./re-execute.sh gcc -Wall /foo.c
        $ foo.c: In function "main":
        $ foo.c:1:1: warning: implicit declaration of function "puts"

``rootfs/``
    directory where all the files used during the original execution
    were archived, they will be required for the reproduced execution.

``proot``
    virtualization tool invoked by re-execute.sh to confine the
    reproduced execution into the rootfs.  It also emulates the
    missing kernel features if needed.

``concealed-accesses.txt``
    list of accessed paths that were concealed during the original
    execution.  Its main purpose is to know what are the paths that
    should be revealed if the the original execution didn't go as
    expected.  It is absolutely useless for the reproduced execution.


Limitations
===========

It's not possible to use GDB, strace, or any programs based on
*ptrace* under CARE yet.  This latter is also based on this syscall,
but the Linux kernel allows only one *ptracer* per process.  This will
be fixed in a future version of CARE thanks to a ptrace emulator.


Example
=======

In this example, Alice wants to report to Bob that the compilation of
PRoot v2.4 raises an unexpected warning::

    alice$ make -C PRoot-2.4/src/
    
    make: Entering directory `PRoot-2.4/src'
    [...]
    CC    path/proc.o
    ./path/proc.c: In function 'readlink_proc':
    ./path/proc.c:132:3: warning: ignoring return value of 'strtol'
    [...]

Technically, Alice uses Ubuntu 11.04 for x86, whereas Bob uses
Slackware 13.37 on x86_64.  Both distros are supposed to be shipped
with GCC 4.5.2, however Bob is not able to reproduce this issue on his
system::

    bob$ make -C PRoot-2.4/src/
    
    make: Entering directory `PRoot-2.4/src'
    [...]
    CC    path/proc.o
    [...]

Since they don't have much time to investigate this issue by iterating
between each other, they decide to use CARE.  First, Alice prepends
``care`` to her command::

    alice$ care make -C PRoot-2.4/src/
    
    care info: concealed path: $HOME
    care info: concealed path: /tmp
    care info: revealed path: $PWD
    care info: ----------------------------------------------------------------------
    make: Entering directory `PRoot-2.4/src'
    [...]
    CC    path/proc.o
    ./path/proc.c: In function 'readlink_proc':
    ./path/proc.c:132:3: warning: ignoring return value of 'strtol'
    [...]
    care info: ----------------------------------------------------------------------
    care info: Hints:
    care info:   - search for "conceal" in `care -h` if the execution didn't go as expected.
    care info:   - use `./care-130213072430.bin` to extract the output archive.

Then she sends the ``care-130213072430.bin`` file to Bob.  Now, he
should be able to reproduce her issue on his system::

    bob$ ./care-130213072430.bin
    [...]
    bob$ ./care-130213072430/re-execute.sh

    make: Entering directory `PRoot-2.4/src'
    [...]
    CC    path/proc.o
    ./path/proc.c: In function 'readlink_proc':
    ./path/proc.c:132:3: warning: ignoring return value of 'strtol'
    [...]

So far so good!  This compiler warning doesn't make sense to Bob since
``strtol`` is used there to check a string format; the return value is
useless, only the ``errno`` value matters.  Further investigations are
required, so Bob re-execute Alice's GCC differently to get more
details::

    bob$ ./care-130213072430/re-execute.sh gcc --version
    
    gcc (Ubuntu/Linaro 4.5.2-8ubuntu4) 4.5.2
    Copyright (C) 2010 Free Software Foundation, Inc.
    This is free software; see the source for copying conditions.  There is NO
    warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

The same invocation on his system returns something slightly
different::

    bob$ gcc --version
    
    gcc (GCC) 4.5.2
    Copyright (C) 2010 Free Software Foundation, Inc.
    This is free software; see the source for copying conditions.  There is NO
    warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

This confirms that both GCC versions are the same, however Alice's one
seems to have been modified by Ubuntu.  Although, according to the web
page related to this Ubuntu package [#]_, no changes regarding
``strtol`` were made.  So Bob decides to search into the files coming
from Alice's system, that is, the ``rootfs`` directory in the
archive::

    bob$ grep -wIrl strtol ./care-130213072430/rootfs
    
    care-130213072430/rootfs/usr/include/inttypes.h
    care-130213072430/rootfs/usr/include/stdlib.h
    [...]

Here, the file ``usr/include/stdlib.h`` contains a declaration of
``strtol`` with the "warn unused result" attribute.  On Ubuntu, this
file belongs to the EGLIBC package, and its related web page [#]_
shows that this attribute was actually wrongly introduced by the
official EGLIBC developers.  Ultimately Bob should notify them in this
regard.

Thanks to CARE, Bob was able to reproduce the issue reported by Alice
without effort.  For investigations purpose, he was able to re-execute
programs differently and to search into the relevant files.

.. [#] https://launchpad.net/ubuntu/oneiric/+source/gcc-4.5/4.5.2-8ubuntu4
.. [#] https://launchpad.net/ubuntu/+source/eglibc/2.13-0ubuntu13.2


Self-extracting format
======================

The self-extracting format used by CARE starts with an extracting
program, followed by a regular archive, and it ends with a special
footer.  This latter contains the signature "I_LOVE_PIZZA" followed by
the size of the embedded archive::

    +------------------------+
    |   extracting program   |
    +------------------------+
    |                        |
    |    embedded archive    |
    |                        |
    +------------------------+
    | uint8_t  signature[13] |
    | uint64_t archive_size  |  # big-endian
    +------------------------+

The command ``care -x`` can be used against a self-extracting archive,
even if they were not build for the same architecture.  For instance,
a self-extracting archive produced for ARM can be extracted with a
``care`` program built for x86_64, and vice versa.  It is also
possible to use external tools to extract the embedded archive, for
example::

    $ care -o foo.tar.gz.bin /usr/bin/echo OK
    [...]
    OK
    [...]

    $ hexdump -C foo.tar.gz.bin | tail -3
    0015b5b0  00 b0 2e 00 49 5f 4c 4f  56 45 5f 50 49 5a 5a 41  |....I_LOVE_PIZZA|
    0015b5c0  00 00 00 00 00 00 12 b4  13                       |.........|
    0015b5c9

    $ file_size=`stat -c %s foo.tar.gz.bin`
    $ archive_size=$((16#12b413))
    $ footer_size=21
    $ skip=$(($file_size - $archive_size - $footer_size))

    $ dd if=foo.tar.gz.bin of=foo.tar.gz bs=1 skip=$skip count=$archive_size
    1225747+0 records in
    1225747+0 records out
    1225747 bytes (1.2 MB) copied, 2.99546 s, 409 kB/s

    $ file foo.tar.gz
    foo.tar.gz: gzip compressed data, from Unix

    $ tar -tzf foo.tar.gz
    foo/rootfs/usr/
    [...]
    foo/re-execute.sh
    foo/README.txt
    foo/proot


Downloads
=========

CARE is heavily based on PRoot_, that's why they are both hosted in
the same repository: https://github.com/proot-me/proot. Previous CARE releases were packaged at https://github.com/proot-me/proot-static-build/releases, however, that repository has since been archived. The latest builds can be found under the job artifacts for the `GitLab CI/CD Pipelines <https://gitlab.com/proot/proot/pipelines>`_ for each commit.

.. _PRoot: https://proot-me.github.io

Colophon
========

Visit https://proot-me.github.io/care for help, bug reports, suggestions, patches, ...
Copyright (C) 2023 PRoot Developers, licensed under GPL v2 or later.

::

        _____ ____ _____ ____
       /   __/ __ |  __ \  __|
      /   /_/     |     /  __|
      \_____|__|__|__|__\____|

