Name:           parlatype
Version:        1.5
Release:        1%{?dist}
Summary:        GNOME audio player for transcription

License:        GPLv3+
URL:            http://gkarsay.github.io/parlatype/
Source0:        https://github.com/gkarsay/parlatype/releases/download/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  intltool >= 0.50.2
BuildRequires:  gtk3-devel >= 3.10.0
BuildRequires:  gtk-doc
BuildRequires:  glade-devel >= 3.15.0
BuildRequires:  gobject-introspection
BuildRequires:  gstreamer1-devel
BuildRequires:  gstreamer1-plugins-base-devel
BuildRequires:  libxml2
BuildRequires:  perl-File-FcntlLock

Requires:       gstreamer1-plugins-good%{?_isa}

%description
Parlatype is a minimal audio player for manual speech transcription, written for the GNOME desktop environment. It plays audio sources to transcribe them in your favourite text application.

%package        devel
Summary:        Development files for %{name}
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description    devel
The %{name}-devel package contains libraries and header files for
developing applications that use %{name}.

%package        libreoffice-helpers
Summary:        Tools for integrating %{name} with LibreOffice
Requires:       %{name}%{?_isa} = %{version}-%{release}
Requires:       libreoffice-pyuno%(?isa} 

%description    libreoffice-helpers
Script files allowing %{name} to be controlled from within LibreOffice.


%prep
%setup -q


%build
%configure --disable-static --enable-gtk-doc --enable-glade-catalog
%make_build


%install
rm -rf $RPM_BUILD_ROOT
%make_install
find $RPM_BUILD_ROOT -name '*.la' -exec rm -f {} ';'

%find_lang %{name} --all-name --with-gnome

%check
appstream-util validate-relax --nonet $RPM_BUILD_ROOT/%{_datadir}/appdata/com.github.gkarsay.parlatype.appdata.xml
desktop-file-validate $RPM_BUILD_ROOT/%{_datadir}/applications/com.github.gkarsay.parlatype.desktop


%post
/sbin/ldconfig
/bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null || :

%postun
/sbin/ldconfig
if [ $1 -eq 0 ] ; then
    /bin/touch --no-create %{_datadir}/icons/hicolor &>/dev/null
    /usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :
fi

%posttrans
/usr/bin/gtk-update-icon-cache %{_datadir}/icons/hicolor &>/dev/null || :


%files -f %{name}.lang
%license COPYING
%doc README.md INSTALL
%{_bindir}/%{name}
%{_libdir}/libparlatype.so.*
%{_libdir}/girepository-1.0/Parlatype-1.0.typelib
%{_datadir}/appdata/com.github.gkarsay.parlatype.appdata.xml
%{_datadir}/applications/com.github.gkarsay.parlatype.desktop
%{_datadir}/glib-2.0/schemas/com.github.gkarsay.parlatype.gschema.xml
%{_datadir}/icons/hicolor/*/apps/%{name}.png
%{_mandir}/man1/%{name}.1*

%files devel
%doc README.md INSTALL
%{_includedir}/*
%{_libdir}/libparlatype.so
%{_libdir}/pkgconfig/%{name}.pc
%{_datadir}/glade/catalogs/%{name}.xml
%{_datadir}/glade/pixmaps/*
%{_datadir}/gir-1.0/Parlatype-1.0.gir
%{_datadir}/gtk-doc/*

%files libreoffice-helpers
%{_libdir}/libreoffice/share/Scripts/python/*.py*

%changelog
* Mon Jun 26 2017 FeRD (Frank Dana) <ferdnyc AT gmail com> - 1.5-1
- Initial build
