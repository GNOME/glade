#ifndef __GLADE_WIDGET_H__
#define __GLADE_WIDGET_H__

#include <gladeui/glade-widget-adaptor.h>
#include <gladeui/glade-widget-action.h>
#include <gladeui/glade-signal.h>
#include <gladeui/glade-property.h>

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS
 
#define GLADE_TYPE_WIDGET            (glade_widget_get_type ())
#define GLADE_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_WIDGET, GladeWidget))
#define GLADE_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_WIDGET, GladeWidgetClass))
#define GLADE_IS_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_WIDGET))
#define GLADE_IS_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_WIDGET))
#define GLADE_WIDGET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_WIDGET, GladeWidgetClass))

typedef struct _GladeWidgetClass   GladeWidgetClass;
typedef struct _GladeWidgetPrivate GladeWidgetPrivate;

struct _GladeWidget
{
  GInitiallyUnowned parent_instance;

  GladeWidgetPrivate *priv;
};

struct _GladeWidgetClass
{
  GInitiallyUnownedClass parent_class;

  void         (*add_child)               (GladeWidget *parent, GladeWidget *child, gboolean at_mouse);
  void         (*remove_child)            (GladeWidget *parent, GladeWidget *child);
  void         (*replace_child)           (GladeWidget *parent, GObject *old_object, GObject *new_object);

  void         (*add_signal_handler)      (GladeWidget *widget, GladeSignal *signal_handler);
  void         (*remove_signal_handler)   (GladeWidget *widget, GladeSignal *signal_handler);
  void         (*change_signal_handler)   (GladeWidget *widget, GladeSignal *new_signal_handler);

  gint         (*button_press_event)      (GladeWidget *widget, GdkEvent *event);
  gint         (*button_release_event)    (GladeWidget *widget, GdkEvent *event);
  gint         (*motion_notify_event)     (GladeWidget *widget, GdkEvent *event);

  gboolean     (*event)                   (GladeWidget *gwidget, GdkEvent *event);

  void   (* glade_reserved1)   (void);
  void   (* glade_reserved2)   (void);
  void   (* glade_reserved3)   (void);
  void   (* glade_reserved4)   (void);
  void   (* glade_reserved5)   (void);
  void   (* glade_reserved6)   (void);
  void   (* glade_reserved7)   (void);
  void   (* glade_reserved8)   (void);
};

/* Nameless widgets in fact have a name, except
 * that they are prefixed with this prefix
 */
#define GLADE_UNNAMED_PREFIX   "__glade_unnamed_"

#define IS_GLADE_WIDGET_EVENT(event)       \
        ((event) == GDK_BUTTON_PRESS ||    \
         (event) == GDK_BUTTON_RELEASE ||  \
         (event) == GDK_MOTION_NOTIFY)

/*******************************************************************************
                                  General api
 *******************************************************************************/

GType                   glade_widget_get_type               (void);

GladeWidget            *glade_widget_get_from_gobject       (gpointer          object);

gboolean                glade_widget_add_verify             (GladeWidget      *widget,
                                                             GladeWidget      *child,
                                                             gboolean          user_feedback);

void                    glade_widget_add_child              (GladeWidget      *parent,
                                                             GladeWidget      *child,
                                                             gboolean          at_mouse);

void                    glade_widget_remove_child           (GladeWidget      *parent,
                                                             GladeWidget      *child);

void                    glade_widget_replace                (GladeWidget      *parent,
                                                             GObject          *old_object,
                                                             GObject          *new_object);
 
void                    glade_widget_rebuild                (GladeWidget      *gwidget);
 
GladeWidget            *glade_widget_dup                    (GladeWidget      *template_widget,
                                                             gboolean          exact);

GList                  *glade_widget_get_signal_list        (GladeWidget      *widget);

void                    glade_widget_copy_signals           (GladeWidget      *widget,
                                                             GladeWidget      *template_widget);
