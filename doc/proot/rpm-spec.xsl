<?xml version="1.0" encoding="UTF-8"?>

<xsl:transform version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="text"/>

  <xsl:template match="/">%define version v<xsl:value-of select="/document/docinfo/version"/>

Summary   : <xsl:value-of select="/document/subtitle"/>
Version   : %{version}
Release   : 1
License   : GPL2+
Group     : Applications/System
Source    : proot-%{version}.tar.gz
Buildroot : %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
Prefix    : /usr
Name      : proot

BuildRequires: libtalloc-devel

%if 0%{?suse_version} >= 1210 || 0%{?fedora_version} >= 15
BuildRequires: glibc-static
%endif

%if !0%{?suse_version} != 0
BuildRequires: which
%endif

%description
<xsl:value-of select="/document/section[@names='description']/paragraph[1]"/>

%prep
%setup -n proot-%{version}

%build
make -C src

%install
make -C src install PREFIX=%{buildroot}/%{prefix}
install -D doc/proot/man.1 %{buildroot}/%{_mandir}/man1/proot.1

%check
env LD_SHOW_AUXV=1 true
cat /proc/cpuinfo
./src/proot -V
./src/proot -v 1 true
make -C test

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%{prefix}/bin/proot
%doc %{_mandir}/man1/proot.1*
%doc COPYING
%doc doc/*

%changelog
</xsl:template>
</xsl:transform>
