#ifndef __GLADE_EDITOR_PROPERTY_H__
#define __GLADE_EDITOR_PROPERTY_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

/*******************************************************************************
                Boiler plate macros (inspired from glade-command.c)
 *******************************************************************************/
/* XXX document me ! */

#define GLADE_MAKE_EPROP_TYPE(func, type, parent)                  \
GType                                                              \
func ## _get_type (void)                                           \
{                                                                  \
  static GType cmd_type = 0;                                       \
                                                                   \
  if (!cmd_type)                                                   \
    {                                                              \
      static const GTypeInfo info = {                              \
        sizeof (type ## Class),                                    \
        (GBaseInitFunc) NULL,                                      \
        (GBaseFinalizeFunc) NULL,                                  \
        (GClassInitFunc) func ## _class_init,                      \
        (GClassFinalizeFunc) NULL,                                 \
        NULL,                                                      \
        sizeof (type),                                             \
        0,                                                         \
        (GInstanceInitFunc) NULL                                   \
      };                                                           \
                                                                   \
      cmd_type = g_type_register_static (parent, #type, &info, 0); \
    }                                                              \
                                                                   \
  return cmd_type;                                                 \
}


#define GLADE_MAKE_EPROP(type, func)                              \
static void                                                       \
func ## _finalize (GObject *object);                              \
static void                                                       \
func ## _load (GladeEditorProperty *me, GladeProperty *property); \
static GtkWidget *                                                \
func ## _create_input (GladeEditorProperty *me);                  \
static void                                                       \
func ## _class_init (gpointer parent_tmp, gpointer notused)       \
{                                                                 \
  GladeEditorPropertyClass *parent = parent_tmp;                  \
  GObjectClass* object_class;                                     \
  object_class = G_OBJECT_CLASS (parent);                         \
  parent->load =  func ## _load;                                  \
  parent->create_input =  func ## _create_input;                  \
  object_class->finalize = func ## _finalize;                     \
}                                                                 \
typedef struct {                                                  \
        GladeEditorPropertyClass cmd;                             \
} type ## Class;                                                  \
GLADE_MAKE_EPROP_TYPE(func, type, GLADE_TYPE_EDITOR_PROPERTY)



#define GLADE_TYPE_EDITOR_PROPERTY            (glade_editor_property_get_type())
#define GLADE_EDITOR_PROPERTY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EDITOR_PROPERTY, GladeEditorProperty))
#define GLADE_EDITOR_PROPERTY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EDITOR_PROPERTY, GladeEditorPropertyClass))
#define GLADE_IS_EDITOR_PROPERTY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EDITOR_PROPERTY))
#define GLADE_IS_EDITOR_PROPERTY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EDITOR_PROPERTY))
#define GLADE_EDITOR_PROPERTY_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_EDITOR_PROPERTY, GladeEditorPropertyClass))


typedef struct _GladeEditorProperty        GladeEditorProperty;
typedef struct _GladeEditorPropertyClass   GladeEditorPropertyClass;
typedef struct _GladeEditorPropertyPrivate GladeEditorPropertyPrivate;

struct _GladeEditorProperty
{
  GtkBox             parent_instance;

  GladeEditorPropertyPrivate *priv;
};

struct _GladeEditorPropertyClass {
  GtkBoxClass  parent_class;

  void        (* load)          (GladeEditorProperty *, GladeProperty *);
  GtkWidget  *(* create_input)  (GladeEditorProperty *);
  void        (* commit)        (GladeEditorProperty *, GValue *);
  void       *(* changed)       (GladeEditorProperty *, GladeProperty *);

  void   (* glade_reserved1)   (void);
  void   (* glade_reserved2)   (void);
  void   (* glade_reserved3)   (void);
  void   (* glade_reserved4)   (void);
};



GType                glade_editor_property_get_type           (void) G_GNUC_CONST;

void                 glade_editor_property_load               (GladeEditorProperty *eprop,
                                                               GladeProperty       *property);

void                 glade_editor_property_load_by_widget     (GladeEditorProperty *eprop,
                                                               GladeWidget         *widget);

void                 glade_editor_property_commit             (GladeEditorProperty *eprop,
                                                               GValue              *value);
void                 glade_editor_property_commit_no_callback (GladeEditorProperty *eprop,
                                                               GValue              *value);
void                 glade_editor_property_set_custom_text    (GladeEditorProperty *eprop,
                                                               const gchar         *custom_text);
const gchar         *glade_editor_property_get_custom_text    (GladeEditorProperty *eprop);
void                 glade_editor_property_set_disable_check  (GladeEditorProperty *eprop,
                                                               gboolean             disable_check);
gboolean             glade_editor_property_get_disable_check  (GladeEditorProperty *eprop);

GtkWidget           *glade_editor_property_get_item_label     (GladeEditorProperty *eprop);
GladePropertyClass  *glade_editor_property_get_pclass         (GladeEditorProperty *eprop);
GladeProperty       *glade_editor_property_get_property       (GladeEditorProperty *eprop);
gboolean             glade_editor_property_loading            (GladeEditorProperty *eprop);

gboolean             glade_editor_property_show_i18n_dialog     (GtkWidget         *parent,
                                                                 gchar            **text,
                                                                 gchar            **context,
                                                                 gchar            **comment,
                                                                 gboolean          *translatable);
gboolean             glade_editor_property_show_resource_dialog (GladeProject      *project, 
                                                                 GtkWidget         *parent, 
                                                                 gchar            **filename);

gboolean             glade_editor_property_show_object_dialog   (GladeProject      *project,
                                                                 const gchar       *title,
                                                                 GtkWidget         *parent, 
                                                                 GType              object_type,
                                                                 GladeWidget       *exception,
                                                                 GladeWidget      **object);

/* Generic eprops */
#define GLADE_TYPE_EPROP_NUMERIC         (glade_eprop_numeric_get_type())
#define GLADE_TYPE_EPROP_ENUM            (glade_eprop_enum_get_type())
#define GLADE_TYPE_EPROP_FLAGS           (glade_eprop_flags_get_type())
#define GLADE_TYPE_EPROP_COLOR           (glade_eprop_color_get_type())
#define GLADE_TYPE_EPROP_NAMED_ICON      (glade_eprop_named_icon_get_type())
#define GLADE_TYPE_EPROP_TEXT            (glade_eprop_text_get_type())
#define GLADE_TYPE_EPROP_BOOL            (glade_eprop_bool_get_type())
#define GLADE_TYPE_EPROP_CHECK           (glade_eprop_check_get_type())
#define GLADE_TYPE_EPROP_UNICHAR         (glade_eprop_unichar_get_type())
#define GLADE_TYPE_EPROP_OBJECT          (glade_eprop_object_get_type())
#define GLADE_TYPE_EPROP_OBJECTS         (glade_eprop_objects_get_type())
GType     glade_eprop_numeric_get_type     (void) G_GNUC_CONST;
GType     glade_eprop_enum_get_type        (void) G_GNUC_CONST;
GType     glade_eprop_flags_get_type       (void) G_GNUC_CONST;
GType     glade_eprop_color_get_type       (void) G_GNUC_CONST;
GType     glade_eprop_named_icon_get_type  (void) G_GNUC_CONST;
GType     glade_eprop_text_get_type        (void) G_GNUC_CONST;
GType     glade_eprop_bool_get_type        (void) G_GNUC_CONST;
GType     glade_eprop_check_get_type       (void) G_GNUC_CONST;
GType     glade_eprop_unichar_get_type     (void) G_GNUC_CONST;
GType     glade_eprop_object_get_type      (void) G_GNUC_CONST;
GType     glade_eprop_objects_get_type     (void) G_GNUC_CONST;


G_END_DECLS

#endif /* __GLADE_EDITOR_PROPERTY_H__ */