void                    glade_widget_copy_properties        (GladeWidget      *widget,
                                                             GladeWidget      *template_widget,
                                                             gboolean          copy_parentless,
                                                             gboolean          exact);

void                    glade_widget_set_packing_properties (GladeWidget      *widget,
                                                             GladeWidget      *container);

GList                  *glade_widget_get_properties         (GladeWidget      *widget);
GList                  *glade_widget_get_packing_properties (GladeWidget      *widget);

GladeProperty          *glade_widget_get_property           (GladeWidget      *widget,
                                                             const gchar      *id_property);
 
GladeProperty          *glade_widget_get_pack_property      (GladeWidget      *widget,
                                                             const gchar      *id_property);

GList                  *glade_widget_dup_properties         (GladeWidget      *dest_widget,
                                                             GList            *template_props,
                                                             gboolean          as_load,
                                                             gboolean          copy_parentless,
                                                             gboolean          exact);

void                    glade_widget_remove_property        (GladeWidget      *widget,
                                                             const gchar      *id_property);
 
void                    glade_widget_show                   (GladeWidget      *widget);
 
void                    glade_widget_hide                   (GladeWidget      *widget);
 
void                    glade_widget_add_signal_handler     (GladeWidget      *widget,
                                                             const GladeSignal      *signal_handler);
 
void                    glade_widget_remove_signal_handler  (GladeWidget      *widget,
                                                             const GladeSignal      *signal_handler);
 
void                    glade_widget_change_signal_handler  (GladeWidget      *widget,
                                                             const GladeSignal      *old_signal_handler,
                                                             const GladeSignal      *new_signal_handler);
 
GPtrArray *             glade_widget_list_signal_handlers   (GladeWidget      *widget,
                                                             const gchar      *signal_name);
 
gboolean                glade_widget_has_decendant          (GladeWidget      *widget,
                                                             GType             type);
 
gboolean                glade_widget_event                  (GladeWidget      *gwidget,
                                                             GdkEvent         *event);

gboolean                glade_widget_placeholder_relation   (GladeWidget      *parent, 
                                                             GladeWidget      *widget);

GladeWidgetAction      *glade_widget_get_action             (GladeWidget *widget,
                                                             const gchar *action_path);

GladeWidgetAction      *glade_widget_get_pack_action        (GladeWidget *widget,
                                                             const gchar *action_path);

GList                  *glade_widget_get_actions            (GladeWidget *widget);
GList                  *glade_widget_get_pack_actions       (GladeWidget *widget);

gboolean                glade_widget_set_action_sensitive   (GladeWidget *widget,
                                                             const gchar *action_path,
                                                             gboolean     sensitive);

gboolean                glade_widget_set_pack_action_sensitive (GladeWidget *widget,
                                                                const gchar *action_path,
                                                                gboolean     sensitive);

gboolean                glade_widget_set_action_visible     (GladeWidget *widget,
                                                             const gchar *action_path,
                                                             gboolean     visible);

gboolean                glade_widget_set_pack_action_visible (GladeWidget *widget,
                                                              const gchar *action_path,
                                                              gboolean     visible);

void                    glade_widget_write                  (GladeWidget     *widget,
                                                             GladeXmlContext *context,
                                                             GladeXmlNode    *node);

void                    glade_widget_write_child            (GladeWidget     *widget,
                                                             GladeWidget     *child,
                                                             GladeXmlContext *context,
                                                             GladeXmlNode    *node);

void                    glade_widget_write_signals          (GladeWidget     *widget,
                                                             GladeXmlContext *context,
                                                             GladeXmlNode    *node);

void                    glade_widget_write_placeholder      (GladeWidget     *parent,
                                                             GObject         *object,
                                                             GladeXmlContext *context,
                                                             GladeXmlNode    *node);
        
GladeWidget            *glade_widget_read                   (GladeProject     *project,
                                                             GladeWidget      *parent,
                                                             GladeXmlNode     *node,
                                                             const gchar      *internal);

