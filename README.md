# Glade

A user interface designer for Gtk+ and GNOME

Web: <http://glade.gnome.org>

Mailing Lists: 

* <glade-users-list@gnome.org>
  * For discussions about using Glade to build applications.
* <glade-devel-list@gnome.org>
  * For discussions about the development of Glade itself.

<a href='https://flathub.org/apps/details/org.gnome.Glade'><img width='240' alt='Download on Flathub' src='https://flathub.org/assets/badges/flathub-badge-i-en.png'/></a>

## General Information

Glade is a RAD tool to enable quick and easy development of user interfaces
for the GTK+ 3 toolkit and the GNOME desktop environment. 

The user interfaces designed in Glade are saved as XML and these can be loaded
by applications dynamically as needed by using GtkBuilder or used directly to
define a new GtkWidget derived object class using Gtk+ new template feature.

By using GtkBuilder, Glade XML files can be used in numerous programming 
languages including C, C++, C#, Vala, Java, Perl, Python, and others. 

## About Glade

This version of Glade (Glade >= 3.10) targets GTK+ >= 3.0 and is parallel
installable with Glade 3.8.

If you need to work with Glade projects that target GTK+2, you need an
installation of Glade 3.8 (more information on http://blogs.gnome.org/tvb/2011/01/15/the-glade-dl/)

## License

Glade is distributed under the GNU General Public License (GPL), as described
in the COPYING file.

## Requirements

* GTK+ 3.10.0 or above - <http://www.gtk.org>  
  You also need the glib, pango and atk libraries.
  Make sure you have the devel packages as well, as these will contain the
  header files which you will need to compile C applications.

* libxml 2.4.1 - used to parse the XML files. If you have GNOME 2 you
  should already have this.

## Installation

See the file '[INSTALL](INSTALL)'
