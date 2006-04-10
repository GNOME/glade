/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_CLIPBOARD_H__
#define __GLADE_CLIPBOARD_H__

#include "glade.h"

G_BEGIN_DECLS

#define GLADE_TYPE_CLIPBOARD    (glade_clipboard_get_type ())
#define GLADE_CLIPBOARD(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_CLIPBOARD, GladeClipboard))
#define GLADE_IS_CLIPBOARD(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_CLIPBOARD))

typedef struct _GladeClipboard      GladeClipboard;
typedef struct _GladeClipboardClass GladeClipboardClass;

struct _GladeClipboard {
	GObject __parent__;

	GList     *widgets;     /* A list of GladeWidget's on the clipboard */
	GList     *selection;   /* Selection list of GladeWidget's */
	gboolean   has_selection; /* TRUE if clipboard has selection */
	GtkWidget *view;        /* see glade-clipboard-view.c */
};

struct _GladeClipboardClass {
	GObjectClass __parent__;
};


LIBGLADEUI_API
GType           glade_clipboard_get_type         (void);

LIBGLADEUI_API
GladeClipboard *glade_clipboard_new              (void);
LIBGLADEUI_API
void            glade_clipboard_add              (GladeClipboard *clipboard, 
						  GList          *widgets);
LIBGLADEUI_API
void            glade_clipboard_remove           (GladeClipboard *clipboard, 
						  GList          *widgets);

LIBGLADEUI_API
void            glade_clipboard_selection_add    (GladeClipboard *clipboard, 
						  GladeWidget    *widget);
LIBGLADEUI_API
void            glade_clipboard_selection_remove (GladeClipboard *clipboard, 
						  GladeWidget    *widget);
LIBGLADEUI_API
void            glade_clipboard_selection_clear  (GladeClipboard *clipboard);
LIBGLADEUI_API
gboolean        glade_clipboard_get_has_selection  (GladeClipboard *clipboard);


G_END_DECLS

#endif				/* __GLADE_CLIPBOARD_H__ */
