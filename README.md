# Glade

User interface designer for Gtk+ and GNOME

* Web - <https://glade.gnome.org>
* Git - <https://gitlab.gnome.org/GNOME/glade>

Mailing Lists: 

* <glade-users-list@gnome.org> - About using Glade to build applications.
* <glade-devel-list@gnome.org> - About the development of Glade itself.


## General Information

Glade is a RAD tool to enable quick and easy development of user interfaces
for the GTK+ 3 toolkit and the GNOME desktop environment. 

The user interfaces designed in Glade are saved as XML and these can be loaded
by applications dynamically as needed by using GtkBuilder or used directly to
define a new GtkWidget derived object class using Gtk+ new template feature.

By using GtkBuilder, Glade XML files can be used in numerous programming 
languages including C, C++, C#, Vala, Java, Perl, Python, and others. 


This version of Glade targets GTK 3  
If you need to work with GTK 2, you can still use Glade 3.8
([more information](http://blogs.gnome.org/tvb/2011/01/15/the-glade-dl/))

## License

Glade is distributed under the
[GNU General Public License](https://www.gnu.org/licenses/gpl-2.0.en.html),
version 2 (GPL) and
[GNU Library General Public License](https://www.gnu.org/licenses/old-licenses/lgpl-2.0.en.html),
version 2 (LGPL) as described in the COPYING file.

## Manual instalation

Requirements

* C compiler like [gcc](https://gcc.gnu.org/)
* [Meson](http://mesonbuild.org) build system
* [GTK](http://www.gtk.org) 3.24.0 or above
* [libxml](http://xmlsoft.org/) 2.4.1 - used to parse XML files
* libgirepository1.0 - Build-time dependency
* xsltproc - for man pages generation

Optional dependencies:

* glib-networking plugins for TLS support (Needed for survey)
* libwebkit2gtk-4.0 - For Webkit plugin
* python-gi - For Python plugin
* libgjs - For JavaScript plugin

Download sources from git and build using meson/ninja

	# Install dependencies, for example in debian
	sudo apt install gcc meson libgtk-3-dev libxml2-dev libgirepository1.0-dev xsltproc 
	
	# Optional dependencies
	sudo apt install libgjs-dev libwebkit2gtk-4.0-dev python-gi-dev glib-networking

	# Clone the source repository or download tarball
	git clone https://gitlab.gnome.org/GNOME/glade.git

	# Create build directory and configure project
	mkdir glade/build && cd glade/build
	meson --prefix=~/.local

	# Build and install
	ninja
	ninja install

To run it you might need to set up LD_LIBRARY_PATH depending on your
distribution defaults

	LD_LIBRARY_PATH=~/.local/lib/x86_64-linux-gnu/ glade

## Linux

Debian

	apt install glade

Fedora

	yum install glade

Any distribution with Flatpak

	flatpak install flathub org.gnome.Glade

[<img width='240' alt='Download on Flathub' src='https://flathub.org/assets/badges/flathub-badge-i-en.png'/>](https://flathub.org/apps/details/org.gnome.Glade)

## Windows

Available as a
[package](https://packages.msys2.org/package/mingw-w64-x86_64-glade) in
[MSYS2](https://www.msys2.org/)

	pacman -S mingw-w64-x86_64-glade

## OSX

Available as a [package](https://formulae.brew.sh/formula/glade) in
[Brew](https://brew.sh/)

	brew install glade






