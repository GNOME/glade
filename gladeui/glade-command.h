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

typedef struct _GladeCommand        GladeCommand;
typedef struct _GladeCommandClass   GladeCommandClass;
typedef struct _GladeCommandPrivate GladeCommandPrivate;
typedef struct _GCSetPropData       GCSetPropData;

/**
 * GCSetPropData
 * @property: A #GladeProperty to set
 * @new_value: The new #GValue to assign to @property
 * @old_value: The old #GValue of @property
 *
 * #GladeProperty can be set in a list as one command,
 * for Undo purposes; we store the list of #GCSetPropData with
 * their old and new #GValue.
 */
struct _GCSetPropData {
  GladeProperty *property;
  GValue        *new_value;
  GValue        *old_value;
};

struct _GladeCommand
{
  GObject parent;

  GladeCommandPrivate *priv;
};

struct _GladeCommandClass
{
  GObjectClass parent_class;

  gboolean (* execute)     (GladeCommand *this_cmd);
  gboolean (* undo)        (GladeCommand *this_cmd);
  gboolean (* unifies)     (GladeCommand *this_cmd, GladeCommand *other_cmd);
  void     (* collapse)    (GladeCommand *this_cmd, GladeCommand *other_cmd);

  void   (* glade_reserved1)   (void);
  void   (* glade_reserved2)   (void);
  void   (* glade_reserved3)   (void);
  void   (* glade_reserved4)   (void);
};



GType                 glade_command_get_type             (void);

void                  glade_command_push_group           (const gchar       *fmt,
                                                          ...) G_GNUC_PRINTF (1, 2);
void                  glade_command_pop_group            (void);
gint                  glade_command_get_group_depth      (void);

G_CONST_RETURN gchar *glade_command_description          (GladeCommand      *command);
gint                  glade_command_group_id             (GladeCommand      *command);
gboolean              glade_command_execute              (GladeCommand      *command);
gboolean              glade_command_undo                 (GladeCommand      *command);
gboolean              glade_command_unifies              (GladeCommand      *command,
                                                          GladeCommand      *other);
void                  glade_command_collapse             (GladeCommand      *command,
                                                          GladeCommand      *other);

/************************ project ******************************/
void           glade_command_set_project_target  (GladeProject *project,
                                                  const gchar  *catalog,
                                                  gint          major,
                                                  gint          minor);

void           glade_command_set_project_domain  (GladeProject *project,     
                                                  const gchar  *domain);

void           glade_command_set_project_template(GladeProject *project,     
                                                  GladeWidget  *widget);

void           glade_command_set_project_license (GladeProject *project,
                                                  const gchar  *license);

void           glade_command_set_project_resource_path (GladeProject *project,
                                                        const gchar  *path);

/************************** properties *********************************/

void           glade_command_set_property_enabled(GladeProperty *property,
                                                  gboolean       enabled);

void           glade_command_set_property        (GladeProperty *property,
                                                  ...);

void           glade_command_set_property_value  (GladeProperty *property,
                                                  const GValue  *value);

void           glade_command_set_properties      (GladeProperty *property,
                                                  const GValue  *old_value,
                                                  const GValue  *new_value,
                                                  ...);

void           glade_command_set_properties_list (GladeProject  *project, 
                                                  GList         *props); /* list of GCSetPropData */

/************************** name ******************************/

void           glade_command_set_name      (GladeWidget       *glade_widget, const gchar  *name);


/************************ protection ******************************/

void           glade_command_lock_widget   (GladeWidget   *widget, 
                                            GladeWidget   *locked);

void           glade_command_unlock_widget (GladeWidget   *widget);


/************************ create/add/delete ******************************/

void           glade_command_add           (GList              *widgets,
                                            GladeWidget        *parent,
                                            GladePlaceholder   *placeholder, 
                                            GladeProject       *project,
                                            gboolean            pasting);

void           glade_command_delete        (GList              *widgets);

GladeWidget   *glade_command_create        (GladeWidgetAdaptor *adaptor,
                                            GladeWidget        *parent,
                                            GladePlaceholder   *placeholder,
                                            GladeProject       *project);

/************************ cut/paste/dnd ******************************/

void           glade_command_cut           (GList             *widgets);

void           glade_command_paste         (GList             *widgets,
                                            GladeWidget       *parent,
                                            GladePlaceholder  *placeholder,
                                            GladeProject      *project);

void           glade_command_dnd           (GList             *widgets,
                                            GladeWidget       *parent,
                                            GladePlaceholder  *placeholder);

/************************ signals ******************************/

void           glade_command_add_signal    (GladeWidget       *glade_widget, 
                                            const GladeSignal *signal);

void           glade_command_remove_signal (GladeWidget       *glade_widget, 
                                            const GladeSignal *signal);

void           glade_command_change_signal (GladeWidget       *glade_widget, 
                                            const GladeSignal *old_signal, 
                                            const GladeSignal *new_signal);

/************************ set i18n ******************************/

void           glade_command_set_i18n      (GladeProperty     *property,
                                            gboolean translatable,
                                            const gchar *context,
                                            const gchar *comment);


G_END_DECLS

#endif /* __GLADE_COMMAND_H__ */
