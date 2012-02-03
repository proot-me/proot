
%define version 0.7.1

Summary   : chroot, mount --bind, and binfmt_misc without privilege/setup
Version   : %{version}
Release   : 2
License   : GPL2+
Group     : Applications/System
Source    : proot-%{version}.tar.gz
Buildroot : %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
Prefix    : /usr
Name      : proot

%description
PRoot is a user-mode implementation of chroot, mount --bind,
and binfmt_misc, that means users don't need any privilege or
setup to use an arbitrary directory as the new root file-system, to
make files accessible somewhere else in the file-system hierarchy, and
to execute programs built for another CPU architecture transparently
through QEMU.  PRoot relies on ptrace, an unprivileged system-call
available in every Linux kernels.

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
  