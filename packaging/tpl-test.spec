Name: tpl-test
Version: 0.2.0
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
export GTEST_DIR="%{_builddir}/%{name}-%{version}/tc/gtest/googletest/googletest"
export GTEST_INCLUDE="-I${GTEST_DIR} -I${GTEST_DIR}/include"
export GTEST_FLAGS="-g -Wall -Wextra -pthread"

# Build Google Test Framework
cd tc/gtest
make

# Build tpl-test using libgtest.a
cd ..
make


%pre
if [ "$1" -eq 1 ]; then
echo "Initial installation"
  # Perform tasks to prepare for the initial installation
elif [ "$1" -eq 2 ]; then
  # Perform whatever maintenance must occur before the upgrade begins
rm -rf /opt/usr/tpl-test
fi


%install
mkdir -p %{buildroot}/opt/usr/tpl-test
cp -arp ./tc/tpl-test %{buildroot}/opt/usr/tpl-test


%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%dir /opt/usr/tpl-test/
/opt/usr/tpl-test/*

