#ifndef __GLADE_CLIPBOARD_H__
#define __GLADE_CLIPBOARD_H__

#include <gladeui/glade.h>

G_BEGIN_DECLS

#define GLADE_TYPE_CLIPBOARD glade_clipboard_get_type ()
G_DECLARE_DERIVABLE_TYPE (GladeClipboard, glade_clipboard, GLADE, CLIPBOARD, GObject)

struct _GladeClipboardClass
{
  GObjectClass parent_class;

  gpointer padding[4];
};

GladeClipboard *glade_clipboard_new              (void);
void            glade_clipboard_add              (GladeClipboard *clipboard, 
                                                  GList          *widgets);
void            glade_clipboard_clear            (GladeClipboard *clipboard);

gboolean        glade_clipboard_get_has_selection(GladeClipboard *clipboard);
GList          *glade_clipboard_widgets          (GladeClipboard *clipboard);

G_END_DECLS

#endif /* __GLADE_CLIPBOARD_H__ */
