=========
 Roadmap
=========

PRoot v5.2: a transparent loader
================================

* The loader is currently mapped to a predefined address, but some
  systems enforce constraints that might make this address illegal
  (cf. Github issue #75).  A solution is to make the loader a
  position-independent program.

* Position-independent programs are currently loaded to a predefined
  address, even if the ASLR is enabled.  This might raise the same
  issue as above, and this might create conflicts with the loader and
  mappings created implicitly by the kernel ("[vdso]", "[stack]",
  ...).  A solution is to load position-independent programs using
  mmap(NULL, ...), that is, to let the kernel choose the location.

* Position-dependent programs might require mappings that would
  overlap the loader, that is, both the loader and the program are
  mapped to a same page.  A solution is to detect such a situation and
  to make the loader relocate itself before loading the program.  Note
  that this doesn't solve the issue for other mappings created
  implicitly by the kernel ("[vdso]", "[stack]", ...).

* The loader stays mapped in memory once its job is done.  This might
  create unexpected address-space pressure for programs that need
  large memory mappings or scan the address-space, like the JVM or
  Valgrind.  A solution is to unmap the loader once its job is done.


PRoot v6.0: kernel namespaces accelerator
=========================================

Since Linux 3.8 it is possible to use kernel namespaces without
privileges, for instance using ``unshare(CLONE_NEWNS|CLONE_NEWUSER)``.
That way, it is possible to mount/bind, to chroot, and to change its
user and group identities without privileges, just like with PRoot.
Kernel namespaces have no performance impact compared to PRoot,
however they imply that supplementary groups are dropped.

The idea is to add a command-line option to PRoot that explicitely
enable kernel namespaces to improve performance by avoiding most
internal operations, like path canonicalisation.  However, some
operations can't be avoided:

  - on x86_64, uname(2) must be ptraced for i686 rootfs.

  - when using an asymetric binding, readlink(2) must be ptraced
    (cf. commit 8e5fa256)

  - when using a qemulator, execve(2) must be ptraced.

  - when using an extension, all its syscalls must be ptraced.

  - when using an extension that hooks HOST_PATH, kernel namespaces
    must be disabled.

  - when binding a symlink (using the "!" syntax), kernel namespaces
    must be disabled.


PRoot v7.0: VFS
===============

One core feature of PRoot is the path translation.  This mechanism
heavily relies on "stat", sadly this syscall is quite slow on some FS
(like NFS).  The idea is to cache the results of the path translation
mechanism to avoid the use of "stat" as much as possible in order to
speed-up PRoot.

The internal structure of this FS cache could also be used to emulate
the "getdents" syscall in order to add or hide entries.


Not yet scheduled
=================

Fixes
-----

* The heap gap isn't removed when ADDR_NO_RANDOMIZE personality is set
  (cf. commit 2f6a1fac).

* Make P_tmpdir customizable at runtime (cf. Github issue #79).

* Fix ``proot-x86_64 -k ... -r rootfs-i686 uname -m``.

* Fix TODO in test-517e1d6a.

* Fix ptrace emulation support on ARM.

* Fix support for GDB v/fork & execve catchpoints.

* Add support for unimplemented ptrace requests.

* Fix ``mkdir foo; cd foo; rmdir ../foo; readlink /proc/self/cwd``.

* Forbid rename/unlink on a mount point::

    $ mv mount_point elsewhere
    mv: cannot move "mount_point" to "elsewhere": Device or resource busy

* mix-mode: Add support for the string $ORIGIN (or equivalently
  ${ORIGIN}) in an rpath specification.

* mix-mode: Add support for /etc/ld.so.preload and
  /etc/ld.so.conf[.d].

* Remove sub-reconfiguration relics.

* Don't use the same prefix for glued path::

      $ proot -b /etc/fstab -b readdir:/bin/readdir -r /tmp/empty-ro-rootfs /bin/readdir /bin
      DT_DIR  ..
      DT_DIR  .
      DT_REG  readdir
      DT_REG  fstab
      $ proot -b /etc/fstab -b readdir:/bin/readdir -r /tmp/empty-ro-rootfs /bin/readdir /etc
      DT_DIR  ..
      DT_DIR  .
      DT_REG  readdir
      DT_REG  fstab


Features
--------

* Support "proot [...] -- command" syntax

* Write a loader for a.out programs, to be able to execute programs
  from very old Linux distros :)

* Use PTRACE_O_EXITKILL if possible.

* Add a mechanism to add new [fake] entries in /proc, for instance
  ``/proc/proot/config``.

* Make ``mount --bind`` change the tracee's configuration dynamically.

* Make ``chroot`` change the tracee's configuration dynamically (not
  only of ``/``).

* Add support for a special environment variable to add paths
  dynamically to the host LD_LIBRARY_PATH
  ("EXTRA_HOST_LD_LIBRARY_PATH").

* Add a "blind" mode where:

  - unreadable executables can be executed::

      proot mount: OK (rwsr-xr-x)
      proot ping: failed (rws--x--x)

  - unreadable directories can be accessed

* Add support for coalesced options, for instance ``proot -eM``.

* Allow a per-module verbose level.

* vfs: Emulate chown, chmod, and mknod when -0 (fake_id0) is enabled.


Documentation
-------------

* Mention that the default command is "/bin/sh -l" in "proot --help".

* Mention "container" in the documentation.

* Explain bindings aren't exclusive, i.e. ``-b /tmp:/foo`` doesn't
  invalidate ``-b /tmp:/bar``.

* Explain why PRoot does not work with setuid programs.


Misc.
-----

* Replace ``readlink(XXX, path, PATH_MAX)`` with ``readlink(XXX, path,
  PATH_MAX - 1)``.

* read_string() should return -ENAMETOOLONG when ``length(XXX) >=
  max_size``.

* mix-mode: Check (in ld.so sources) if more than one RPATH/RUNPATH
  entry is allowed.

* Ensure tracees' clone flags satisfies ``CLONE_PTRACE &
  ~CLONE_UNTRACED``.

* Add a stealth mode where over-the-stack content is restored.

* Try Trinity/Scrashme (syscall fuzzers) against PRoot.


Performance
-----------

* Fallback to /proc/<pid>/mem when process_vm_readv() isn't available.

* Add a "multi-process" mode where there's one fork of PRoot per
  monitored process.

    Each time a new_tracee structure is created, PRoot forks itself.
    Be sure that the tracer of this new process really is the new
    forked PRoot! (Thanks Yves for this comment)
