%define version v0.7.0

Summary   : chroot, mount --bind, and binfmt_misc without any privilege
Version   : %{version}
Release   : 2
License   : GPL2+
Group     : Applications/System
Source    : proot-%{version}.tar.gz
Buildroot : %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
Prefix    : /usr
Name      : proot

%description
PRoot is a rewrite of chroot, mount --bind, and binfmt_misc in
user-mode, so that you do not need any privilege to -respectively- use
an arbitrary directory as a root file-system, make files accessible
somewhere else in the file-system hierarchy, and execute programs
built for another CPU architecture transparently with the help of a
CPU emulator like QEMU.  PRoot relies on ptrace, an unprivileged
system-call available in every Linux kernels and also used by
User-Mode Linux, strace, and GDB.

%prep
%setup -n proot-%{version}

%build
make -C src

%install
make -C src install PREFIX=%{buildroot}/%{prefix}

%clean
rm -rf %{buildroot}

%files
%{prefix}/bin/proot
%doc COPYING
%doc doc/*
