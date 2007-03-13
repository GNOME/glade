dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.52)
AC_INIT([glade3], [3.2.0],
	[http://bugzilla.gnome.org/enter_bug.cgi?product=glade3])
	
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([gladeui/glade.h])
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_SYSTEM

AM_INIT_AUTOMAKE([1.9 foreign no-dist-gzip dist-bzip2])

AM_MAINTAINER_MODE

IT_PROG_INTLTOOL([0.35.0])

AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)
AC_PATH_PROG(DLLTOOL, dlltool)

AC_ISC_POSIX
AC_PROG_CC
AC_DISABLE_STATIC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_LIBTOOL_DLOPEN
AC_LIBTOOL_WIN32_DLL
AM_PROG_LIBTOOL

# If the source code has changed at all, increment GLADE_REVISION
# If any interfaces have been added, removed, or changed, increment GLADE_CURRENT, and set GLADE_REVISION to 0.
# If any interfaces have been added since the last public release, then increment GLADE_AGE.
# If any interfaces have been removed since the last public release, then set GLADE_AGE to 0.
# Reference: http://www.gnu.org/software/libtool/manual.html#Versioning
GLADE_REVISION=0
GLADE_CURRENT=5
GLADE_AGE=0
AC_SUBST(GLADE_REVISION)
AC_SUBST(GLADE_CURRENT)
AC_SUBST(GLADE_AGE)

dnl ================================================================
dnl Gettext stuff.
dnl ================================================================
GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Name of the gettext package.])
AM_GLIB_GNU_GETTEXT

