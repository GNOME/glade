#ifndef __GLADE_PROPERTY_H__
#define __GLADE_PROPERTY_H__

#include <glib-object.h>
#include <gladeui/glade-macros.h>

G_BEGIN_DECLS

#define GLADE_TYPE_PROPERTY            (glade_property_get_type())
#define GLADE_PROPERTY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PROPERTY, GladeProperty))
#define GLADE_PROPERTY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_PROPERTY, GladePropertyClass))
#define GLADE_IS_PROPERTY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PROPERTY))
#define GLADE_IS_PROPERTY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_PROPERTY))
#define GLADE_PROPERTY_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_PROPERTY, GladePropertyClass))

typedef struct _GladePropertyClass   GladePropertyClass;
typedef struct _GladePropertyPrivate GladePropertyPrivate;

typedef enum {
  GLADE_STATE_NORMAL              = 0,
  GLADE_STATE_CHANGED             = (1 << 0),
  GLADE_STATE_UNSUPPORTED         = (1 << 1),
  GLADE_STATE_SUPPORT_DISABLED    = (1 << 2)
} GladePropertyState;

/* A GladeProperty is an instance of a GladePropertyDef.
 * There will be one GladePropertyDef for "GtkLabel->label" but one
 * GladeProperty for each GtkLabel in the GladeProject.
 */
struct _GladeProperty
{
  GObject             parent_instance;

  GladePropertyPrivate *priv;
};


struct _GladePropertyClass
{
  GObjectClass  parent_class;

  /* Class methods */
  GladeProperty *         (* dup)                   (GladeProperty *template_prop, GladeWidget *widget);
  gboolean                (* equals_value)          (GladeProperty *property, const GValue *value);
  gboolean                (* set_value)             (GladeProperty *property, const GValue *value);
  void                    (* get_value)             (GladeProperty *property, GValue *value);
  void                    (* sync)                  (GladeProperty *property);
  void                    (* load)                  (GladeProperty *property);

  /* Signals */
  void             (* value_changed)         (GladeProperty *property, GValue *old_value, GValue *new_value);
  void             (* tooltip_changed)       (GladeProperty *property, const gchar *tooltip, 
                                              const gchar   *insensitive_tooltip, const gchar *support_warning);
  void   (* glade_reserved1)   (void);
  void   (* glade_reserved2)   (void);
  void   (* glade_reserved3)   (void);
  void   (* glade_reserved4)   (void);
  void   (* glade_reserved5)   (void);
  void   (* glade_reserved6)   (void);
};


GLADEUI_EXPORTS
GType                   glade_property_get_type              (void) G_GNUC_CONST;

GLADEUI_EXPORTS
GladeProperty          *glade_property_new                   (GladePropertyDef   *def,
                                                              GladeWidget        *widget,
                                                              GValue             *value);

GLADEUI_EXPORTS
GladeProperty          *glade_property_dup                   (GladeProperty      *template_prop,
                                                              GladeWidget        *widget);

GLADEUI_EXPORTS
void                    glade_property_reset                 (GladeProperty      *property);

GLADEUI_EXPORTS
void                    glade_property_original_reset        (GladeProperty      *property);

GLADEUI_EXPORTS
gboolean                glade_property_default               (GladeProperty      *property);

GLADEUI_EXPORTS
gboolean                glade_property_original_default      (GladeProperty      *property);

GLADEUI_EXPORTS
gboolean                glade_property_equals_value          (GladeProperty      *property, 
                                                              const GValue       *value);

GLADEUI_EXPORTS
gboolean                glade_property_equals                (GladeProperty      *property, 
                                                              ...);

GLADEUI_EXPORTS
gboolean                glade_property_set_value             (GladeProperty      *property, 
                                                              const GValue       *value);

GLADEUI_EXPORTS
gboolean                glade_property_set_va_list           (GladeProperty      *property,
                                                              va_list             vl);

GLADEUI_EXPORTS
gboolean                glade_property_set                   (GladeProperty      *property,
                                                              ...);

GLADEUI_EXPORTS
void                    glade_property_get_value             (GladeProperty      *property, 
                                                              GValue             *value);