void                    glade_widget_read_child             (GladeWidget      *widget,
                                                             GladeXmlNode     *node);


void                    glade_widget_write_special_child_prop (GladeWidget     *parent, 
                                                               GObject         *object,
                                                               GladeXmlContext *context,
                                                               GladeXmlNode    *node);

void                    glade_widget_set_child_type_from_node (GladeWidget         *parent,
                                                               GObject             *child,
                                                               GladeXmlNode        *node);

GladeEditorProperty    *glade_widget_create_editor_property (GladeWidget      *widget,
                                                             const gchar      *property,
                                                             gboolean          packing,
                                                             gboolean          use_command);

gchar                  *glade_widget_generate_path_name     (GladeWidget      *widget);

gboolean                glade_widget_is_ancestor            (GladeWidget      *widget,
                                                             GladeWidget      *ancestor);

G_DEPRECATED
gboolean                glade_widget_depends                (GladeWidget      *widget,
                                                             GladeWidget      *other);

G_DEPRECATED
GdkDevice              *glade_widget_get_device_from_event  (GdkEvent *event);

void                    glade_widget_ensure_name            (GladeWidget      *widget,
                                                             GladeProject     *project,
                                                             gboolean          use_command);

/*******************************************************************************
                      Project, object property references
 *******************************************************************************/
 
void                    glade_widget_add_prop_ref           (GladeWidget      *widget,
                                                             GladeProperty    *property);
 
void                    glade_widget_remove_prop_ref        (GladeWidget      *widget,
                                                             GladeProperty    *property);

GList                  *glade_widget_list_prop_refs         (GladeWidget      *widget);
gboolean                glade_widget_has_prop_refs          (GladeWidget      *widget);

GladeProperty          *glade_widget_get_parentless_widget_ref (GladeWidget  *widget);


GList                  *glade_widget_get_parentless_reffed_widgets (GladeWidget *widget);

/*******************************************************************************
            Functions that deal with properties on the runtime object
 *******************************************************************************/

void                    glade_widget_object_set_property    (GladeWidget      *widget,
                                                             const gchar      *property_name,
                                                             const GValue     *value);

void                    glade_widget_object_get_property    (GladeWidget      *widget,
                                                             const gchar      *property_name,
                                                             GValue           *value);

void                    glade_widget_child_set_property     (GladeWidget      *widget,
                                                             GladeWidget      *child,
                                                             const gchar      *property_name,
                                                             const GValue     *value);

void                    glade_widget_child_get_property     (GladeWidget      *widget,
                                                             GladeWidget      *child,
                                                             const gchar      *property_name,
                                                             GValue           *value);

/*******************************************************************************
                   GladeProperty api convenience wrappers
 *******************************************************************************/
 
gboolean                glade_widget_property_get           (GladeWidget      *widget,
                                                             const gchar      *id_property,
                                                             ...);
 
gboolean                glade_widget_property_set           (GladeWidget      *widget,
                                                             const gchar      *id_property,
                                                             ...);
 
gboolean                glade_widget_pack_property_get      (GladeWidget      *widget,
                                                             const gchar      *id_property,
                                                             ...);
 
gboolean                glade_widget_pack_property_set      (GladeWidget      *widget,
                                                             const gchar      *id_property,
                                                             ...);
 
gboolean                glade_widget_property_reset         (GladeWidget      *widget,
                                                             const gchar      *id_property);
 
gboolean                glade_widget_pack_property_reset    (GladeWidget      *widget,
                                                             const gchar      *id_property);
 
gboolean                glade_widget_property_default       (GladeWidget      *widget,
                                                             const gchar      *id_property);
 
gboolean                glade_widget_property_original_default (GladeWidget      *widget,
                                                                const gchar      *id_property);
 
gboolean                glade_widget_pack_property_default  (GladeWidget      *widget,
                                                             const gchar      *id_property);
 
gboolean                glade_widget_property_set_sensitive (GladeWidget      *widget,
                                                             const gchar      *id_property,
                                                             gboolean          sensitive,
                                                             const gchar      *reason);
 
