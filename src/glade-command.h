/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_COMMAND_H__
#define __GLADE_COMMAND_H__

#include <glib-object.h>
#include "glade-widget.h"

G_BEGIN_DECLS

void glade_command_undo (void);
void glade_command_redo (void);

const gchar *glade_command_get_description (GList *l);

void glade_command_set_property (GladeProperty *property, const GValue *value);
void glade_command_set_name (GladeWidget *obj, const gchar *name);

void glade_command_delete (GladeWidget *widget);
void glade_command_create (GladeWidgetClass *class, GladePlaceholder *placeholder, GladeProject *project);

void glade_command_cut   (GladeWidget *widget);
void glade_command_paste (GladePlaceholder *placeholder);

G_END_DECLS

#endif /* GLADE_COMMAND_H */
