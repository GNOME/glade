/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_COMMAND_H__
#define __GLADE_COMMAND_H__

#include <gladeui/glade-placeholder.h>
#include <gladeui/glade-widget.h>
#include <gladeui/glade-signal.h>
#include <gladeui/glade-property.h>
#include <gladeui/glade-project.h>
#include <glib-object.h>

G_BEGIN_DECLS


#define GLADE_TYPE_COMMAND            (glade_command_get_type ())
#define GLADE_COMMAND(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_COMMAND, GladeCommand))
#define GLADE_COMMAND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_COMMAND, GladeCommandClass))
#define GLADE_IS_COMMAND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_COMMAND))
#define GLADE_IS_COMMAND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_COMMAND))
#define GLADE_COMMAND_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_COMMAND, GladeCommandClass))

typedef struct _GladeCommand      GladeCommand;
typedef struct _GladeCommandClass GladeCommandClass;
typedef struct _GCSetPropData     GCSetPropData;

struct _GCSetPropData {
	GladeProperty *property;
	GValue        *new_value;
	GValue        *old_value;
};

struct _GladeCommand
{
	GObject parent;

	gchar *description; /* a string describing the command.
			     * It's used in the undo/redo menu entry.
			     */

	gint   group_id;    /* If this is part of a command group, this is
			     * the group id (id is needed only to ensure that
			     * consecutive groups dont get merged).
			     */
};

struct _GladeCommandClass
{
	GObjectClass parent_class;

	gboolean (* execute)     (GladeCommand *this_cmd);
	gboolean (* undo)        (GladeCommand *this_cmd);
	gboolean (* unifies)     (GladeCommand *this_cmd, GladeCommand *other_cmd);
	void     (* collapse)    (GladeCommand *this_cmd, GladeCommand *other_cmd);
};


LIBGLADEUI_API
GType          glade_command_get_type      (void);
LIBGLADEUI_API
void           glade_command_push_group    (const gchar       *fmt,
					    ...);
LIBGLADEUI_API
void           glade_command_pop_group     (void);

LIBGLADEUI_API
gboolean       glade_command_execute       (GladeCommand      *command);
LIBGLADEUI_API
gboolean       glade_command_undo          (GladeCommand      *command);
LIBGLADEUI_API
gboolean       glade_command_unifies       (GladeCommand      *command,
					    GladeCommand      *other);
LIBGLADEUI_API
void           glade_command_collapse      (GladeCommand      *command,
					    GladeCommand      *other);

/************************** properties *********************************/
LIBGLADEUI_API
void           glade_command_set_property        (GladeProperty *property,     
					          ...);
LIBGLADEUI_API
void           glade_command_set_property_value  (GladeProperty *property,     
						  const GValue  *value);
LIBGLADEUI_API
void           glade_command_set_properties      (GladeProperty *property, 
					          const GValue  *old_value, 
					          const GValue  *new_value,
						  ...);
LIBGLADEUI_API
void           glade_command_set_properties_list (GladeProject  *project, 
						  GList         *props); /* list of GCSetPropData */

/************************** name ******************************/
LIBGLADEUI_API
void           glade_command_set_name      (GladeWidget       *glade_widget, const gchar  *name);


/************************ create/delete ******************************/
LIBGLADEUI_API
void           glade_command_delete        (GList              *widgets);
LIBGLADEUI_API
GladeWidget   *glade_command_create        (GladeWidgetAdaptor *adaptor,
					    GladeWidget        *parent,
					    GladePlaceholder   *placeholder,
					    GladeProject       *project);

/************************ cut/copy/paste ******************************/
LIBGLADEUI_API
void           glade_command_cut           (GList             *widgets);
LIBGLADEUI_API
void           glade_command_copy          (GList             *widgets);
LIBGLADEUI_API
void           glade_command_paste         (GList             *widgets,
					    GladeWidget       *parent,
					    GladePlaceholder  *placeholder);
LIBGLADEUI_API
void           glade_command_dnd           (GList             *widgets,
					    GladeWidget       *parent,
					    GladePlaceholder  *placeholder);

/************************ signals ******************************/
LIBGLADEUI_API
void           glade_command_add_signal    (GladeWidget       *glade_widget, 
					    const GladeSignal *signal);
LIBGLADEUI_API
void           glade_command_remove_signal (GladeWidget       *glade_widget, 
					    const GladeSignal *signal);
LIBGLADEUI_API
void           glade_command_change_signal (GladeWidget       *glade_widget, 
					    const GladeSignal *old, 
					    const GladeSignal *new_signal);

/************************ set i18n ******************************/
LIBGLADEUI_API
void           glade_command_set_i18n      (GladeProperty     *property,
                        gboolean translatable,
                        gboolean has_context,
                        const gchar *comment);


G_END_DECLS

#endif /* GLADE_COMMAND_H */
