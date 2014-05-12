dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.52)

m4_define(glade_major_version, 3)
m4_define(glade_minor_version, 8)
m4_define(glade_micro_version, 5)
m4_define(glade_version, glade_major_version.glade_minor_version.glade_micro_version)

AC_INIT([glade3], [glade_version],
	[http://bugzilla.gnome.org/enter_bug.cgi?product=glade3])
	
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([gladeui/glade.h])
AC_CONFIG_MACRO_DIR([m4])

AC_DEFINE(GLADE_MAJOR_VERSION, glade_major_version, [Glade major version])
AC_SUBST(GLADE_MAJOR_VERSION, glade_major_version)
AC_DEFINE(GLADE_MINOR_VERSION, glade_minor_version, [Glade minor version])
AC_SUBST(GLADE_MINOR_VERSION, glade_minor_version)
AC_DEFINE(GLADE_MICRO_VERSION, glade_micro_version, [Glade micro version])
AC_SUBST(GLADE_MICRO_VERSION, glade_micro_version)

AC_CANONICAL_SYSTEM

AM_INIT_AUTOMAKE([1.9 foreign no-dist-gzip dist-bzip2])

AM_MAINTAINER_MODE

# Support silent build rules, requires at least automake-1.11. Disable
# by either passing --disable-silent-rules to configure or passing V=1
# to make
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

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
GLADE_CURRENT=13
GLADE_AGE=2
GLADE_CURRENT_MINUS_AGE=`expr $GLADE_CURRENT - $GLADE_AGE`
AC_SUBST(GLADE_REVISION)
AC_SUBST(GLADE_CURRENT)
AC_SUBST(GLADE_AGE)
AC_SUBST(GLADE_CURRENT_MINUS_AGE)

dnl are we unstable or stable?
m4_define([glade_unstable],
          m4_if(m4_eval(glade_minor_version % 2), [1], [yes], [no]))
GLADE_UNSTABLE=glade_unstable
AC_SUBST(GLADE_UNSTABLE)

dnl The symbol GLADE_UNSTABLE is defined above for substitution in
dnl Makefiles and conditionally defined here as a preprocessor symbol
dnl and automake conditional.
if test "x$GLADE_UNSTABLE" = "xyes"; then
  AC_DEFINE(GLADE_UNSTABLE, 1,
            [Define to 1 if this is an unstable version of Glade])
fi
AM_CONDITIONAL(GLADE_UNSTABLE, test "x$GLADE_UNSTABLE" = "xyes")

# ================================================================

GNOME_COMMON_INIT
GNOME_DEBUG_CHECK
GNOME_COMPILE_WARNINGS([maximum])
GNOME_MAINTAINER_MODE_DEFINES

# For the plugins, we don't use the warning flags defined by GNOME_COMPILE_WARNINGS.
# These flags include -Wmissing-prototypes, which we prefer not to use.
if test "x$GCC" = "xyes"; then
    PLUGINS_WARN_CFLAGS="-Wall -Wnested-externs -Wpointer-arith"
fi
AC_SUBST(PLUGINS_WARN_CFLAGS)

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
GTK_DOC_CHECK(1.9)

dnl ================================================================
dnl Check for gtk+
dnl ================================================================
PKG_CHECK_MODULES(GTK, [gtk+-2.0 >= 2.24 gmodule-2.0 gthread-2.0 libxml-2.0 >= 2.4.0])
AC_SUBST(GTK_LIBS)
AC_SUBST(GTK_CFLAGS)

dnl ================================================================
dnl Check for the unix print widgets in gtk+
dnl ================================================================
LIBS=$GTK_LIBS
AC_CHECK_FUNCS(gtk_print_unix_dialog_new,[have_unix_print=yes]; break,[have_unix_print=no])
AM_CONDITIONAL(HAVE_GTK_UNIX_PRINT, test x"$have_unix_print" = "xyes")

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


dnl ================================================================
dnl Python for optional python dev libs
dnl ================================================================
AC_ARG_ENABLE(python,
    AS_HELP_STRING([--disable-python], [disable python catalog]),
    check_python=$enableval, check_python=yes)

if test x"$check_python" = x"yes"; then
  PYGTK_REQUIRED_MAJOR=2
  PYGTK_REQUIRED_MINOR=10
  PYGTK_REQUIRED_MICRO=0
  PKG_CHECK_MODULES(PYGTK, [pygtk-2.0 >= 2.10.0], [have_python=yes],[have_python=no])

  AM_CHECK_PYTHON_HEADERS(, [have_python_headers=no])
  AM_CHECK_PYTHON_LIBS(, [have_python_lib=no])

  if test x"$have_python_headers" = x"no"; then
    have_python=no
  fi

  if test x"$have_python_lib" = x"no"; then
    have_python=no
  fi


  AC_DEFINE_UNQUOTED(PYGTK_REQUIRED_MAJOR, $PYGTK_REQUIRED_MAJOR, Pygtk mayor version required)
  AC_DEFINE_UNQUOTED(PYGTK_REQUIRED_MINOR, $PYGTK_REQUIRED_MINOR, Pygtk minor version required)
  AC_DEFINE_UNQUOTED(PYGTK_REQUIRED_MICRO, $PYGTK_REQUIRED_MICRO, Pygtk micro version required)

  AC_SUBST(PYGTK_LIBS)
  AC_SUBST(PYGTK_CFLAGS)
else
  have_python=no
fi

AM_CONDITIONAL(BUILD_PYTHON, test x"$have_python" = "xyes")

# ==================================================================
# Glade User Manual (requires gnome-doc-utils)
# ==================================================================

GNOME_DOC_INIT([0.9.0],[],[:])

dnl ================================================================
dnl Check for windows
dnl ================================================================
case $host_os in
  *mingw* | pw32* | cygwin*)
    platform_win32=yes
    ;;
  *)
    platform_win32=no
    ;;
