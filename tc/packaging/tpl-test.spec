#sbs-git:slp/pkgs/o/opengl-es-mali400mp opengl-es-mali400mp 0.1.3 8e36cff0cad0eee06ba0a107442cc14ef2e7bd44
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
#BuildRequires:  pkgconfig(libpng)
#BuildRequires:  pkgconfig(gles20)
#BuildRequires:	opengl-es-m400-devel
#BuildRequires:  pkgconfig(libump)
#BuildRequires:  opengl-es-mali-midgard-devel
#BuildRequires:  pkgconfig(x11)
BuildRequires:  pkgconfig(gbm)
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  libwayland-egl-devel
#BuildRequires:  pkgconfig(wayland-egl)
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
#cp -arp ./data 		%{buildroot}/opt/usr/tpl-test

%files
%manifest tpl-test.manifest
%defattr(-,root,root,-)
/opt/usr/tpl-test/*
