#ifndef __GLADE_CLIPBOARD_H__
#define __GLADE_CLIPBOARD_H__

#include <gladeui/glade.h>

G_BEGIN_DECLS

#define GLADE_TYPE_CLIPBOARD    (glade_clipboard_get_type ())
#define GLADE_CLIPBOARD(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_CLIPBOARD, GladeClipboard))
#define GLADE_IS_CLIPBOARD(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_CLIPBOARD))

typedef struct _GladeClipboard        GladeClipboard;
typedef struct _GladeClipboardClass   GladeClipboardClass;
typedef struct _GladeClipboardPrivate GladeClipboardPrivate;

struct _GladeClipboard
{
  GObject    parent_instance;

  GladeClipboardPrivate *priv;
};

struct _GladeClipboardClass
{
  GObjectClass parent_class;

  void   (* glade_reserved1)   (void);
  void   (* glade_reserved2)   (void);
  void   (* glade_reserved3)   (void);
  void   (* glade_reserved4)   (void);
};


GType           glade_clipboard_get_type         (void);

GladeClipboard *glade_clipboard_new              (void);
void            glade_clipboard_add              (GladeClipboard *clipboard, 
                                                  GList          *widgets);
void            glade_clipboard_clear            (GladeClipboard *clipboard);

gboolean        glade_clipboard_get_has_selection(GladeClipboard *clipboard);
GList          *glade_clipboard_widgets          (GladeClipboard *clipboard);

G_END_DECLS

#endif /* __GLADE_CLIPBOARD_H__ */
