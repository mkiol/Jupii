Name:       harbour-jupii

# >> macros
%define harbour %(if [[ "%{_builddir}" == *harbour ]]; then echo 1; else echo 0; fi)
# << macros

%{!?qtc_qmake:%define qtc_qmake %qmake}
%{!?qtc_qmake5:%define qtc_qmake5 %qmake5}
%{!?qtc_make:%define qtc_make make}
%{?qtc_builddir:%define _builddir %qtc_builddir}
Summary:    Jupii
Version:    2.14.3
Release:    1
Group:      Qt/Qt
License:    LICENSE
URL:        http://mozilla.org/MPL/2.0/
Source0:    %{name}-%{version}.tar.bz2
Requires:   sailfishsilica-qt5 >= 0.10.9
Requires:   python3-gobject
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Sql)
BuildRequires:  pkgconfig(Qt5Multimedia)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(libpulse)
BuildRequires:  pkgconfig(libcurl)
BuildRequires:  pkgconfig(audioresource)
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  pkgconfig(keepalive)
BuildRequires:  pkgconfig(zlib)
BuildRequires:  pkgconfig(expat)
BuildRequires:  pkgconfig(python3)
BuildRequires:  pkgconfig(liblzma)
BuildRequires:  pkgconfig(libarchive)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gstreamer-1.0)
BuildRequires:  pkgconfig(gstreamer-app-1.0)
BuildRequires:  bzip2-devel
BuildRequires:  desktop-file-utils
BuildRequires:  libasan-static
BuildRequires:  libubsan-static
BuildRequires:  xz
BuildRequires:  patch
BuildRequires:  busybox-symlinks-tar
BuildRequires:  python3-pip
BuildRequires:  autoconf
BuildRequires:  automake
BuildRequires:  libtool
BuildRequires:  make
BuildRequires:  cmake
BuildRequires:  m4

%description
Play audio, video and images on UPnP/DLNA devices


%prep
%setup -q -n %{name}-%{version}

# >> setup
# << setup

%build
# >> build pre
# << build pre
%cmake .
make %{?_smp_mflags}

# >> build post
# << build post

%install
%if 0%{?harbour} == 0
echo Build type: normal
%else
echo Build type: harbour
%endif
rm -rf %{buildroot}
# >> install pre
# << install pre
%make_install

# >> install post
# << install post

%if 0%{?harbour} == 0
desktop-file-install --delete-original \
  --dir %{buildroot}%{_datadir}/applications \
   %{buildroot}%{_datadir}/applications/*.desktop
%else
desktop-file-install --delete-original \
  --dir %{buildroot}%{_datadir}/applications \
   %{buildroot}%{_datadir}/applications/%{name}.desktop
%endif

%files
%defattr(-,root,root,-)
%if 0%{?harbour} == 0
%attr(2755, root, privileged) %{_bindir}/%{name}
%endif
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%if 0%{?harbour} == 0
%{_datadir}/applications/%{name}-open-url.desktop
/etc/sailjail/permissions/*.permission
%endif
%{_datadir}/icons/hicolor/*/apps/%{name}.png
# >> files
# << files
