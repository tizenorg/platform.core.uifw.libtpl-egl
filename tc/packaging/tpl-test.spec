Name:       tpl-test
Summary:    Test module for tpl
Version:	0.0.1
Release:    1
Group:      Graphics & UI Framework/GL
License:    Samsung
Source0:    %{name}-%{version}.tar.gz
BuildRequires:  pkgconfig(libtbm)
BuildRequires:  pkgconfig(gbm)
BuildRequires:  libwayland-egl-devel
BuildRequires:  libtpl-egl-devel
%description
Test module for testing TPL frontend APIs

%prep
%setup -q

%build
export LDFLAGS="${LDFLAGS} -rdynamic"
make

%install
mkdir -p %{buildroot}/opt/usr/tpl-test
cp -arp ./tpl-test 		%{buildroot}/opt/usr/tpl-test

%files
%defattr(-,root,root,-)
/opt/usr/tpl-test/*
