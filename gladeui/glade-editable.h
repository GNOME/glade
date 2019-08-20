#ifndef __GLADE_EDITABLE_H__
#define __GLADE_EDITABLE_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GLADE_TYPE_EDITABLE glade_editable_get_type ()
G_DECLARE_INTERFACE (GladeEditable, glade_editable, GLADE, EDITABLE, GtkWidget)

typedef enum
{
  GLADE_PAGE_GENERAL,
  GLADE_PAGE_COMMON,
  GLADE_PAGE_PACKING,
  GLADE_PAGE_ATK,
  GLADE_PAGE_QUERY,
  GLADE_PAGE_SIGNAL
} GladeEditorPageType;


struct _GladeEditableInterface
{
  GTypeInterface g_iface;

  /* virtual table */
  void          (* load)          (GladeEditable  *editable,
                                   GladeWidget    *widget);
  void          (* set_show_name) (GladeEditable  *editable,
                                   gboolean        show_name);
};

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