GLADEUI_EXPORTS
void                    glade_property_get_default           (GladeProperty      *property, 
                                                              GValue             *value);

GLADEUI_EXPORTS
void                    glade_property_get_va_list           (GladeProperty      *property,
                                                              va_list             vl);

GLADEUI_EXPORTS
void                    glade_property_get                   (GladeProperty      *property, 
                                                              ...);

GLADEUI_EXPORTS
void                    glade_property_add_object            (GladeProperty      *property,
                                                              GObject            *object);

GLADEUI_EXPORTS
void                    glade_property_remove_object         (GladeProperty      *property,
                                                              GObject            *object);

GLADEUI_EXPORTS
void                    glade_property_sync                  (GladeProperty      *property);

GLADEUI_EXPORTS
void                    glade_property_load                  (GladeProperty      *property);

GLADEUI_EXPORTS
void                    glade_property_read                  (GladeProperty      *property,
                                                              GladeProject       *project,
                                                              GladeXmlNode       *node);

GLADEUI_EXPORTS
void                    glade_property_write                 (GladeProperty      *property,        
                                                              GladeXmlContext    *context,
                                                              GladeXmlNode       *node);

GLADEUI_EXPORTS
GladePropertyDef       *glade_property_get_def               (GladeProperty      *property);

GLADEUI_EXPORTS
void                    glade_property_set_sensitive         (GladeProperty      *property,
                                                              gboolean            sensitive,
                                                              const gchar        *reason);
GLADEUI_EXPORTS
const gchar   *glade_propert_get_insensitive_tooltip(GladeProperty      *property);

GLADEUI_EXPORTS
void                    glade_property_set_support_warning   (GladeProperty      *property,
                                                              gboolean            disable,
                                                              const gchar        *reason);
GLADEUI_EXPORTS
const gchar   *glade_property_get_support_warning   (GladeProperty      *property);

GLADEUI_EXPORTS
gboolean                glade_property_warn_usage            (GladeProperty      *property);

GLADEUI_EXPORTS
gboolean                glade_property_get_sensitive         (GladeProperty      *property);


GLADEUI_EXPORTS
void                    glade_property_set_save_always       (GladeProperty      *property,
                                                              gboolean            setting);

GLADEUI_EXPORTS
gboolean                glade_property_get_save_always       (GladeProperty      *property);


GLADEUI_EXPORTS
void                    glade_property_set_enabled           (GladeProperty      *property,
                                                              gboolean            enabled);

GLADEUI_EXPORTS
gboolean                glade_property_get_enabled           (GladeProperty      *property);


GLADEUI_EXPORTS
gchar                  *glade_property_make_string           (GladeProperty      *property);

GLADEUI_EXPORTS
GladeWidget            *glade_property_get_widget            (GladeProperty      *property);
GLADEUI_EXPORTS
void                    glade_property_set_widget            (GladeProperty      *property,
                                                              GladeWidget        *widget);

GLADEUI_EXPORTS
GValue                 *glade_property_inline_value          (GladeProperty      *property);

GLADEUI_EXPORTS
GladePropertyState      glade_property_get_state             (GladeProperty      *property);

GLADEUI_EXPORTS
void                    glade_property_i18n_set_comment      (GladeProperty      *property, 
                                                              const gchar        *str);

GLADEUI_EXPORTS
const gchar   *glade_property_i18n_get_comment      (GladeProperty      *property);

GLADEUI_EXPORTS
void                    glade_property_i18n_set_context      (GladeProperty      *property, 
                                                              const gchar        *str);

GLADEUI_EXPORTS
const gchar   *glade_property_i18n_get_context      (GladeProperty      *property);

GLADEUI_EXPORTS
void                    glade_property_i18n_set_translatable (GladeProperty      *property,
                                                              gboolean            translatable);

GLADEUI_EXPORTS
gboolean                glade_property_i18n_get_translatable (GladeProperty      *property);

GLADEUI_EXPORTS
void                    glade_property_push_superuser        (void);

GLADEUI_EXPORTS
void                    glade_property_pop_superuser         (void);

GLADEUI_EXPORTS
gboolean                glade_property_superuser             (void);

G_END_DECLS

#endif /* __GLADE_PROPERTY_H__ */
