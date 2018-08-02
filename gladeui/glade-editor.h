#ifndef __GLADE_EDITOR_H__
#define __GLADE_EDITOR_H__

#include <gladeui/glade-signal-editor.h>
#include <gladeui/glade-editable.h>

G_BEGIN_DECLS


#define GLADE_TYPE_EDITOR            (glade_editor_get_type ())
#define GLADE_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EDITOR, GladeEditor))
#define GLADE_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EDITOR, GladeEditorClass))
#define GLADE_IS_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EDITOR))
#define GLADE_IS_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EDITOR))
#define GLADE_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_EDITOR, GladeEditorClass))

typedef struct _GladeEditor          GladeEditor;
typedef struct _GladeEditorClass     GladeEditorClass;
typedef struct _GladeEditorPrivate   GladeEditorPrivate;

/* The GladeEditor is a window that is used to display and modify widget
 * properties. The glade editor contains the details of the selected
 * widget for the selected project
 */
struct _GladeEditor
{
  GtkBox vbox;  /* The editor is a vbox */

  GladeEditorPrivate *priv;
};

struct _GladeEditorClass
{
  GtkBoxClass parent_class;

  void   (* glade_reserved1)   (void);
  void   (* glade_reserved2)   (void);
  void   (* glade_reserved3)   (void);
  void   (* glade_reserved4)   (void);
};


GType        glade_editor_get_type           (void);

GladeEditor *glade_editor_new                (void);
void         glade_editor_load_widget        (GladeEditor       *editor,
                                              GladeWidget       *widget);
G_DEPRECATED
void         glade_editor_show_info          (GladeEditor       *editor);
G_DEPRECATED
void         glade_editor_hide_info          (GladeEditor       *editor);

void         glade_editor_show_class_field   (GladeEditor       *editor);
void         glade_editor_hide_class_field   (GladeEditor       *editor);

gboolean     glade_editor_query_dialog       (GladeWidget       *widget);
GtkWidget   *glade_editor_dialog_for_widget  (GladeWidget       *widget);
void         glade_editor_reset_dialog_run   (GtkWidget         *parent,
                                              GladeWidget       *gwidget);

G_END_DECLS

#endif /* __GLADE_EDITOR_H__ */
