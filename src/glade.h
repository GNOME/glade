/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_H__
#define __GLADE_H__

#include <gtk/gtk.h>

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

/* Circular header dependancie are fixed here with forward 
 * declarations.
 */
typedef struct _GladeWidget         GladeWidget;
typedef struct _GladeProperty       GladeProperty;
typedef struct _GladeProject        GladeProject;
typedef enum   _GladeItemAppearance GladeItemAppearance;

#include "glade-widget-adaptor.h"
#include "glade-widget.h"
#include "glade-property-class.h"
#include "glade-property.h"
#include "glade-project.h"
#include "glade-app.h"
#include "glade-command.h"
#include "glade-editor.h"
#include "glade-palette.h"
#include "glade-clipboard.h"
#include "glade-project-view.h"
#include "glade-placeholder.h"
#include "glade-utils.h"
#include "glade-builtins.h"
#include "glade-xml-utils.h"
#include "glade-fixed.h"

#define GLADE_TAG_FALSE        "False"
#define GLADE_TAG_TRUE         "True"
#define GLADE_TAG_YES          "Yes"
#define GLADE_TAG_NO           "No"
#define GLADE_ENUM_DATA_TAG    "GladeEnumDataTag"

#define GLADE_LARGE_ICON_SUBDIR "22x22"
#define GLADE_LARGE_ICON_SIZE 22

#define GLADE_SMALL_ICON_SUBDIR "16x16"
#define GLADE_SMALL_ICON_SIZE 16


#define GLADE_TAG_EVENT_HANDLER_CONNECTED "EventHandlerConnected"

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
#define GLADE_XML_TAG_TOPLEVEL "toplevel"

/* Used for catalog tags and attributes */
#define GLADE_TAG_GLADE_CATALOG                   "glade-catalog"
#define GLADE_TAG_GLADE_WIDGET_CLASSES            "glade-widget-classes"
#define GLADE_TAG_GLADE_WIDGET_CLASS              "glade-widget-class"
#define GLADE_TAG_GLADE_WIDGET_GROUP              "glade-widget-group"
#define GLADE_TAG_GLADE_WIDGET_CLASS_REF          "glade-widget-class-ref"

#define GLADE_TAG_LANGUAGE                        "language"
#define GLADE_TAG_ADAPTOR                         "adaptor"
#define GLADE_TAG_LIBRARY                         "library"
#define GLADE_TAG_DEPENDS                         "depends"
#define GLADE_TAG_DOMAIN                          "domain"
#define GLADE_TAG_BOOK                            "book"
#define GLADE_TAG_SIGNAL_NAME                     "signal-name"
#define GLADE_TAG_DEFAULT                         "default"
#define GLADE_TAG_DISABLED                        "disabled"
#define GLADE_TAG_DEFAULT_PALETTE_STATE           "default-palette-state"
#define GLADE_TAG_REPLACE_CHILD_FUNCTION          "replace-child-function"
#define GLADE_TAG_POST_CREATE_FUNCTION            "post-create-function"
#define GLADE_TAG_GET_INTERNAL_CHILD_FUNCTION     "get-internal-child-function"
#define GLADE_TAG_LAUNCH_EDITOR_FUNCTION          "launch-editor-function"
#define GLADE_TAG_ADD_CHILD_FUNCTION              "add-child-function"
#define GLADE_TAG_REMOVE_CHILD_FUNCTION           "remove-child-function"
#define GLADE_TAG_GET_CHILDREN_FUNCTION           "get-children-function"
#define GLADE_TAG_CHILD_SET_PROP_FUNCTION         "child-set-property-function"
#define GLADE_TAG_CHILD_GET_PROP_FUNCTION         "child-get-property-function"
#define GLADE_TAG_CHILD_VERIFY_FUNCTION           "child-verify-function"
#define GLADE_TAG_PROPERTIES                      "properties"
#define GLADE_TAG_PACKING_PROPERTIES              "packing-properties"
#define GLADE_TAG_PROPERTY                        "property"
#define GLADE_TAG_TYPE                            "type"
#define GLADE_TAG_SPEC                            "spec"
#define GLADE_TAG_TOOLTIP                         "tooltip"
#define GLADE_TAG_PARAMETERS                      "parameters"
#define GLADE_TAG_PARAMETER                       "parameter"
#define GLADE_TAG_SET_FUNCTION                    "set-property-function"
#define GLADE_TAG_GET_FUNCTION                    "get-property-function"
#define GLADE_TAG_VERIFY_FUNCTION                 "verify-function"
#define GLADE_TAG_QUERY                           "query"
#define GLADE_TAG_COMMON                          "common"
#define GLADE_TAG_OPTIONAL                        "optional"
#define GLADE_TAG_OPTIONAL_DEFAULT                "optional-default"
#define GLADE_TAG_VISIBLE                         "visible"
#define GLADE_TAG_EXPANDED                        "expanded"
#define GLADE_TAG_GENERIC_NAME                    "generic-name"
#define GLADE_TAG_NAME                            "name"
#define GLADE_TAG_TITLE                           "title"
#define GLADE_TAG_ID                              "id"
#define GLADE_TAG_KEY                             "key"
#define GLADE_TAG_VALUE                           "value"
#define GLADE_TAG_TRANSLATABLE                    "translatable"
#define GLADE_TAG_PACKING_DEFAULTS                "packing-defaults"
#define GLADE_TAG_PARENT_CLASS                    "parent-class"
#define GLADE_TAG_CHILD_PROPERTY                  "child-property"
#define GLADE_TAG_DISPLAYABLE_VALUES              "displayable-values"
#define GLADE_TAG_NICK                            "nick"
#define GLADE_TAG_SPECIAL_CHILD_TYPE              "special-child-type"
#define GLADE_TAG_SAVE                            "save"
#define GLADE_TAG_EDITABLE                        "editable"
#define GLADE_TAG_IGNORE                          "ignore"
#define GLADE_TAG_VISIBLE_LINES                   "visible-lines"
#define GLADE_TAG_RESOURCE                        "resource"
#define GLADE_TAG_INIT_FUNCTION                   "init-function"
#define GLADE_TAG_ATK_ACTION                      "atk-action"
#define GLADE_TAG_ATK_PROPERTY                    "atk-property"
#define GLADE_TAG_FIXED                           "fixed"
#define GLADE_TAG_TRANSFER_ON_PASTE               "transfer-on-paste"
#define GLADE_TAG_WEIGHT                          "weight"
#define GLADE_TAG_ACTION_GROUP                    "action-group"
#define GLADE_TAG_ACTION                          "action"
#define GLADE_TAG_STOCK                           "stock"

#define GLADE_NUMERICAL_STEP_INCREMENT             1
#define GLADE_FLOATING_STEP_INCREMENT              0.01F
#define GLADE_NUMERICAL_PAGE_INCREMENT             10
#define GLADE_NUMERICAL_PAGE_SIZE                  1
#define GLADE_GENERIC_BORDER_WIDTH                 6

LIBGLADEUI_API gboolean glade_verbose;

LIBGLADEUI_API gchar* glade_pixmaps_dir;
LIBGLADEUI_API gchar* glade_scripts_dir;
LIBGLADEUI_API gchar* glade_catalogs_dir;
LIBGLADEUI_API gchar* glade_modules_dir;
LIBGLADEUI_API gchar* glade_plugins_dir;
LIBGLADEUI_API gchar* glade_bindings_dir;
LIBGLADEUI_API gchar* glade_locale_dir;
LIBGLADEUI_API gchar* glade_icon_dir;

#endif /* __GLADE_H__ */
