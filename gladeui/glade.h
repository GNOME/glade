/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_H__
#define __GLADE_H__

/* symbol export attributes
*/
#ifndef __GNUC__
# define __DLL_IMPORT__	__declspec(dllimport)
# define __DLL_EXPORT__	__declspec(dllexport)
#else
# define __DLL_IMPORT__	__attribute__((dllimport)) extern
# define __DLL_EXPORT__	__attribute__((dllexport)) extern
#endif 

#if defined(__WIN32__) || defined(_WIN32) || defined(WIN32)
# ifdef INSIDE_LIBGLADEUI
#  define LIBGLADEUI_API	__DLL_EXPORT__
# else
#  define LIBGLADEUI_API	__DLL_IMPORT__
# endif
#else
# define LIBGLADEUI_API		extern
#endif

/* fix circular header dependencies with forward declarations.
 */
typedef struct _GladeWidget         GladeWidget;
typedef struct _GladeProperty       GladeProperty;
typedef struct _GladeProject        GladeProject;

#include <gladeui/glade-widget-adaptor.h>
#include <gladeui/glade-widget.h>
#include <gladeui/glade-property-class.h>
#include <gladeui/glade-property.h>
#include <gladeui/glade-project.h>
#include <gladeui/glade-app.h>
#include <gladeui/glade-command.h>
#include <gladeui/glade-editor.h>
#include <gladeui/glade-palette.h>
#include <gladeui/glade-clipboard.h>
#include <gladeui/glade-project-view.h>
#include <gladeui/glade-placeholder.h>
#include <gladeui/glade-utils.h>
#include <gladeui/glade-builtins.h>
#include <gladeui/glade-xml-utils.h>
#include <gladeui/glade-fixed.h>

/* FIXME: should not expose variables in the public interface.
          It would be better if these were implemented as
          private methods on GladeApp.
*/
LIBGLADEUI_API gboolean glade_verbose;
LIBGLADEUI_API gchar* glade_scripts_dir;
LIBGLADEUI_API gchar* glade_catalogs_dir;
LIBGLADEUI_API gchar* glade_modules_dir;
LIBGLADEUI_API gchar* glade_plugins_dir;
LIBGLADEUI_API gchar* glade_bindings_dir;
LIBGLADEUI_API gchar* glade_pixmaps_dir;
LIBGLADEUI_API gchar* glade_locale_dir;

#endif /* __GLADE_H__ */