dnl ================================================================
dnl va_copy.
dnl ================================================================
AC_MSG_CHECKING([how to copy va_list])
AC_TRY_LINK([#include <stdarg.h>], [va_list ap1, ap2; va_copy(ap1, ap2);],
        AC_MSG_RESULT([va_copy]),
        [ AH_TEMPLATE([va_copy], [define if va_copy is not available])
        AC_TRY_LINK([#include <stdarg.h>], [va_list ap1, ap2; __va_copy(ap1, ap2);],
            [ AC_DEFINE([va_copy], [__va_copy])
            AC_MSG_RESULT([__va_copy])],
            [ AC_DEFINE([va_copy(dest,src)], [memcpy(&dest,&src,sizeof(va_list))])
            AC_MSG_RESULT([memcpy])]
            )
        ])

dnl ================================================================
dnl Check for gtk-doc
dnl ================================================================
GTK_DOC_CHECK(1.4)

dnl ================================================================
dnl Check for gtk+
dnl ================================================================
PKG_CHECK_MODULES(GTK, [gtk+-2.0 >= 2.10.0  libxml-2.0 >= 2.4.0])
AC_SUBST(GTK_LIBS)
AC_SUBST(GTK_CFLAGS)

dnl ================================================================
dnl Check for optional gnome libs
dnl ================================================================
AC_ARG_ENABLE(gnome,
    AS_HELP_STRING([--disable-gnome], [disable gnome catalog]),
    check_gnome=$enableval, check_gnome=yes)

if test x"$check_gnome" = x"yes"; then
  PKG_CHECK_MODULES(GNOME, [libbonoboui-2.0 libgnomeui-2.0],
    [have_gnome=yes],[have_gnome=no])
  AC_SUBST(GNOME_LIBS)
  AC_SUBST(GNOME_CFLAGS)
else
  have_gnome=no
fi

AM_CONDITIONAL(BUILD_GNOME, test x"$have_gnome" = "xyes")

# ==================================================================
# Glade User Manual (requires gnome-doc-utils)
# ==================================================================

GNOME_DOC_INIT([0.9.0],[],[:])


dnl ================================================================
dnl Python
dnl ================================================================

AC_MSG_CHECKING([whether Python support is requested])
AC_ARG_ENABLE([python],
	AS_HELP_STRING([--enable-python],[Enable python support]),
	[enable_python=$enableval have_python=$enableval],
	[enable_python=autodetect have_python=yes])
AC_MSG_RESULT([$enable_python])

if test "x$have_python" != "xno"; then
	AM_PATH_PYTHON([2.3],[],[no])
	if test "x$PYTHON" = "x:"; then
		have_python=no
	fi
fi

if test "x$have_python" != "xno"; then
	AM_CHECK_PYTHON_HEADERS([],[have_python=no])
fi

if test "x$have_python" != "xno"; then
	PY_PREFIX=`$PYTHON -c 'import sys ; print sys.prefix'`
	PY_EXEC_PREFIX=`$PYTHON -c 'import sys ; print sys.exec_prefix'`
	PYTHON_LIBS="-lpython$PYTHON_VERSION"
	PYTHON_LIB_LOC="-L$PY_EXEC_PREFIX/lib/python$PYTHON_VERSION/config"
	PYTHON_CFLAGS="-I$PY_PREFIX/include/python$PYTHON_VERSION"
	PYTHON_MAKEFILE="$PY_EXEC_PREFIX/lib/python$PYTHON_VERSION/config/Makefile"
	PYTHON_LOCALMODLIBS=`sed -n -e 's/^LOCALMODLIBS=\(.*\)/\1/p' $PYTHON_MAKEFILE`
	PYTHON_BASEMODLIBS=`sed -n -e 's/^BASEMODLIBS=\(.*\)/\1/p' $PYTHON_MAKEFILE`
	PYTHON_OTHER_LIBS=`sed -n -e 's/^LIBS=\(.*\)/\1/p' $PYTHON_MAKEFILE`
	PYTHON_EXTRA_LIBS="$PYTHON_LOCALMODLIBS $PYTHON_BASEMODLIBS $PYTHON_OTHER_LIBS"
	AC_SUBST([PYTHON_LIBS])
	AC_SUBST([PYTHON_LIB_LOC])
	AC_SUBST([PYTHON_CFLAGS])
	AC_SUBST([PYTHON_EXTRA_LIBS])
fi

if test "x$have_python" != "xno"; then
	PYGTK_REQUIRED_MAJOR=2
	PYGTK_REQUIRED_MINOR=10
	PYGTK_REQUIRED_MICRO=0
	
	PYGOBJECT_REQUIRED_MAJOR=2
	PYGOBJECT_REQUIRED_MINOR=12
	PYGOBJECT_REQUIRED_MICRO=0
	
	PYGOBJECT_REQUIRED=$PYGOBJECT_REQUIRED_MAJOR.$PYGOBJECT_REQUIRED_MINOR.$PYGOBJECT_REQUIRED_MICRO
	PYGTK_REQUIRED=$PYGTK_REQUIRED_MAJOR.$PYGTK_REQUIRED_MINOR.$PYGTK_REQUIRED_MICRO
	
	PKG_CHECK_MODULES([PYGTK], [
		pygobject-2.0 >= $PYGOBJECT_REQUIRED
		pygtk-2.0 >= $PYGTK_REQUIRED],
		[],[have_python=no])

	AC_SUBST([PYGTK_CFLAGS])
	AC_SUBST([PYGTK_LIBS])

	AC_DEFINE_UNQUOTED(PYGTK_REQUIRED_MAJOR, $PYGTK_REQUIRED_MAJOR, Pygtk mayor version required)
	AC_DEFINE_UNQUOTED(PYGTK_REQUIRED_MINOR, $PYGTK_REQUIRED_MINOR, Pygtk minor version required)
	AC_DEFINE_UNQUOTED(PYGTK_REQUIRED_MICRO, $PYGTK_REQUIRED_MICRO, Pygtk micro version required)
fi

if test "x$have_python" != "xno"; then
	AC_MSG_CHECKING([for pygtk defs])
	PYGTK_DEFSDIR=`$PKG_CONFIG --variable=defsdir pygtk-2.0`
	AC_MSG_RESULT([$PYGTK_DEFSDIR])

	AC_MSG_CHECKING([for pygtk codegen])
	PYGTK_CODEGEN="$PYTHON `$PKG_CONFIG --variable=codegendir pygtk-2.0`/codegen.py"
	AC_MSG_RESULT([$PYGTK_CODEGEN])

	AC_MSG_CHECKING([for pygtk h2def])
	PYGTK_H2DEF="$PYTHON `$PKG_CONFIG --variable=codegendir pygtk-2.0`/h2def.py"
	AC_MSG_RESULT([$PYGTK_H2DEF])

	AC_SUBST([PYGTK_DEFSDIR])
	AC_SUBST([PYGTK_CODEGEN])
	AC_SUBST([PYGTK_H2DEF])

	dnl Check for -fno-strict-aliasing
	FLAGS="-fno-strict-aliasing"
	save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $FLAGS"
	AC_MSG_CHECKING([whether [$]CC understands $FLAGS])
	AC_TRY_COMPILE([], [], [compiler_has_option=yes], [compiler_has_option=no])
	CFLAGS="$save_CFLAGS"
	AC_MSG_RESULT($compiler_has_option)
	if test $compiler_has_option = yes; then
		NO_STRICT_ALIASING_CFLAGS="$FLAGS"
	fi
	AC_SUBST([NO_STRICT_ALIASING_CFLAGS])
fi

if test "x$have_python" != "xyes"; then
	if test "x$enable_python" = "xyes"; then
		AC_MSG_ERROR([Python not found])
	elif test "x$enable_python" = "xautodetect"; then
		enable_python=no
		AC_MSG_WARN([Python not found, disabling python support])
	fi
elif test "x$enable_python" != "xno"; then
	enable_python=yes
	AC_DEFINE([ENABLE_PYTHON],[1],[Define to compile with python support])
fi

AM_CONDITIONAL([ENABLE_PYTHON],[test "x$enable_python" = "xyes"])

dnl ================================================================
dnl Check for windows
dnl ================================================================
case $host_os in
  *mingw* | pw32* | cygwin*)
    win32=yes
    ;;
  *)
    win32=no
    ;;
