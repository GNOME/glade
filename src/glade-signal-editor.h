/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_SIGNAL_EDITOR_H__
#define __GLADE_SIGNAL_EDITOR_H__

G_BEGIN_DECLS

#define GLADE_SIGNAL_EDITOR(e) ((GladeSignalEditor *)e)
#define GLADE_IS_SIGNAL_EDITOR(e) (e != NULL)

/* The GladeSignalEditor is used to 
 * 
 * 
 */
struct _GladeSignalEditor {
	
	GtkWidget *main_window;  /* A vbox where all the widgets are added */

	GladeWidget *widget;
	GladeWidgetClass *class;

	GtkWidget *signals_list;
	GtkWidget *signals_name_entry;
	GtkWidget *signals_handler_entry;
	GtkWidget *signals_after_button; /* A GtkToggleButton */

	/* Buttons */
	GtkWidget *add_button;
	GtkWidget *update_button;
	GtkWidget *delete_button;
	GtkWidget *clear_button;
};

GtkWidget *         glade_signal_editor_get_widget (GladeSignalEditor *editor);
GladeSignalEditor * glade_signal_editor_new (void);
void                glade_signal_editor_load_widget (GladeSignalEditor *editor, GladeWidget *widget);

G_END_DECLS

#endif /* __GLADE_SIGNAL_EDITOR_H__ */
