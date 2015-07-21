%define TPL_VER_MAJOR	0
%define TPL_VER_MINOR	3
%define TPL_RELEASE	2
%define TPL_VERSION	%{TPL_VER_MAJOR}.%{TPL_VER_MINOR}
%define TPL_VER_FULL	%{TPL_VERSION}.%{TPL_RELEASE}

%define WINSYS_DRI2	0
%define WINSYS_DRI3	0
%define WINSYS_WL	1

%define ENABLE_TTRACE	0

################################################################################

Name:			libtpl-egl
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
Group:			System/Libraries
License:		MIT
Source:			%{name}-%{version}.tar.gz

BuildRequires:		pkg-config
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

%if "%{WINSYS_WL}" == "1"
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
%if "%{WINSYS_WL}" == "1"
make -C src/wayland_module/gbm_tbm all
%endif

%if "%{WINSYS_DRI2}" == "1"
export TPL_OPTIONS=${TPL_OPTIONS}-winsys_dri2
%endif
%if "%{WINSYS_DRI3}" == "1"
export TPL_OPTIONS=${TPL_OPTIONS}-winsys_dri3
%endif
%if "%{WINSYS_WL}" == "1"
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
cp -a src/tpl_internal.h		%{buildroot}%{_includedir}/
cp -a src/tpl_utils.h			%{buildroot}%{_includedir}/
%if "%{WINSYS_DRI2}" == "1"
cp -a src/tpl_x11_internal.h		%{buildroot}%{_includedir}/
%endif
%if "%{WINSYS_DRI3}" == "1"
cp -a src/tpl_x11_internal.h		%{buildroot}%{_includedir}/
%endif
cp -a pkgconfig/tpl-egl.pc		%{buildroot}%{_libdir}/pkgconfig/

%if "%{WINSYS_WL}" == "1"
%makeinstall -C src/wayland_module/gbm_tbm
%endif

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_libdir}/libtpl-egl.so
%{_libdir}/libtpl-egl.so.%{TPL_VER_MAJOR}
%{_libdir}/libtpl-egl.so.%{TPL_VERSION}
%{_libdir}/libtpl-egl.so.%{TPL_VER_FULL}

%files devel
%defattr(-,root,root,-)
%{_includedir}/tpl.h
%{_includedir}/tpl_internal.h
%{_includedir}/tpl_utils.h
%if "%{WINSYS_DRI2}" == "1"
%{_includedir}/tpl_x11_internal.h
%endif
%if "%{WINSYS_DRI3}" == "1"
%{_includedir}/tpl_x11_internal.h
%endif
%{_libdir}/pkgconfig/tpl-egl.pc

%if "%{WINSYS_WL}" == "1"
%files -n libgbm_tbm
%{_libdir}/gbm/gbm_tbm.so
%{_libdir}/gbm/libgbm_tbm.so
%{_libdir}/libgbm_tbm.so

%files -n libgbm_tbm-devel
%{_includedir}/gbm_tbm.h
%endif
