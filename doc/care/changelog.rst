=======================================================================
=======================================================================

  This file is no longer used for tracking changes for CARE. For
  user visible changes, please look in the top-level CHANGELOG.rst
  file.

=======================================================================
=======================================================================

CARE v2.2
=========

+ Special characters in environment variables are now correctly quoted
  in the generated "re-execute.sh" script.  For instance, the value of
  "COMP_WORDBREAKS" environment variable on Ubuntu used to make CARE
  generate broken "re-execute.sh" scripts.

+ It is now possible to deliver CARE as a dynamically linked binary.
  In this case, the self-extracting format is not supported since one
  can't assume the re-execution environment contains all the
  pre-requisites (libtalloc, libarchive, proot).  Thus, CARE and PRoot
  should be deployed independently on the re-execution environment.

+ Syscall are now emulated only if it is really needed.

This release is based on PRoot v4.0.3, so it contains all the
following generic fixes too:

  https://github.com/cedric-vincent/PRoot/releases/tag/v4.0.0
  https://github.com/cedric-vincent/PRoot/releases/tag/v4.0.1
  https://github.com/cedric-vincent/PRoot/releases/tag/v4.0.2
  https://github.com/cedric-vincent/PRoot/releases/tag/v4.0.3


CARE v2.1
=========

This release contains all the fixes from PRoot v3.2.2 [1] and the
following new features:

+ CARE now supports three new archive formats:

  - a self-extracting format, this has become the default behavior.
    Here's an example::

      somewhere$ care echo OK
      [...]
      OK
      [...]
      care info:   - run `./care-140115135934.bin` to extract the output archive.

      elsewhere$ ./care-140115135934.bin
       info: archive found: offset = 195816, size = 3035122
       info: extracted: care-140115135934/rootfs/usr
       info: extracted: care-140115135934/rootfs/usr/local
       [...]
       info: extracted: care-140115135934/re-execute.sh
       info: extracted: care-140115135934/README.txt
       info: extracted: care-140115135934/proot

   - the GNU tar format, this is the most commonly used archive
     format.  It can be combined with all the compression formats
     supported by CARE: ".tar.gz" or ".tar.lzo".

   - a "copy" format where the content is copied directly into a
     directory instead of being archived in a file.  For instance::

       $ care -o foo/ echo OK
       [...]
       $ ls foo
       README.txt  proot  re-execute.sh  rootfs/

+ It is recommended to use the new "-x/--extract" option to extract
  archives created by CARE, since most extracting tools -- that are
  not based on libarchive -- are too limited to extract them
  correctly.

+ CARE now returns the exit code from the re-executed command.  For
  instance, with the previous version:

    $ care-v2.0 -o test.cpio sh -c 'exit 1'
    [...]
    $ echo $?
    1
    [...]
    $ test/re-execute.sh
    $ echo $?
    0
    $ test/re-execute.sh sh -c 'exit 2'
    $ echo $?
    0

  And with this new version:

    $ care-v2.1 -o test.cpio sh -c 'exit 1'
    [...]
    $ echo $?
    1
    [...]
    $ test/re-execute.sh
    $ echo $?
    1
    $ test/re-execute.sh sh -c 'exit 2'
    $ echo $?
    2

+ Applications that rely on DBUS and/or on KDE cache are now
  supported.

Thanks to Antoine Moynault, Christophe Guillon, and Rémi Duraffort for
their help.

[1] https://github.com/cedric-vincent/PRoot/blob/v3.2.2/doc/changelog.txt
