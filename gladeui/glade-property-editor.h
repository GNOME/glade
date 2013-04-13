#ifndef __GLADE_PROPERTY_EDITOR_H__
#define __GLADE_PROPERTY_EDITOR_H__

#include <glib.h>
#include <glib-object.h>
#include <gladeui/glade-xml-utils.h>

G_BEGIN_DECLS

#define GLADE_TYPE_PROPERTY_EDITOR            (glade_property_editor_get_type ())
#define GLADE_PROPERTY_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PROPERTY_EDITOR, GladePropertyEditor))
#define GLADE_PROPERTY_EDITOR_CLASS(obj)      (G_TYPE_CHECK_CLASS_CAST ((obj), GLADE_TYPE_PROPERTY_EDITOR, GladePropertyEditorInterface))
#define GLADE_IS_PROPERTY_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PROPERTY_EDITOR))
#define GLADE_PROPERTY_EDITOR_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GLADE_TYPE_PROPERTY_EDITOR, GladePropertyEditorInterface))

typedef struct _GladePropertyEditor      GladePropertyEditor;
typedef struct _GladePropertyEditorIface GladePropertyEditorInterface;

struct _GladePropertyEditorIface
{
  GTypeInterface g_iface;

  /* virtual table */
  void          (* load) (GladePropertyEditor  *editor,
			  GladeWidget          *widget);
};

GType        glade_property_editor_get_type   (void) G_GNUC_CONST;
void         glade_property_editor_load       (GladePropertyEditor *editor,
					       GladeWidget         *widget);

G_END_DECLS

#endif /* __GLADE_PROPERTY_EDITOR_H__ */
