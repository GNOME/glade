#ifndef __GLADE_STRING_LIST_H__
#define __GLADE_STRING_LIST_H__

#include <glib-object.h>
#include <gladeui/glade.h>

G_BEGIN_DECLS


#define GLADE_TYPE_EPROP_STRING_LIST       (glade_eprop_string_list_get_type())
#define GLADE_TYPE_STRING_LIST             (glade_string_list_get_type())

/* The GladeStringList boxed type is a GList * of GladeString structs */
typedef struct _GladeString             GladeString;

struct _GladeString {
  gchar    *string;
  gchar    *comment;
  gchar    *context;
  gchar    *id;
  gboolean  translatable;
};

GType        glade_eprop_string_list_get_type    (void) G_GNUC_CONST;
GType        glade_string_list_get_type          (void) G_GNUC_CONST;

void         glade_string_list_free              (GList         *list);
GList       *glade_string_list_copy              (GList         *list);

GList       *glade_string_list_append            (GList         *list,
                                                  const gchar   *string,
                                                  const gchar   *comment,
                                                  const gchar   *context,
                                                  gboolean       translatable,
                                                  const gchar   *id);

gchar       *glade_string_list_to_string         (GList         *list);

GladeEditorProperty *glade_eprop_string_list_new (GladePropertyDef *pdef,
                                                  gboolean          use_command,
                                                  gboolean          translatable,
                                                  gboolean          with_id);

G_END_DECLS

#endif   /* __GLADE_STRING_LIST_H__ */