esac
AM_CONDITIONAL(WITH_WIN32, test x"$win32" = "xyes")

dnl ================================================================
dnl Only use -Wall if we have gcc
dnl ================================================================
if test "x$GCC" = "xyes"; then
    GCC_WARN_FLAGS="-Wall"
fi

AC_SUBST(GCC_WARN_FLAGS)

AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_CXXFLAGS])
AC_SUBST([AM_LDFLAGS])

AC_CONFIG_FILES([
Makefile
data/gladeui-1.0.pc
data/glade-3.desktop.in
data/Makefile
data/icons/Makefile
data/icons/hicolor/Makefile
data/icons/hicolor/16x16/Makefile
data/icons/hicolor/16x16/apps/Makefile
data/icons/hicolor/22x22/Makefile
data/icons/hicolor/22x22/apps/Makefile
data/icons/hicolor/24x24/Makefile
data/icons/hicolor/24x24/apps/Makefile
data/icons/hicolor/32x32/Makefile
data/icons/hicolor/32x32/apps/Makefile
data/icons/hicolor/48x48/Makefile
data/icons/hicolor/48x48/apps/Makefile
data/icons/hicolor/scalable/Makefile
data/icons/hicolor/scalable/apps/Makefile
gladeui/Makefile
src/Makefile
plugins/Makefile
plugins/gtk+/Makefile
plugins/gtk+/icons/Makefile
plugins/gtk+/icons/16x16/Makefile
plugins/gtk+/icons/22x22/Makefile
plugins/gnome/Makefile
plugins/gnome/icons/Makefile
plugins/gnome/icons/16x16/Makefile
plugins/gnome/icons/22x22/Makefile
bindings/Makefile
bindings/python/Makefile
po/Makefile.in
doc/Makefile
doc/version.xml
help/Makefile
])

AC_OUTPUT

echo "

Configuration:

	Source code location:	${srcdir}
	Compiler:		${CC}
	GnomeUI Catalog:	${have_gnome}
	Python Binding:		${have_python}
	Glade User Manual:	${gdu_cv_have_gdu}
"
