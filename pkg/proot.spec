%define version v0.6.2

Summary   : chroot, mount --bind, and binfmt_misc without any privilege
Version   : %{version}
Release   : 1
License   : GPL2+
Group     : Applications/System
Source    : proot-%{version}.tar.gz
Buildroot : %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

# Special case for the STLinux distro.
%if %{defined _stm_install_prefix}
    %define prefix %{_stm_install_prefix}
    %define bindir %{_stm_host_bin_dir}
    %define name   %{_stm_pkg_prefix}-host-proot
%else
    %define prefix /usr/bin
    %define bindir /usr/bin
    %define name   proot
%endif
Prefix    : %{prefix}
Name      : %{name}

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
rm  -rf  $RPM_BUILD_DIR/proot-%{version}
tar -xzf $RPM_SOURCE_DIR/proot-%{version}.tar.gz

%build
make -C proot-%{version}/src

%install
make -C proot-%{version}/src install DESTDIR=%{buildroot}/%{bindir}

%clean
rm -rf %{buildroot}

%files
%{bindir}/proot
