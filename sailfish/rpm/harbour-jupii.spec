Name:       harbour-jupii

# >> macros
%define __provides_exclude_from ^%{_datadir}/.*$
%define __requires_exclude ^libgumbo.*|libomx*|libx264.*|libavdevice.*|libavcodec.*|libavformat.*|libavutil.*|libswresample.*|libswscale.*|libmp3lame.*|libtag.*|libnpupnp.*|libmicrohttpd.*|libupnpp.*$
# << macros

%{!?qtc_qmake:%define qtc_qmake %qmake}
%{!?qtc_qmake5:%define qtc_qmake5 %qmake5}
%{!?qtc_make:%define qtc_make make}
%{?qtc_builddir:%define _builddir %qtc_builddir}
Summary:    Jupii
Version:    2.11.3
Release:    1
Group:      Qt/Qt
License:    LICENSE
URL:        http://mozilla.org/MPL/2.0/
Source0:    %{name}-%{version}.tar.bz2
Source100:  harbour-jupii.yaml
Requires:   sailfishsilica-qt5 >= 0.10.9
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(libcurl)
BuildRequires:  pkgconfig(audioresource)
BuildRequires:  pkgconfig(nemonotifications-qt5)
BuildRequires:  pkgconfig(libpulse)
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  pkgconfig(keepalive)
BuildRequires:  pkgconfig(zlib)
BuildRequires:  pkgconfig(expat)
BuildRequires:  bzip2-devel
BuildRequires:  gcc-c++
BuildRequires:  libstdc++-devel
BuildRequires:  glibc-devel
BuildRequires:  cmake
BuildRequires:  desktop-file-utils

%description
Play audio, video and images on UPnP/DLNA devices


%prep
%setup -q -n %{name}-%{version}

# >> setup
# << setup

%build
# >> build pre
# << build pre

%qtc_qmake5 

%qtc_make %{?_smp_mflags}

# >> build post
# << build post

%install
rm -rf %{buildroot}
# >> install pre
# << install pre
%qmake5_install

# >> install post
# << install post

desktop-file-install --delete-original \
  --dir %{buildroot}%{_datadir}/applications \
   %{buildroot}%{_datadir}/applications/*.desktop

%files
%defattr(-,root,root,-)
%{_bindir}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
# >> files
# << files
