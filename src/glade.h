/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_H__
#define __GLADE_H__

#include <gtk/gtk.h>
#include <libintl.h>

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


#define g_ok_print g_print
/* I always grep for g_print to find left over debuging print's
 * so for now, use g_ok_print on the code that is ment to do a g_print
 * (like --dump GtkWindow). Later rename to g_print. Chema
 */
#include "glade-types.h"
#include "glade-utils.h"
#include "glade-xml-utils.h"

#define GLADE_PATH_SEP_STR "/"
#define GLADE_TAG_GLADE_WIDGET_CLASS "GladeWidgetClass"
#define GLADE_TAG_GET_TYPE_FUNCTION "GetTypeFunction"
#define GLADE_TAG_GENERIC_NAME "GenericName"
#define GLADE_TAG_NAME         "Name"
#define GLADE_TAG_ID           "Id"
#define GLADE_TAG_KEY          "Key"
#define GLADE_TAG_VALUE        "Value"
#define GLADE_TAG_TOPLEVEL     "Toplevel"
#define GLADE_TAG_PLACEHOLDER  "Placeholder"
#define GLADE_TAG_ICON         "Icon"
#define GLADE_TAG_PROPERTIES   "Properties"
#define GLADE_TAG_PROPERTY     "Property"
#define GLADE_TAG_COMMON       "Common"
#define GLADE_TAG_OPTIONAL     "Optional"
#define GLADE_TAG_OPTIONAL_DEFAULT "OptionalDefault"
#define GLADE_TAG_TYPE         "Type"
#define GLADE_TAG_TOOLTIP      "Tooltip"
#define GLADE_TAG_GTKARG       "GtkArg"
#define GLADE_TAG_PARAMETERS   "Parameters"
#define GLADE_TAG_PARAMETER    "Parameter"
#define GLADE_TAG_SYMBOL       "Symbol"
#define GLADE_TAG_ENUM         "Enum"
#define GLADE_TAG_CHOICE       "Choice"
#define GLADE_TAG_FALSE        "False"
#define GLADE_TAG_TRUE         "True"
#define GLADE_TAG_YES          "Yes"
#define GLADE_TAG_NO           "No"
#define GLADE_TAG_STRING       "String"
#define GLADE_TAG_BOOLEAN      "Boolean"
#define GLADE_TAG_FLOAT        "Float"
#define GLADE_TAG_INTEGER      "Integer"
#define GLADE_TAG_DOUBLE       "Double"
#define GLADE_TAG_CHOICE       "Choice"
#define GLADE_TAG_OTHER_WIDGETS "OtherWidgets"
#define GLADE_TAG_OBJECT       "Object"
#define GLADE_TAG_SET_FUNCTION "SetFunction"
#define GLADE_TAG_GET_FUNCTION "GetFunction"
#define GLADE_TAG_QUERY        "Query"
#define GLADE_TAG_WINDOW_TITLE "WindowTitle"
#define GLADE_TAG_QUESTION     "Question"
#define GLADE_TAG_PARAM_SPEC   "ParamSpec"
#define GLADE_TAG_VISIBLE_LINES "VisibleLines"
#define GLADE_CHOICE_DATA_TAG    "GladeChoiceDataTag"
#define GLADE_TAG_UPDATE_SIGNALS "UpdateSignals"
#define GLADE_TAG_SIGNAL_NAME "SignalName"
#define GLADE_TAG_DEFAULT      "Default"
#define GLADE_TAG_POST_CREATE_FUNCTION "PostCreateFunction"

#define GLADE_TAG_CATALOG      "GladeCatalog"
#define GLADE_TAG_GLADE_WIDGET "GladeWidget"

#define GLADE_WIDGET_DATA_TAG "GladeWidgetDataTag"

#define GLADE_MODIFY_PROPERTY_DATA "GladeModifyPropertyData"

#define GLADE_XML_TAG_PROJECT  "glade-interface"
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


#endif /* __GLADE_H__ */
