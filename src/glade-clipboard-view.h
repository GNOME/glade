#ifndef __GLADE_CLIPBOARD_VIEW_H__
#define __GLADE_CLIPBOARD_VIEW_H__

G_BEGIN_DECLS
#define GLADE_CLIPBOARD_VIEW(obj)           (GTK_CHECK_CAST (obj, glade_clipboard_view_get_type (), GladeClipboardView))
#define GLADE_CLIPBOARD_VIEW_CLASS(klass) (GTK_CHECK_CLASS_CAST (klass, glade_clipboard_view_get_type (), GladeClipboardViewClass))
#define GLADE_IS_CLIPBOARD_VIEW(obj)        (GTK_CHECK_TYPE (obj, glade_clipboard_view_get_type ()))
typedef struct _GladeClipboardView GladeClipboardView;
typedef struct _GladeClipboardViewClass GladeClipboardViewClass;

struct _GladeClipboardView {
	GtkWindow __parent__;

	GtkWidget *widget;	   /* The GtkTreeView widget */
	GtkTreeStore *model;	   /* The GtkTreeStore model for the View */
	GladeClipboard *clipboard; /* The Clipboard for which this is a view */
};

struct _GladeClipboardViewClass {
	GtkWindowClass __parent__;
};

GtkType glade_clipboard_view_get_type ();
GtkWidget *glade_clipboard_view_new (GladeClipboard * clipboard);

void glade_clipboard_view_add (GladeClipboardView * view,
			       GladeWidget * widget);
void glade_clipboard_view_remove (GladeClipboardView * view,
				  GladeWidget * widget);

G_END_DECLS
#endif				/* __GLADE_CLIPBOARD_VIEW_H__ */
