
%define version v1.8.3

Summary   : chroot, mount --bind, and binfmt_misc without privilege/setup
Version   : %{version}
Release   : 1
License   : GPL2+
Group     : Applications/System
Source    : proot-%{version}.tar.gz
Buildroot : %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
Prefix    : /usr
Name      : proot

%description
PRoot is a user-space implementation of chroot, mount --bind,
and binfmt_misc.  This means that users don't need any privilege
or setup to do things like: using an arbitrary directory as the new
root file-system or making files accessible somewhere else in the
file-system hierarchy or executing programs built for another CPU
architecture transparently through QEMU.  Technically PRoot relies on
ptrace, an unprivileged system-call available in every Linux
kernel.

%prep
%setup -n proot-%{version}

%build
make -C src

%install
make -C src install PREFIX=%{buildroot}/%{prefix}

%check
make -C tests

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%{prefix}/bin/proot
%doc COPYING
%doc doc/*
  