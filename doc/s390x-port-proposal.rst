s390x port proposal
====================

This document scopes out issue `#324`_ (milestone v5.6.0), the next
architecture-support issue in line after the multi-arch work was
split across per-version milestones.

.. _#324: https://github.com/proot-me/proot/issues/324

What a new architecture touches
---------------------------------

Based on how ``ARCH_ARM64`` is wired in today, a new arch needs:

+ ``src/arch.h``: an ``ARCH_S390X`` block defining
  ``SYSNUMS_HEADER1``/``SYSNUMS_ABI1``, ``SYSTRAP_SIZE``,
  ``SECCOMP_ARCHS``, ``HOST_ELF_MACHINE``, ``LOADER_ADDRESS``,
  ``EXEC_PIC_ADDRESS``/``INTERP_PIC_ADDRESS``.

+ ``src/tracee/reg.c``: a ``reg_offset[]`` table mapping
  ``SYSARG_NUM``/``SYSARG_1``-``SYSARG_6``/``SYSARG_RESULT``/
  ``STACK_POINTER``/``INSTR_POINTER`` to fields of the platform's
  ``struct user_regs_struct``.

+ ``src/syscall/sysnums-s390x.h``: syscall-number table, same shape as
  the existing ``sysnums-*.h`` files.

+ ``src/loader/assembly-s390x.h``: the raw asm syscall trampoline the
  loader uses, analogous to ``assembly-arm64.h``/``assembly-x86_64.h``.

+ ``src/execve/ldso.c``: any arch-specific ELF/auxv handling (mostly
  already generic, needs checking against s390x specifics once the
  above exists).

Sourced technical facts
-------------------------

Pulled directly from the Linux kernel and glibc source, not assumed:

+ **Register struct** (`arch/s390/include/uapi/asm/ptrace.h`_):
  ``struct user_regs_struct`` is ``psw_t psw``, then
  ``unsigned long gprs[NUM_GPRS]`` (16 general-purpose registers),
  ``unsigned int acrs[NUM_ACRS]``, ``unsigned long orig_gpr2``, fp
  regs, PER trace data.

+ **Syscall convention** (glibc
  ``sysdeps/unix/sysv/linux/s390/s390-64/sysdep.h``): syscall number
  in ``%r1``, arguments 1-6 in ``%r2``-``%r7``, return value in
  ``%r2``, trap instruction ``svc 0``.

+ **ELF/audit constants** (``include/uapi/linux/audit.h``):
  ``AUDIT_ARCH_S390 = EM_S390``, ``AUDIT_ARCH_S390X = EM_S390 |
  __AUDIT_ARCH_64BIT``, notably with no ``__AUDIT_ARCH_LE`` flag.

+ **Syscall table source** (``arch/s390/kernel/syscalls/syscall.tbl``):
  same 4-column ``<nr> <abi> <name> <entry>`` format already used to
  verify syscall numbers elsewhere in this codebase (eg. fchmodat2 in
  #408), ~450 entries, one shared table for both s390 (31-bit) and
  s390x (64-bit).

.. _arch/s390/include/uapi/asm/ptrace.h: https://github.com/torvalds/linux/blob/master/arch/s390/include/uapi/asm/ptrace.h

One open question needs real hardware/QEMU to resolve: whether
``SYSARG_1`` should map to ``gprs[2]`` (relying on proot's own
ORIGINAL/CURRENT register-version cache, the way ``ARCH_ARM64`` reuses
``regs[0]`` for both argument 1 and the result) or to the kernel's own
``orig_gpr2`` shadow field. Both are plausible from the header alone;
this needs to be settled by testing, not guessed at.

The big risk: first big-endian target
----------------------------------------

Every architecture PRoot currently supports is little-endian
(x86, x86_64, arm, arm64, sh4). The absence of ``__AUDIT_ARCH_LE`` in
s390x's audit-arch constant is the first concrete signal that this
port is different in kind, not just another ``reg_offset[]`` table.

Anywhere PRoot reads or writes tracee memory as anything other than
opaque bytes needs auditing for a host/guest byte-order assumption
before this port can be trusted: register values, struct layouts read
via ``read_data``/``write_data``, path/buffer length fields. This
needs its own investigation pass; it is not scoped out further here.

31-bit compat mode: out of scope for v1
------------------------------------------

s390x can run 31-bit s390 binaries in compat mode, the same relationship
x86_64 has with i386/x32 (three sysnum tables) and arm64 could have
with 32-bit ARM EABI (which PRoot doesn't support: ``ARCH_ARM64``
ships with exactly one sysnum table). Following the arm64 precedent,
propose 64-bit-only for the initial port and revisit 31-bit compat as
a separate follow-up if there's real demand.

Testing and CI
-----------------

No GitHub-hosted runner offers native s390x. The existing aarch64
smoke test (``.github/workflows/pull-request.yml``) cross-compiles and
then runs ``qemu-aarch64 -L <sysroot> src/proot --version`` as a basic
liveness check; the same shape would work for a cross-compiled s390x
build.

That only proves the binary starts, not that ptrace-based tracing
works. A meaningfully bigger ask is running proot itself, not just
what it traces, under ``qemu-user``. ptrace-of-a-process-under-emulation
is a known-fragile combination, not something to assume works without
hands-on verification on either real s390x hardware or a full-system
QEMU install.

Proposed phases
-------------------

1. This document: sourced references, open questions flagged.
2. Byte-order audit of ``read_data``/``write_data`` and any
   struct-layout-dependent code, before writing s390x-specific code.
3. Core port: ``arch.h``, ``tracee/reg.c``, ``sysnums-s390x.h``
   (generated from the kernel's ``syscall.tbl``, not hand-typed; worth
   writing this generator once and reusing it for future ports),
   ``loader/assembly-s390x.h``.
4. Verify the open ``SYSARG_1``/``orig_gpr2`` question and the
   byte-order audit's findings against a real or QEMU-emulated s390x
   target before merging.
