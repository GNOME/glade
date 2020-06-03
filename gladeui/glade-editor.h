#ifndef __GLADE_EDITOR_H__
#define __GLADE_EDITOR_H__

#include <gladeui/glade-signal-editor.h>
#include <gladeui/glade-editable.h>

G_BEGIN_DECLS

#define GLADE_TYPE_EDITOR glade_editor_get_type ()
GLADEUI_EXPORTS
G_DECLARE_DERIVABLE_TYPE (GladeEditor, glade_editor, GLADE, EDITOR, GtkBox)

/* The GladeEditor is a window that is used to display and modify widget
 * properties. The glade editor contains the details of the selected
 * widget for the selected project
 */

struct _GladeEditorClass
{
  GtkBoxClass parent_class;

  gpointer padding[4];
};

GLADEUI_EXPORTS
GladeEditor *glade_editor_new                (void);
GLADEUI_EXPORTS
void         glade_editor_load_widget        (GladeEditor       *editor,
                                              GladeWidget       *widget);
G_DEPRECATED
GLADEUI_EXPORTS
void         glade_editor_show_info          (GladeEditor       *editor);
G_DEPRECATED
GLADEUI_EXPORTS
void         glade_editor_hide_info          (GladeEditor       *editor);

GLADEUI_EXPORTS
void         glade_editor_show_class_field   (GladeEditor       *editor);
GLADEUI_EXPORTS
void         glade_editor_hide_class_field   (GladeEditor       *editor);

GLADEUI_EXPORTS
gboolean     glade_editor_query_dialog       (GladeWidget       *widget);
GLADEUI_EXPORTS
GtkWidget   *glade_editor_dialog_for_widget  (GladeWidget       *widget);
GLADEUI_EXPORTS
void         glade_editor_reset_dialog_run   (GtkWidget         *parent,
                                              GladeWidget       *gwidget);

G_END_DECLS

#endif /* __GLADE_EDITOR_H__ */
