/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_COMMAND_H__
#define __GLADE_COMMAND_H__

#include <glib-object.h>
#include "glade-widget.h"

void glade_command_undo (void);
void glade_command_redo (void);
const gchar* glade_command_get_description (GList *l);

void glade_cmd_set_property (GObject *obj, const gchar* name, const GValue* value);
void glade_cmd_delete (GladeWidget *widget);
void glade_cmd_create (GladeWidget *widget);

#endif /* GLADE_COMMAND_H */
