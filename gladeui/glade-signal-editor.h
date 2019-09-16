#ifndef __GLADE_SIGNAL_EDITOR_H__
#define __GLADE_SIGNAL_EDITOR_H__

#include <gladeui/glade.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GLADE_TYPE_SIGNAL_EDITOR glade_signal_editor_get_type ()
G_DECLARE_DERIVABLE_TYPE (GladeSignalEditor, glade_signal_editor, GLADE, SIGNAL_EDITOR, GtkBox)

/* The GladeSignalEditor is used to house the signal editor interface and
 * associated functionality.
 */

struct _GladeSignalEditorClass
{
  GtkBoxClass parent_class;

  gchar ** (* callback_suggestions) (GladeSignalEditor *editor, GladeSignal *signal);
  gchar ** (* detail_suggestions) (GladeSignalEditor *editor, GladeSignal *signal);

  gpointer padding[4];
};

GladeSignalEditor *glade_signal_editor_new                    (void);
void               glade_signal_editor_load_widget            (GladeSignalEditor *editor,
                                                               GladeWidget       *widget);
GladeWidget       *glade_signal_editor_get_widget             (GladeSignalEditor *editor);

void               glade_signal_editor_enable_dnd             (GladeSignalEditor *editor,
                                                               gboolean           enabled);

G_END_DECLS

#endif /* __GLADE_SIGNAL_EDITOR_H__ */
