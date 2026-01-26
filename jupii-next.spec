Name:           jupii-next
Version:        0
Release:        git.%BUILD_DATE%.%COMMIT_HASH%%{?dist}
Summary:        Play media files on UPnP/DLNA devices
License:        MPL-2.0
URL:            https://github.com/mkiol/Jupii
Source0:        Jupii.tar.gz

%define debug_package %{nil}

BuildRequires:	dnf-plugins-core
BuildRequires:	rpmfusion-free-release
BuildRequires:	rpmfusion-nonfree-release
BuildRequires:	gcc
BuildRequires:	gcc-c++
BuildRequires:	cmake
BuildRequires:	libnpupnp
BuildRequires:	lame-devel
BuildRequires:	libupnpp
BuildRequires:	pybind11-devel
BuildRequires:	libcurl-devel
BuildRequires:	expat-devel
BuildRequires:	qt5-qtbase-devel
BuildRequires:	qt5-qtdeclarative-devel
BuildRequires:	qt5-qtmultimedia-devel
BuildRequires:	qt5-qttools-devel
BuildRequires:	qt5-qtimageformats-devel
BuildRequires:	kf5-kirigami2-devel
BuildRequires:	kf5-kcoreaddons-devel
BuildRequires:	kf5-kconfig-devel
BuildRequires:	kf5-ki18n-devel
BuildRequires:	kf5-kiconthemes-devel
BuildRequires:	qt5-qtquickcontrols2-devel
BuildRequires:	libarchive-devel
BuildRequires:	xz-devel
BuildRequires:	ffmpeg-devel
BuildRequires:	gumbo-parser-devel
BuildRequires:	fmt-devel
BuildRequires:	taglib-devel
BuildRequires:	python3-devel
BuildRequires:	catch2-devel
BuildRequires:	chrpath
Requires:	qt5-qtbase
Requires:	qt5-qtdeclarative
Requires:	qt5-qtmultimedia
Requires:	kf5-kirigami2
Requires:	qt5-qtquickcontrols2
Requires:	dnf-plugins-core
Requires:	rpmfusion-free-release
Requires:	rpmfusion-nonfree-release
Requires:	libnpupnp
Requires:	libupnpp
Requires:	lame
Requires:	libcurl
Requires:	expat
Requires:	fmt
Requires:	gumbo-parser
Requires:	taglib
Requires:	python3-pybind11
Requires:	xz
Requires:	xz-libs
Requires:	xz-lzma-compat
Requires:	libarchive
Requires:   python3-ytmusicapi
%if 0%{?fedora}
Requires: ffmpeg
Conflicts: ffmpeg-free
%else
Requires: ffmpeg
%endif

%description
Jupii is a graphical application that allows playing audio, video, and image
files on UPnP/DLNA compatible devices over a local network.

%prep
%autosetup -n Jupii
%if 0%{?fedora} >= 44
    for file in src/logger.cpp src/caster.cpp; do
        sed -i '2i #include <string_view>' $file
        sed -i 's/fmt::format(fmt,/fmt::format(fmt::runtime(fmt),/g' $file
    done
    sed -i '/void Caster::setVideoStreamRotation/,/^}/c\
    void Caster::setVideoStreamRotation(OutCtx*, VideoOrientation)\n\
    {\n\
        // Disabled for FFmpeg 8 compatibility\n\
        return;\n\
    }\n' src/caster.cpp
%endif

%build
%if 0%{?fedora} >= 44
    export CXXFLAGS="%{optflags} -Wno-error=deprecated-declarations -Wno-error=invalid-constexpr"
    export CFLAGS="%{optflags} -Wno-error=deprecated-declarations -Wno-error=invalid-constexpr"
%endif
mkdir -p build
cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_DESKTOP=ON \
  -DCMAKE_C_FLAGS="%{optflags} -Wno-deprecated-declarations -Wno-error=invalid-constexpr" \
  -DCMAKE_CXX_FLAGS="%{optflags} -Wno-deprecated-declarations -Wno-error=invalid-constexpr" \
  -DCMAKE_INSTALL_PREFIX=%{_prefix} \
  -DCMAKE_INSTALL_RPATH= \
  -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=FALSE \
  -DCMAKE_SKIP_RPATH=ON \
  -DCMAKE_BUILD_WITH_INSTALL_RPATH=OFF \
  -DWITH_TRACE_LOGS=OFF \
  -DWITH_SANITIZERS=OFF \
  -DWITH_STATIC_SANITIZERS=OFF \
  -DBUILD_FFMPEG=OFF \
  -DBUILD_FMT=OFF \
  -DBUILD_GUMBO=OFF \
  -DBUILD_UPNPP=OFF \
  -DBUILD_TAGLIB=OFF \
  -DBUILD_PYBIND11=OFF \
  -DBUILD_LIBARCHIVE=OFF \
  -DBUILD_XZ=OFF \
  -DBUILD_CATCH2=OFF
make %{?_smp_mflags}

