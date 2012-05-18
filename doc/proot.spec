%define version v1.9

Summary   : chroot, mount --bind, and binfmt_misc without privilege/setup
Version   : %{version}
Release   : 1
License   : GPL2+
Group     : Applications/System
Source    : proot-%{version}.tar.gz
Buildroot : %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
Prefix    : /usr
Name      : proot

# For testing purpose only
BuildRequires: glibc-static

%description
PRoot is a user-space implementation of chroot, mount --bind,
and binfmt_misc.  This means that users don't need any privilege
or setup to do things like: using an arbitrary directory as the new
root file-system or making files accessible somewhere else in the
file-system hierarchy or executing programs built for another CPU
architecture transparently through QEMU user-mode.  Technically PRoot
relies on ptrace, an unprivileged system-call available in every
Linux kernel.

%prep
%setup -n proot-%{version}

%build
make -C src

%install
make -C src install PREFIX=%{buildroot}/%{prefix}
install -D doc/proot.1 %{buildroot}/%{_mandir}/man1/proot.1

%check
make -C tests

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%{prefix}/bin/proot
%doc %{_mandir}/man1/proot.1*
%doc COPYING
%doc doc/*
