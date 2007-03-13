/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_POPUP_H__
#define __GLADE_POPUP_H__

G_BEGIN_DECLS

void glade_popup_widget_pop (GladeWidget *widget, GdkEventButton *event, gboolean add_children);
void glade_popup_placeholder_pop (GladePlaceholder *placeholder, GdkEventButton *event);
void glade_popup_clipboard_pop (GladeWidget *widget, GdkEventButton *event);

G_END_DECLS

#endif /* __GLADE_POPUP_H__ */
