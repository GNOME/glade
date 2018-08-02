#ifndef __GLADE_EDITABLE_H__
#define __GLADE_EDITABLE_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GLADE_TYPE_EDITABLE            (glade_editable_get_type ())
#define GLADE_EDITABLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EDITABLE, GladeEditable))
#define GLADE_EDITABLE_CLASS(obj)      (G_TYPE_CHECK_CLASS_CAST ((obj), GLADE_TYPE_EDITABLE, GladeEditableInterface))
#define GLADE_IS_EDITABLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EDITABLE))
#define GLADE_EDITABLE_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GLADE_TYPE_EDITABLE, GladeEditableInterface))


typedef struct _GladeEditable      GladeEditable; /* Dummy typedef */
typedef struct _GladeEditableIface GladeEditableInterface; /* used by G_DEFINE_INTERFACE */
typedef struct _GladeEditableIface GladeEditableIface; /* keep this symbol for binary compatibility */

typedef enum
{
  GLADE_PAGE_GENERAL,
  GLADE_PAGE_COMMON,
  GLADE_PAGE_PACKING,
  GLADE_PAGE_ATK,
  GLADE_PAGE_QUERY,
  GLADE_PAGE_SIGNAL
} GladeEditorPageType;


struct _GladeEditableIface
{
  GTypeInterface g_iface;

  /* virtual table */
  void          (* load)          (GladeEditable  *editable,
                                   GladeWidget    *widget);
  void          (* set_show_name) (GladeEditable  *editable,
                                   gboolean        show_name);
};

GType        glade_editable_get_type       (void) G_GNUC_CONST;

void         glade_editable_load           (GladeEditable *editable,
                                            GladeWidget   *widget);
void         glade_editable_set_show_name  (GladeEditable *editable,
                                            gboolean       show_name);
GladeWidget *glade_editable_loaded_widget  (GladeEditable *editable);
gboolean     glade_editable_loading        (GladeEditable *editable);

void         glade_editable_block          (GladeEditable *editable);
void         glade_editable_unblock        (GladeEditable *editable);

G_END_DECLS

#endif /* __GLADE_EDITABLE_H__ */
