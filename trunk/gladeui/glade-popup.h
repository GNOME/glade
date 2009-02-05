/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_POPUP_H__
#define __GLADE_POPUP_H__

G_BEGIN_DECLS

void glade_popup_widget_pop           (GladeWidget *widget,
				       GdkEventButton *event,
				       gboolean packing);

void glade_popup_placeholder_pop      (GladePlaceholder *placeholder,
				       GdkEventButton *event);

void glade_popup_clipboard_pop        (GladeWidget *widget,
				       GdkEventButton *event);

void glade_popup_palette_pop          (GladeWidgetAdaptor *adaptor,
				       GdkEventButton     *event);

gint glade_popup_action_populate_menu (GtkWidget *menu,
				       GladeWidget *widget,
				       GladeWidgetAction *action,
				       gboolean packing);

void glade_popup_simple_pop           (GdkEventButton *event);

void glade_popup_property_pop         (GladeProperty  *property,
				       GdkEventButton *event);

G_END_DECLS

#endif /* __GLADE_POPUP_H__ */
