#ifndef __GLADE_EDITOR_PROPERTY_H__
#define __GLADE_EDITOR_PROPERTY_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

/*******************************************************************************
                Boiler plate macros (inspired from glade-command.c)
 *******************************************************************************/
/* XXX document me ! */

#define GLADE_MAKE_EPROP(type, func, MODULE, OBJ_NAME)            \
G_DECLARE_FINAL_TYPE (type, func, MODULE, OBJ_NAME, GladeEditorProperty) \
G_DEFINE_TYPE (type, func, GLADE_TYPE_EDITOR_PROPERTY);           \
static void                                                       \
func ## _finalize (GObject *object);                              \
static void                                                       \
func ## _load (GladeEditorProperty *me, GladeProperty *property); \
static GtkWidget *                                                \
func ## _create_input (GladeEditorProperty *me);                  \
static void                                                       \
func ## _class_init (type ## Class *klass)                        \
{                                                                 \
  GladeEditorPropertyClass *ep_class = GLADE_EDITOR_PROPERTY_CLASS (klass); \
  GObjectClass* object_class = G_OBJECT_CLASS (klass);            \
  ep_class->load =  func ## _load;                                \
  ep_class->create_input =  func ## _create_input;                \
  object_class->finalize = func ## _finalize;                     \
}                                                                 \
static void                                                       \
func ## _init (type *self)                                        \
{                                                                 \
}

#define GLADE_TYPE_EDITOR_PROPERTY glade_editor_property_get_type ()
G_DECLARE_DERIVABLE_TYPE (GladeEditorProperty, glade_editor_property, GLADE, EDITOR_PROPERTY, GtkBox)

struct _GladeEditorPropertyClass {
  GtkBoxClass  parent_class;

  void        (* load)          (GladeEditorProperty *eprop, GladeProperty *property);
  GtkWidget  *(* create_input)  (GladeEditorProperty *eprop);
  void        (* commit)        (GladeEditorProperty *eprop, GValue *value);
  void       *(* changed)       (GladeEditorProperty *eprop, GladeProperty *property);

  gpointer padding[4];
};

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
GladePropertyDef    *glade_editor_property_get_property_def   (GladeEditorProperty *eprop);
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
