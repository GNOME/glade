/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_H__
#define __GLADE_H__

#include <gtk/gtk.h>

/* Borrow from libgnome/libgnome.h */
#ifdef ENABLE_NLS
#    include <libintl.h>
#    ifdef GNOME_EXPLICIT_TRANSLATION_DOMAIN
#        undef _
#        define _(String) dgettext (GNOME_EXPLICIT_TRANSLATION_DOMAIN, String)
#    else 
#        define _(String) gettext (String)
#    endif
#    ifdef gettext_noop
#        define N_(String) gettext_noop (String)
#    else
#        define N_(String) (String)
#    endif
#else
/* Stubs that do something close enough.  */
#    define textdomain(String) (String)
#    define gettext(String) (String)
#    define dgettext(Domain,Message) (Message)
#    define dcgettext(Domain,Message,Type) (Message)
#    define bindtextdomain(Domain,Directory) (Domain)
#    define _(String) (String)
#    define N_(String) (String)
#endif


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


#define g_ok_print g_print
/* I always grep for g_print to find left over debuging print's
 * so for now, use g_ok_print on the code that is ment to do a g_print
 * (like --dump GtkWindow). Later rename to g_print. Chema
 */
#include "glade-types.h"
#include "glade-utils.h"
#include "glade-xml-utils.h"

#define GLADE_PATH_SEP_STR "/"
#define GLADE_TAG_GET_TYPE_FUNCTION "GetTypeFunction"
#define GLADE_TAG_TOPLEVEL     "Toplevel"
#define GLADE_TAG_PLACEHOLDER  "Placeholder"
#define GLADE_TAG_ICON         "Icon"
#define GLADE_TAG_CHILD_PROPERTIES   "ChildProperties"
#define GLADE_TAG_CONTAINER    "Container"
#define GLADE_TAG_GTKARG       "GtkArg"
#define GLADE_TAG_SYMBOL       "Symbol"
#define GLADE_TAG_ENUM         "Enum"
#define GLADE_TAG_ENUMS        "Enums"
#define GLADE_TAG_FLAGS        "Flags"
#define GLADE_TAG_FALSE        "False"
#define GLADE_TAG_TRUE         "True"
#define GLADE_TAG_YES          "Yes"
#define GLADE_TAG_NO           "No"
#define GLADE_TAG_STRING       "String"
#define GLADE_TAG_BOOLEAN      "Boolean"
#define GLADE_TAG_UNICHAR      "Unichar"
#define GLADE_TAG_FLOAT        "Float"
#define GLADE_TAG_INTEGER      "Integer"
#define GLADE_TAG_DOUBLE       "Double"
#define GLADE_TAG_CHOICE       "Choice"
#define GLADE_TAG_OTHER_WIDGETS "OtherWidgets"
#define GLADE_TAG_OBJECT       "Object"
#define GLADE_TAG_QUESTION     "Question"
#define GLADE_TAG_VISIBLE_LINES "VisibleLines"
#define GLADE_ENUM_DATA_TAG    "GladeEnumDataTag"
#define GLADE_FLAGS_DATA_TAG   "GladeFlagsDataTag"

#define GLADE_TAG_IN_PALETTE   "InPalette"

#define GLADE_TAG_EVENT_HANDLER_CONNECTED "EventHandlerConnected"

#define GLADE_MODIFY_PROPERTY_DATA "GladeModifyPropertyData"

#define GLADE_XML_TAG_PROJECT  "glade-interface"
#define GLADE_XML_TAG_REQUIRES "requires"
#define GLADE_XML_TAG_WIDGET   "widget"
#define GLADE_XML_TAG_PROPERTY "property"
#define GLADE_XML_TAG_CLASS    "class"
#define GLADE_XML_TAG_ID       "id"
#define GLADE_XML_TAG_SIGNAL   "signal"
#define GLADE_XML_TAG_HANDLER  "handler"
#define GLADE_XML_TAG_NAME     "name"
#define GLADE_XML_TAG_CHILD    "child"
#define GLADE_XML_TAG_SIGNAL   "signal"
#define GLADE_XML_TAG_AFTER    "after"
#define GLADE_XML_TAG_PACKING  "packing"
#define GLADE_XML_TAG_PLACEHOLDER "placeholder"
#define GLADE_XML_TAG_INTERNAL_CHILD "internal-child"

