Name: tpl-test
Version: 0.1.0
Release: 1
Summary: TPL Test Module

Group: Graphics & UI Framework/GL
License: BSD-3-Clause, MIT
Source0: %{name}-%{version}.tar.gz

BuildRequires: pkgconfig(libtbm)
BuildRequires: pkgconfig(gbm)
BuildRequires: pkgconfig(wayland-client)
BuildRequires: libwayland-egl-devel
BuildRequires: libtpl-egl-devel


%description
Test module for testing libtpl-egl frontend APIs


%prep
%setup -q


%build
export GTEST_DIR="%{_builddir}/%{name}-%{version}/gtest/googletest/googletest"
export GTEST_INCLUDE="-I${GTEST_DIR} -I${GTEST_DIR}/include"
export GTEST_FLAGS="-g -Wall -Wextra -pthread"

# Build Google Test Framework
cd gtest
make

# Build tpl-test using libgtest.a
cd ..
make


%install
rm -fr %{buildroot}

mkdir -p %{buildroot}/opt/usr/tpl-test
cp -arp ./tpl-test %{buildroot}/opt/usr/tpl-test


%files
%defattr(-,root,root,-)
/opt/usr/tpl-test/*

