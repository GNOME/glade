/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_COMMAND_H__
#define __GLADE_COMMAND_H__

#include <glib-object.h>
#include "glade-widget.h"

void glade_command_undo (void);
void glade_command_redo (void);
const gchar* glade_command_get_description (GList *l);

void glade_command_set_property (GObject *obj, const gchar* name, const GValue* value);
void glade_command_set_name (GladeWidget *obj, const gchar* name);

void glade_command_delete (GladeWidget *widget);
void glade_command_create (GladeWidget *widget);

void glade_command_cut   (GladeWidget *widget);
void glade_command_paste (GladeWidget *widget, GladePlaceholder *placeholder);

#endif /* GLADE_COMMAND_H */
