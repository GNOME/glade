/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#ifndef GLADE_KEYS_DIALOG_H
#define GLADE_KEYS_DIALOG_H

#include <gtk/gtkdialog.h>
#include <gtk/gtkliststore.h>

G_BEGIN_DECLS


#define GLADE_TYPE_KEYS_DIALOG            (glade_keys_dialog_get_type ())
#define GLADE_KEYS_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_KEYS_DIALOG, GladeKeysDialog))
#define GLADE_KEYS_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_KEYS_DIALOG, GladeKeysDialogClass))
#define GLADE_IS_KEYS_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_KEYS_DIALOG))
#define GLADE_IS_KEYS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_KEYS_DIALOG))
#define GLADE_KEYS_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_KEYS_DIALOG, GladeKeysDialogClass))


typedef struct _GladeKeysDialog       GladeKeysDialog;
typedef struct _GladeKeysDialogClass  GladeKeysDialogClass;

struct _GladeKeysDialog
{
	GtkDialog dialog; /* is a dialog */

	GtkListStore *store; /* model */

	GtkWidget *list;     /* view */
};

struct _GladeKeysDialogClass
{
	GtkDialogClass parent_class;
};


GType glade_keys_dialog_get_type (void);

GtkWidget * glade_keys_dialog_new (void);

gchar * glade_keys_dialog_get_selected_key_symbol (GladeKeysDialog *dialog);


G_END_DECLS

#endif /* GLADE_KEYS_DIALOG_H */
