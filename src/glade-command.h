/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_COMMAND_H__
#define __GLADE_COMMAND_H__

#include <glib-object.h>
#include "glade-types.h"

G_BEGIN_DECLS


#define GLADE_TYPE_COMMAND            (glade_command_get_type ())
#define GLADE_COMMAND(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_COMMAND, GladeCommand))
#define GLADE_COMMAND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_COMMAND, GladeCommandClass))
#define GLADE_IS_COMMAND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_COMMAND))
#define GLADE_IS_COMMAND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_COMMAND))
#define GLADE_COMMAND_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_COMMAND, GladeCommandClass))

typedef struct _GladeCommand GladeCommand;
typedef struct _GladeCommandClass GladeCommandClass;


struct _GladeCommand
{
	GObject parent;

	gchar *description; /* a string describing the command.
			     * It's used in the undo/redo menu entry.
			     */
};

struct _GladeCommandClass
{
	GObjectClass parent_class;

	gboolean (* undo_cmd)    (GladeCommand *this);
	gboolean (* execute_cmd) (GladeCommand *this);
	gboolean (* unifies)     (GladeCommand *this, GladeCommand *other);
	void     (* collapse)    (GladeCommand *this, GladeCommand *other);
};


GType glade_command_get_type (void);

void glade_command_undo			(GladeProject *project);
void glade_command_redo			(GladeProject *project);

void glade_command_set_property		(GladeProperty *property, const GValue *value);
void glade_command_set_name		(GladeWidget *glade_widget, const gchar *name);

void glade_command_delete		(GladeWidget *glade_widget);
void glade_command_create		(GladeWidgetClass *widget_class, GladePlaceholder *placeholder, GladeProject *project);

void glade_command_cut			(GladeWidget *glade_widget);
void glade_command_copy			(GladeWidget *glade_widget);
void glade_command_paste		(GladePlaceholder *placeholder);

void glade_command_add_signal		(GladeWidget *glade_widget, const GladeSignal *signal);
void glade_command_remove_signal	(GladeWidget *glade_widget, const GladeSignal *signal);


G_END_DECLS

#endif /* GLADE_COMMAND_H */