gboolean                glade_widget_pack_property_set_sensitive (GladeWidget      *widget,
                                                                  const gchar      *id_property,
                                                                  gboolean          sensitive,
                                                                  const gchar      *reason);
 
gboolean                glade_widget_property_set_enabled   (GladeWidget      *widget,
                                                             const gchar      *id_property,
                                                             gboolean          enabled);
 
gboolean                glade_widget_pack_property_set_enabled (GladeWidget      *widget,
                                                                const gchar      *id_property,
                                                                gboolean          enabled);

 
gboolean                glade_widget_property_set_save_always (GladeWidget      *widget,
                                                               const gchar      *id_property,
                                                               gboolean          setting);
 
gboolean                glade_widget_pack_property_set_save_always (GladeWidget      *widget,
                                                                    const gchar      *id_property,
                                                                    gboolean          setting);

gchar                  *glade_widget_property_string        (GladeWidget      *widget,
                                                             const gchar      *id_property,
                                                             const GValue     *value);


gchar                  *glade_widget_pack_property_string   (GladeWidget      *widget,
                                                             const gchar      *id_property,
                                                             const GValue     *value);

/*******************************************************************************
                                  Accessors
 *******************************************************************************/

void                    glade_widget_set_name               (GladeWidget      *widget,
                                                             const gchar      *name);
 
const gchar   *glade_widget_get_name               (GladeWidget      *widget);
const gchar   *glade_widget_get_display_name       (GladeWidget      *widget);
gboolean                glade_widget_has_name               (GladeWidget      *widget);

void                    glade_widget_set_is_composite       (GladeWidget      *widget,
                                                             gboolean          composite);

gboolean                glade_widget_get_is_composite       (GladeWidget      *widget);

void                    glade_widget_set_internal           (GladeWidget      *widget,
                                                             const gchar      *internal);
 
const gchar   *glade_widget_get_internal           (GladeWidget      *widget);

GObject                *glade_widget_get_object             (GladeWidget      *widget);

void                    glade_widget_set_project            (GladeWidget      *widget,
                                                             GladeProject     *project);
 
GladeProject           *glade_widget_get_project            (GladeWidget      *widget);

void                    glade_widget_set_in_project         (GladeWidget      *widget,
                                                             gboolean          in_project);
gboolean                glade_widget_in_project             (GladeWidget      *widget);

GladeWidgetAdaptor     *glade_widget_get_adaptor            (GladeWidget      *widget);
 
GladeWidget            *glade_widget_get_parent             (GladeWidget      *widget);
 
void                    glade_widget_set_parent             (GladeWidget      *widget,
                                                             GladeWidget      *parent);

GList                  *glade_widget_get_children           (GladeWidget* widget);

GladeWidget            *glade_widget_get_toplevel           (GladeWidget      *widget);
 
gboolean                glade_widget_superuser              (void);
 
void                    glade_widget_push_superuser         (void);
 
void                    glade_widget_pop_superuser          (void);

void                    glade_widget_verify                 (GladeWidget      *widget);
void                    glade_widget_set_support_warning    (GladeWidget      *widget,
                                                             const gchar      *warning);
const gchar   *glade_widget_support_warning        (GladeWidget      *widget);

void                    glade_widget_lock                   (GladeWidget      *widget,
                                                             GladeWidget      *locked);
void                    glade_widget_unlock                 (GladeWidget      *widget);

GladeWidget            *glade_widget_get_locker             (GladeWidget      *widget);

GList                  *glade_widget_list_locked_widgets    (GladeWidget      *widget);

void                    glade_widget_support_changed        (GladeWidget      *widget);

GtkTreeModel           *glade_widget_get_signal_model       (GladeWidget      *widget);

GladeWidget            *glade_widget_find_child             (GladeWidget *widget,
                                                             const gchar *name);

G_END_DECLS

#endif /* __GLADE_WIDGET_H__ */
