%define TPL_VER_MAJOR	0
%define TPL_VER_MINOR	1
%define TPL_VERSION	%{TPL_VER_MAJOR}.%{TPL_VER_MINOR}
%define TPL_RELEASE	01

%define WINSYS_DRI2	0
%define WINSYS_DRI3	1
%define WINSYS_WL	0

%define ENABLE_TTRACE	0

Name:			libtpl
Summary:		Tizen Porting Layer for ARM Mali EGL
%if "%{WINSYS_DRI2}" == "1"
Version:		%{TPL_VERSION}.dri2
%else
%if "%{WINSYS_DRI3}" == "1"
Version:		%{TPL_VERSION}.dri3
%else
%if "%{WINSYS_WL}" == "1"
Version:		%{TPL_VERSION}.wl
%else
Version:		%{TPL_VERSION}.unknown
%endif
%endif
%endif
Release:		%{TPL_RELEASE}
ExclusiveArch:		%arm
Group:			System/Libraries
License:		MIT
Source:			%{name}-%{version}.tar.gz

BuildRequires:		pkgconfig(glesv2)
BuildRequires:		pkgconfig(libdrm)
BuildRequires:		pkgconfig(libtbm)

%if "%{WINSYS_DRI2}" == "1" || "%{WINSYS_DRI3}" == "1"
BuildRequires:		pkgconfig(libdri2)
BuildRequires:		pkgconfig(xext)
BuildRequires:		pkgconfig(xfixes)
BuildRequires:		pkgconfig(x11)
BuildRequires:		pkgconfig(x11-xcb)
BuildRequires:		pkgconfig(xcb)
BuildRequires:		pkgconfig(xcb-dri3)
BuildRequires:		pkgconfig(xcb-sync)
BuildRequires:		pkgconfig(xcb-present)
BuildRequires:		pkgconfig(xshmfence)
%endif

%if "%{WINSYS_WL}" == "1"
BuildRequires:  	pkgconfig(gbm)
BuildRequires:  	wayland-devel
BuildRequires:  	pkgconfig(wayland-drm)
%endif

%description
Tizen Porting Layer (a.k.a TPL) is a linkage between the underlying window
system and the EGL porting layer found in ARM Mali DDKs.

The following window systems are supported:
- X11 DRI2/DRI3
- Wayland

%package devel
Summary:		Development files for TPL
Group:			System/Libraries
Requires:		%{name} = %{version}-%{release}

%description devel
This package contains the development libraries, header files needed by
the DDK for ARM Mali EGL.

%if "%{WINSYS_WL}" == "1"
%Package -n libgbm_tbm
Summary:		A backend of GBM using TBM
Group:			Development/Libraries
BuildRequires:		pkgconfig(gbm)
BuildRequires:		pkgconfig(libdrm)
BuildRequires:		pkgconfig(wayland-drm)

%description -n libgbm_tbm
GBM backend using TBM(Tizen Buffer Manager)
%endif

%prep
%setup -q

%build
%if "%{WINSYS_WL}" == "1"
make -C src/wayland_module/gbm_tbm all
%endif

%if "%{WINSYS_DRI2}" == "1" 
export TPL_OPTIONS=%{TPL_OPTIONS}-winsys_dri2
%endif
%if "%{WINSYS_DRI3}" == "1" 
export TPL_OPTIONS=%{TPL_OPTIONS}-winsys_dri3
%endif
%if "%{WINSYS_WL}" == "1" 
export TPL_OPTIONS=%{TPL_OPTIONS}-winsys_wl
%endif

export TPL_VER_MAJOR=%{TPL_VER_MAJOR}
export TPL_VER_MINOR=%{TPL_VER_MINOR}

%if "%{ENABLE_TTRACE}" == "1"
export TPL_OPTIONS=%{TPL_OPTIONS}-ttrace
%endif

make all

%install
mkdir -p %{buildroot}%{_libdir}
mkdir -p %{buildroot}%{_libdir}/pkgconfig

cp -a libtpl.so.%{TPL_VERSION}		%{buildroot}%{_libdir}/
ln -sf libtpl.so.%{TPL_VER_MAJOR}	%{buildroot}%{_libdir}/libtpl.so.%{TPL_VERSION}
ln -sf libtpl.so			%{buildroot}%{_libdir}/libtpl.so.%{TPL_VER_MAJOR}

cp -a src/tpl.h				%{buildroot}%{_includedir}/
cp -a pkgconfig/tpl.pc			%{buildroot}%{_libdir}/pkgconfig/

%if "%{WINSYS_WL}" == "1"
%makeinstall -C src/wayland_module/gbm_tbm
%endif

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_libdir}/libtpl.so
%{_libdir}/libtpl.so.%{TPL_VER_MAJOR}
%{_libdir}/libtpl.so.%{TPL_VER_MAJOR}.%{TPL_VER_MINOR}

%if "%{WINSYS_WL}" == "1"
%files -n libgbm_tbm
%{_libdir}/gbm/gbm_tbm.so
%{_libdir}/libgbm_tbm.so
%endif

%files devel
%defattr(-,root,root,-)
%{_includedir}/tpl.h
%{_libdir}/pkgconfig/tpl.pc