esac
AM_CONDITIONAL(PLATFORM_WIN32, test x"$platform_win32" = "xyes")

case $host_os in
  *mingw*)
    native_win32=yes
    ;;
  *)
    native_win32=no
    ;;
esac
AM_CONDITIONAL(NATIVE_WIN32, test x"$native_win32" = "xyes")

if test "$native_win32" = "yes"; then
  AC_CHECK_TOOL(WINDRES, windres, no)
  if test "$WINDRES" = no; then
    AC_MSG_ERROR([*** Could not find an implementation of windres in your PATH.])
  fi
fi

dnl ================================================================
dnl Check for GDK Quartz and MacOSX integration package 
dnl ================================================================
_gdk_tgt=`$PKG_CONFIG --variable=target gdk-2.0`
AM_CONDITIONAL([GDK_TARGET_QUARTZ], [test x$_gdk_tgt = xquartz])
if test "x$_gdk_tgt" = xquartz; then 
   PKG_CHECK_MODULES(GTK_MAC, gtk-mac-integration)

   GTK_MAC_BUNDLE_FLAG=

   AC_ARG_ENABLE(mac-bundle,
      AS_HELP_STRING([--enable-mac-bundle], [enable mac bundling]),
      build_bundle=yes, build_bundle=no)

   if test "x$build_bundle" = xyes; then
      AC_MSG_NOTICE([enableing mac bundle..])

      GTK_MAC_BUNDLE_FLAG=-DMAC_BUNDLE
   fi      

   AC_SUBST(GTK_MAC_BUNDLE_FLAG)
   AC_SUBST(GTK_MAC_LIBS)
   AC_SUBST(GTK_MAC_CFLAGS)
fi

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
gladeui/gladeui.rc
src/Makefile
src/glade-3.rc
plugins/Makefile
plugins/gtk+/Makefile
plugins/gtk+/icons/Makefile
plugins/gtk+/icons/16x16/Makefile
plugins/gtk+/icons/22x22/Makefile
plugins/gnome/Makefile
plugins/gnome/icons/Makefile
plugins/gnome/icons/16x16/Makefile
plugins/gnome/icons/22x22/Makefile
plugins/python/Makefile
po/Makefile.in
doc/Makefile
doc/version.xml
help/Makefile
build/mingw-w64/glade.nsi
])

AC_OUTPUT

echo "

Configuration:

	Source code location:	 ${srcdir}
	Compiler:		 ${CC}
	GTK+ UNIX Print Widgets: ${have_unix_print}
	GNOME UI Widgets:	 ${have_gnome}
	PYTHON Widgets support:	 ${have_python}

	Build Reference Manual:  ${enable_gtk_doc}
	Build User Manual:       ${gdu_cv_have_gdu}
"
