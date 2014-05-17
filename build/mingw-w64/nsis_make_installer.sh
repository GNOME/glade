#!/bin/sh
#
# nsis_make_installer.sh
# 
# Copyright (C) 2012 - Juan pablo Ugarte
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#

CWD=`pwd`             # save CWD
ARG0=`readlink -f $0` # get absolute path of this script whihc is localted under build/mingw-w64
WORK=`dirname $ARG0`  # work directory
ROOTB=`dirname $WORK` # get Glade source root directory
ROOT=`dirname $ROOTB` # get Glade source root directory

if test -n "$2" -a -d "$2"; then
  WORK=`readlink -f $2`
fi

MINGW_ROOT=$WORK/local # directory where all the win32 precompiled dependencies for building will be downloaded
MINGW_ROOT_BIN=$WORK/usr/i686-w64-mingw32/sys-root/mingw #binaries deps

# SYSTEM ENV
export CC=i686-w64-mingw32-gcc
export CXX=i686-w64-mingw32-g++
export RANLIB=i686-w64-mingw32-ranlib
export PKG_CONFIG_LIBDIR=$MINGW_ROOT/lib/pkgconfig
export PKG_CONFIG_PATH=$MINGW_ROOT/share/pkgconfig
export PKG_CONFIG="pkg-config --define-variable=prefix=$MINGW_ROOT"
export CONFIGURE_ARGS="--host i686-w64-mingw32 --target i686-w64-mingw32 --enable-shared=yes --enable-static=no"

case $1 in
  shell)
    echo "Use \$PKG_CONFIG instead pkg-config and \$CONFIGURE_ARGS as parameters for configure scripts"
    bash
    exit
    ;;
  makedev)
    DEVELOPER="true"
    INCLUDE_DIR=include
    EXCLUDE_FILES=
    ;;
  make)
    DEVELOPER="false"
    EXCLUDE_FILES="( -path lib/pkgconfig -o -name *.dll.a -o -name *.la ) -prune"
    INCLUDE_DIR=
    ;;
  *)
    echo "Usage nsis_make_installer.sh command build_path -- Creates a Glade installer using nsis under build_path (defaults to script location)"
    echo "     shell      Runs a shell with CC and PKG_CONFIG variables set for cross compiling"
    echo "     make       Get deps and create windows installer"
    echo "     makedev    Get deps and create windows installer with dev files"
    exit
    ;;
esac

# Install mingw-w64 if nescesary
if test ! -e /usr/bin/i686-w64-mingw32-gcc; then
sudo apt-get install mingw-w64
fi

# Install nsis if nescesary
if test ! -e /usr/bin/makensis; then
sudo apt-get install nsis
fi

# Install 7z if nescesary
if test ! -e /usr/bin/7z; then
sudo apt-get install p7zip-full
fi

echo cd $WORK
cd $WORK

#this python script is a helper to download binaries from OBS (OpenSuse build service)
if test ! -e download-mingw-rpm.py; then
  wget https://github.com/mkbosmans/download-mingw-rpm/raw/master/download-mingw-rpm.py
fi

if test ! -d $MINGW_ROOT; then
  python3 download-mingw-rpm.py --deps gtk2 hicolor-icon-theme gtk2-devel libxml2-devel
  mv $MINGW_ROOT_BIN $MINGW_ROOT
  if test $DEVELOPER = "true"; then
    python3 download-mingw-rpm.py --deps gtk2 hicolor-icon-theme gtk2-devel libxml2-devel
  else
    python3 download-mingw-rpm.py --deps gtk2 hicolor-icon-theme
  fi
fi

if test ! -e $MINGW_ROOT_BIN/bin/glade-3.exe; then
cd $ROOT && ./autogen.sh --prefix=$MINGW_ROOT_BIN $CONFIGURE_ARGS && make && make install

# rename executables names
if test -e $MINGW_ROOT_BIN/bin/i686-w64-mingw32-glade-3.exe; then
  mv $MINGW_ROOT_BIN/bin/i686-w64-mingw32-glade-3.exe $MINGW_ROOT_BIN/bin/glade-3.exe
fi

fi

if test ! -e $MINGW_ROOT_BIN/bin/glade-3.exe; then
	echo Executable not found! Aborting...
	exit 1
fi

#copy files to installer directory

cp $ROOT/build/mingw-w64/glade.nsi $ROOT/data/icons/glade-3.ico $MINGW_ROOT_BIN

# Move locale files
if test -d $MINGW_ROOT_BIN/lib/locale; then
  cp -r $MINGW_ROOT_BIN/lib/locale/* $MINGW_ROOT_BIN/share/locale/
  rm -rf $MINGW_ROOT_BIN/lib/locale
fi

#change to installer directory
cd $MINGW_ROOT_BIN

# remove locales whitout a glade file
rm -r `find share/locale/ -mindepth 1 -maxdepth 1 -type d -exec test ! -e "{}/LC_MESSAGES/glade3.mo" \; -print`

if test ! -d COPYING; then
cat > COPYING << EOF
Glade - A user interface designer for GTK+ and GNOME.

Copyright © 2001-2006 Ximian, Inc.
Copyright © 2001-2006 Joaquin Cuenca Abela, Paolo Borelli, et al.
Copyright © 2001-2014 Tristan Van Berkom, Juan Pablo Ugarte, et al.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

EOF

cat $ROOT/COPYING.GPL >> COPYING
fi

if test $DEVELOPER = "true"; then

cat > install_files.nsh << EOF
        file /r lib
        file /r etc
        file /r share
        file /r include
EOF
else
cat > install_files.nsh << EOF
        file /r etc
        file /r /x pkgconfig /x *.dll.a /x *.la lib
        file /r share
EOF
fi

# Create a list of files to delete in the uninstaller
# Note that we have to reverse the list to remove the directories once they are empty
find bin etc lib share $INCLUDE_DIR \
        $EXCLUDE_FILES \
        -o -type f -printf "delete \"\$INSTDIR\\\%p\"\n" -or -type d -printf "rmDir \"\$INSTDIR\\\%p\"\n" \
        | sed 'y/\//\\/' | tac > uninstall_files.nsh

# create installer
makensis glade.nsi

rm install_files.nsh uninstall_files.nsh

#move installer in build directory
mv glade-*-installer.exe $CWD

#go back to start
cd $CWD
