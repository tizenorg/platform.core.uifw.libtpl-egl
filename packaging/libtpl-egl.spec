%define TPL_VER_MAJOR	0
%define TPL_VER_MINOR	5
%define TPL_RELEASE	1
%define TPL_VERSION	%{TPL_VER_MAJOR}.%{TPL_VER_MINOR}
%define TPL_VER_FULL	%{TPL_VERSION}.%{TPL_RELEASE}

%define ENABLE_TTRACE	0

################################################################################

%define TPL_WINSYS	WL

%if "%{?tizen_version}" == "2.4"
%define TPL_WINSYS	DRI3
%if "%{?tizen_target_name}" == "Z130H"
%define TPL_WINSYS	DRI2
%endif
%if "%{?tizen_target_name}" == "Z300H"
%define TPL_WINSYS	DRI2
%endif
%endif

%if "%{TPL_WINSYS}" != "DRI2" && "%{TPL_WINSYS}" != "DRI3" && "%{TPL_WINSYS}" != "WL"
BuildRequires:		ERROR(No_window_system_designated)
%endif

Name:			libtpl-egl
Version:		%{TPL_VERSION}
Release:		%{TPL_RELEASE}
%if "%{TPL_WINSYS}" == "DRI2"
Summary:		Tizen Porting Layer for ARM Mali EGL (DRI2 backend)
%endif
%if "%{TPL_WINSYS}" == "DRI3"
Summary:		Tizen Porting Layer for ARM Mali EGL (DRI3 backend)
%endif
%if "%{TPL_WINSYS}" == "WL"
Summary:		Tizen Porting Layer for ARM Mali EGL (Wayland backend)
%endif
Group:			System/Libraries
License:		MIT
Source:			%{name}-%{version}.tar.gz

BuildRequires:		pkg-config
BuildRequires:		pkgconfig(libdrm)
BuildRequires:		pkgconfig(libtbm)

%if "%{TPL_WINSYS}" == "DRI2" || "%{TPL_WINSYS}" == "DRI3"
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

%if "%{TPL_WINSYS}" == "WL"
BuildRequires:  	pkgconfig(gbm)
BuildRequires:  	wayland-devel
BuildRequires:  	pkgconfig(wayland-drm)
BuildRequires:		pkgconfig(wayland-egl)
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
This package contains the development libraries and header files needed by
the DDK for ARM Mali EGL.

%if "%{TPL_WINSYS}" == "WL"
%package -n libgbm_tbm
Summary:		A backend of GBM using TBM
Group:			Development/Libraries
BuildRequires:		pkgconfig(gbm)
BuildRequires:		pkgconfig(libdrm)
BuildRequires:		pkgconfig(wayland-drm)

%description -n libgbm_tbm
GBM backend using TBM(Tizen Buffer Manager)

%package -n libgbm_tbm-devel
Summary:		Development files for GBM using TBM
Group:			Development/Libraries
Requires:		libgbm_tbm = %{version}-%{release}

%description -n libgbm_tbm-devel
This package contains the development libraries and header files.
%endif

%prep
%setup -q

%build
%if "%{TPL_WINSYS}" == "WL"
make -C src/wayland_module/gbm_tbm all
%endif

%if "%{TPL_WINSYS}" == "DRI2"
export TPL_OPTIONS=${TPL_OPTIONS}-winsys_dri2
%endif
%if "%{TPL_WINSYS}" == "DRI3"
export TPL_OPTIONS=${TPL_OPTIONS}-winsys_dri3
%endif
%if "%{TPL_WINSYS}" == "WL"
export TPL_OPTIONS=${TPL_OPTIONS}-winsys_wl
%endif

export TPL_VER_MAJOR=%{TPL_VER_MAJOR}
export TPL_VER_MINOR=%{TPL_VER_MINOR}
export TPL_RELEASE=%{TPL_RELEASE}

%if "%{ENABLE_TTRACE}" == "1"
export TPL_OPTIONS=${TPL_OPTIONS}-ttrace
%endif

export TPL_OPTIONS=${TPL_OPTIONS}-

make all

%install
mkdir -p %{buildroot}%{_libdir}
mkdir -p %{buildroot}%{_includedir}
mkdir -p %{buildroot}%{_libdir}/pkgconfig

cp -a libtpl-egl.so.%{TPL_VER_FULL}	%{buildroot}%{_libdir}/
ln -sf libtpl-egl.so.%{TPL_VER_FULL}	%{buildroot}%{_libdir}/libtpl-egl.so.%{TPL_VERSION}
ln -sf libtpl-egl.so.%{TPL_VERSION}	%{buildroot}%{_libdir}/libtpl-egl.so.%{TPL_VER_MAJOR}
ln -sf libtpl-egl.so.%{TPL_VER_MAJOR}	%{buildroot}%{_libdir}/libtpl-egl.so

cp -a src/tpl.h				%{buildroot}%{_includedir}/
cp -a pkgconfig/tpl-egl.pc		%{buildroot}%{_libdir}/pkgconfig/

%if "%{TPL_WINSYS}" == "WL"
%makeinstall -C src/wayland_module/gbm_tbm
%endif

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%manifest packaging/libtpl-egl.manifest
%defattr(-,root,root,-)
%{_libdir}/libtpl-egl.so
%{_libdir}/libtpl-egl.so.%{TPL_VER_MAJOR}
%{_libdir}/libtpl-egl.so.%{TPL_VERSION}
%{_libdir}/libtpl-egl.so.%{TPL_VER_FULL}

%files devel
%defattr(-,root,root,-)
%{_includedir}/tpl.h
%{_libdir}/pkgconfig/tpl-egl.pc

%if "%{TPL_WINSYS}" == "WL"
%files -n libgbm_tbm
%{_libdir}/gbm/gbm_tbm.so
%{_libdir}/gbm/libgbm_tbm.so
%{_libdir}/libgbm_tbm.so

%files -n libgbm_tbm-devel
%{_includedir}/gbm_tbm.h
%endif