%install
mkdir -p %{buildroot}%{_bindir}
mkdir -p %{buildroot}%{_datadir}/applications
mkdir -p %{buildroot}%{_datadir}/metainfo
mkdir -p %{buildroot}/usr/share/icons/hicolor
mkdir -p %{buildroot}/usr/share/icons/hicolor/16x16
mkdir -p %{buildroot}/usr/share/icons/hicolor/32x32
mkdir -p %{buildroot}/usr/share/icons/hicolor/48x48
mkdir -p %{buildroot}/usr/share/icons/hicolor/64x64
mkdir -p %{buildroot}/usr/share/icons/hicolor/96x96
mkdir -p %{buildroot}/usr/share/icons/hicolor/128x128
mkdir -p %{buildroot}/usr/share/icons/hicolor/172x172
mkdir -p %{buildroot}/usr/share/icons/hicolor/256x256
mkdir -p %{buildroot}/usr/share/icons/hicolor/512x512
mkdir -p %{buildroot}/usr/share/icons/hicolor/16x16/apps
mkdir -p %{buildroot}/usr/share/icons/hicolor/32x32/apps
mkdir -p %{buildroot}/usr/share/icons/hicolor/48x48/apps
mkdir -p %{buildroot}/usr/share/icons/hicolor/64x64/apps
mkdir -p %{buildroot}/usr/share/icons/hicolor/96x96/apps
mkdir -p %{buildroot}/usr/share/icons/hicolor/128x128/apps
mkdir -p %{buildroot}/usr/share/icons/hicolor/172x172/apps
mkdir -p %{buildroot}/usr/share/icons/hicolor/256x256/apps
mkdir -p %{buildroot}/usr/share/icons/hicolor/512x512/apps
mkdir -p %{buildroot}/usr/share/icons/hicolor/scalable/apps
chrpath -d build/jupii || true
install -Dm0755 build/jupii %{buildroot}%{_bindir}/jupii-next
install -Dm0644 build/jupii.desktop %{buildroot}%{_datadir}/applications/jupii-next.desktop
sed -i 's/^Icon=jupii/Icon=jupii-next/' %{buildroot}%{_datadir}/applications/jupii-next.desktop
sed -i 's/^Exec=jupii/Exec=jupii-next/' %{buildroot}%{_datadir}/applications/jupii-next.desktop
sed -i 's/^Name=Jupii/Name=Jupii Next/' %{buildroot}%{_datadir}/applications/jupii-next.desktop
sed -i 's/^StartupWMClass=net.mkiol.jupii/StartupWMClass=jupii/' %{buildroot}%{_datadir}/applications/jupii-next.desktop
sed -i 's/^Keywords=Jupii;jupii;/Keywords=Jupii;jupii;Jupii-Next;jupii-next;/' %{buildroot}%{_datadir}/applications/jupii-next.desktop
install -Dm0644 build/jupii.metainfo.xml %{buildroot}%{_datadir}/metainfo/jupii-next.metainfo.xml
install -Dm0644 desktop/icons/16x16/jupii.png %{buildroot}%{_datadir}/icons/hicolor/16x16/apps/jupii-next.png
install -Dm0644 desktop/icons/32x32/jupii.png %{buildroot}%{_datadir}/icons/hicolor/32x32/apps/jupii-next.png
install -Dm0644 desktop/icons/48x48/jupii.png %{buildroot}%{_datadir}/icons/hicolor/48x48/apps/jupii-next.png
install -Dm0644 desktop/icons/64x64/jupii.png %{buildroot}%{_datadir}/icons/hicolor/64x64/apps/jupii-next.png
install -Dm0644 desktop/icons/96x96/jupii.png %{buildroot}%{_datadir}/icons/hicolor/96x96/apps/jupii-next.png
install -Dm0644 desktop/icons/128x128/jupii.png %{buildroot}%{_datadir}/icons/hicolor/128x128/apps/jupii-next.png
install -Dm0644 desktop/icons/172x172/jupii.png %{buildroot}%{_datadir}/icons/hicolor/172x172/apps/jupii-next.png
install -Dm0644 desktop/icons/256x256/jupii.png %{buildroot}%{_datadir}/icons/hicolor/256x256/apps/jupii-next.png
install -Dm0644 desktop/icons/512x512/jupii.png %{buildroot}%{_datadir}/icons/hicolor/512x512/apps/jupii-next.png
install -Dm0644 desktop/jupii.svg %{buildroot}%{_datadir}/icons/hicolor/scalable/apps/jupii-next.svg


%post
if [ -x /usr/bin/gtk-update-icon-cache ]; then
  /usr/bin/gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor || :
fi

%postun
if [ -x /usr/bin/gtk-update-icon-cache ]; then
  /usr/bin/gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor || :
fi

%files
%license LICENSE
%doc README.md
%{_bindir}/jupii-next
%{_datadir}/applications/jupii-next.desktop
%{_datadir}/metainfo/*.xml
%{_datadir}/icons/hicolor/*/*/*

%changelog
%autochangelog