/* Used for catalog tags and attributes */
#define GLADE_TAG_GLADE_CATALOG                   "glade-catalog"
#define GLADE_TAG_GLADE_WIDGET_CLASSES            "glade-widget-classes"
#define GLADE_TAG_GLADE_WIDGET_CLASS              "glade-widget-class"
#define GLADE_TAG_GLADE_WIDGET_GROUP              "glade-widget-group"
#define GLADE_TAG_GLADE_WIDGET_CLASS_REF          "glade-widget-class-ref"

#define GLADE_TAG_SIGNAL_NAME                     "signal-name"
#define GLADE_TAG_DEFAULT                         "default"
#define GLADE_TAG_DISABLED                        "disabled"
#define GLADE_TAG_REPLACE_CHILD_FUNCTION          "replace-child-function"
#define GLADE_TAG_POST_CREATE_FUNCTION            "post-create-function"
#define GLADE_TAG_FILL_EMPTY_FUNCTION             "fill-empty-function"
#define GLADE_TAG_GET_INTERNAL_CHILD_FUNCTION     "get-internal-child-function"
#define GLADE_TAG_ADD_CHILD_FUNCTION              "add-child-function"
#define GLADE_TAG_REMOVE_CHILD_FUNCTION           "remove-child-function"
#define GLADE_TAG_GET_CHILDREN_FUNCTION           "get-children-function"
#define GLADE_TAG_GET_ALL_CHILDREN_FUNCTION       "get-all-children-function"
#define GLADE_TAG_CHILD_SET_PROP_FUNCTION         "child-set-property-function"
#define GLADE_TAG_CHILD_GET_PROP_FUNCTION         "child-get-property-function"
#define GLADE_TAG_CHILD_PROPERTY_APPLIES_FUNCTION "child-property-applies-function"
#define GLADE_TAG_PROPERTIES                      "properties"
#define GLADE_TAG_PROPERTY                        "property"
#define GLADE_TAG_TYPE                            "type"
#define GLADE_TAG_SPEC                            "spec"
#define GLADE_TAG_TOOLTIP                         "tooltip"
#define GLADE_TAG_PARAMETERS                      "parameters"
#define GLADE_TAG_PARAMETER                       "parameter"
#define GLADE_TAG_SET_FUNCTION                    "set-function"
#define GLADE_TAG_GET_FUNCTION                    "get-function"
#define GLADE_TAG_VERIFY_FUNCTION                 "verify-function"
#define GLADE_TAG_QUERY                           "query"
#define GLADE_TAG_COMMON                          "common"
#define GLADE_TAG_OPTIONAL                        "optional"
#define GLADE_TAG_OPTIONAL_DEFAULT                "optional-default"
#define GLADE_TAG_VISIBLE                         "visible"
#define GLADE_TAG_GENERIC_NAME                    "generic-name"
#define GLADE_TAG_NAME                            "name"
#define GLADE_TAG_TITLE                           "title"
#define GLADE_TAG_LIBRARY                         "library"
#define GLADE_TAG_ID                              "id"
#define GLADE_TAG_KEY                             "key"
#define GLADE_TAG_VALUE                           "value"
#define GLADE_TAG_CHILD                           "child"
#define GLADE_TAG_CHILDREN                        "children"
#define GLADE_TAG_TRANSLATABLE                    "translatable"
#define GLADE_TAG_PACKING_DEFAULTS                "packing-defaults"
#define GLADE_TAG_PARENT_CLASS                    "parent-class"
#define GLADE_TAG_CHILD_PROPERTY                  "child-property"

LIBGLADEUI_API gboolean glade_verbose;

LIBGLADEUI_API gchar* glade_pixmaps_dir;
LIBGLADEUI_API gchar* glade_catalogs_dir;
LIBGLADEUI_API gchar* glade_modules_dir;
LIBGLADEUI_API gchar* glade_locale_dir;
LIBGLADEUI_API gchar* glade_icon_dir;

#endif /* __GLADE_H__ */
