#ifndef __GLADE_TYPES_H__
#define __GLADE_TYPES_H__


typedef struct _GladePalette       GladePalette;
typedef struct _GladeEditor        GladeEditor;
typedef struct _GladeSignal        GladeSignal;
typedef struct _GladeSignalEditor  GladeSignalEditor;
typedef struct _GladeProject       GladeProject;
typedef struct _GladeClipboard     GladeClipboard;

typedef struct _GladeWidget        GladeWidget;
typedef struct _GladeWidgetClass   GladeWidgetClass;
typedef struct _GladeWidgetClassSignal GladeWidgetClassSignal;
typedef struct _GladePackagingDefault  GladePackingDefault;

typedef struct _GladeProperty      GladeProperty;
typedef struct _GladePropertyCinfo GladePropertyCinfo;
typedef struct _GladePropertyClass GladePropertyClass;
typedef struct _GladePropertyQuery GladePropertyQuery;

typedef struct _GladeParameter     GladeParameter;
typedef struct _GladeChoice        GladeChoice;

typedef struct _GladeProjectView     GladeProjectView;
typedef struct _GladeProjectViewList GladeProjectViewList;
typedef struct _GladeProjectViewTree GladeProjectViewTree;
typedef struct _GladeProjectWindow   GladeProjectWindow;

typedef struct _GladeCatalog       GladeCatalog;
typedef struct _GladeWidgetGroup   GladeWidgetGroup;
typedef struct _GladeCursor        GladeCursor;

typedef struct _GladePlaceholder   GladePlaceholder;
typedef struct _GladePlaceholderClass GladePlaceholderClass;

typedef struct _GladeXmlContext GladeXmlContext;
typedef struct _GladeXmlNode    GladeXmlNode;
typedef struct _GladeXmlDoc     GladeXmlDoc;

#include "glade-parser.h"

#endif /* __GLADE_TYPES_H__ */
