#ifndef __GLADE_CLIPBOARD_H__
#define __GLADE_CLIPBOARD_H__

G_BEGIN_DECLS

#define GLADE_TYPE_CLIPBOARD    (glade_clipboard_get_type ())
#define GLADE_CLIPBOARD(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_CLIPBOARD, GladeClipboard))
#define GLADE_IS_CLIPBOARD(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_CLIPBOARD))

typedef struct _GladeClipboardClass GladeClipboardClass;

struct _GladeClipboard {
	GObject __parent__;

	GList *widgets;		/* A list of GladeWidget's on the clipboard */

	GladeWidget *curr;	/* The currently selected GladeWidget in the
				 * Clipboard
				 */

	GtkWidget *view;	/* see glade-clipboard-view.c */
};

struct _GladeClipboardClass {
	GObjectClass __parent__;
};


GType glade_clipboard_get_type ();

GladeClipboard *glade_clipboard_new ();

void glade_clipboard_add (GladeClipboard *clipboard, GladeWidget *widget);

void glade_clipboard_add_copy (GladeClipboard *clipboard, GladeWidget *widget);

void glade_clipboard_remove (GladeClipboard *clipboard, GladeWidget *widget);


G_END_DECLS

#endif				/* __GLADE_CLIPBOARD_H__ */
