Name:       tpl-test
Summary:    test for tpl
Version:	0.0.1
Release:    01
Group:      System/X Hardware Support
License:    Samsung
Source0:    %{name}-%{version}.tar.gz
BuildRequires:  pkgconfig(libdrm)
BuildRequires:  pkgconfig(libudev)
BuildRequires:  pkgconfig(libtbm)
BuildRequires:  pkgconfig(gbm)
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  libwayland-egl-devel
BuildRequires:  libtpl-egl-devel
%description
The Simple Test Cases


%prep
%setup -q

%build
export LDFLAGS="${LDFLAGS} -rdynamic"
make

%install
mkdir -p %{buildroot}/opt/usr/tpl-test
cp -arp ./tpl-test 		%{buildroot}/opt/usr/tpl-test

%files
%manifest tpl-test.manifest
%defattr(-,root,root,-)
/opt/usr/tpl-test/*
