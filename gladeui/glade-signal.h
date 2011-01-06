#ifndef __GLADE_SIGNAL_H__
#define __GLADE_SIGNAL_H__

#include <glib.h>

G_BEGIN_DECLS

#define GLADE_TYPE_SIGNAL            (glade_signal_get_type())
#define GLADE_SIGNAL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_SIGNAL, GladeSignal))
#define GLADE_SIGNAL_KLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_SIGNAL, GladeSignalKlass))
#define GLADE_IS_SIGNAL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_SIGNAL))
#define GLADE_IS_SIGNAL_KLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_SIGNAL))
#define GLADE_SIGNAL_GET_KLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_SIGNAL, GladeSignalKlass))

typedef struct _GladeSignal         GladeSignal;
typedef struct _GladeSignalKlass    GladeSignalKlass;
typedef struct _GladeSignalPrivate  GladeSignalPrivate;

struct _GladeSignal {
  GObject object;

  GladeSignalPrivate *priv;
};

struct _GladeSignalKlass {
  GObjectClass object_class;

  void   (* glade_reserved1)   (void);
  void   (* glade_reserved2)   (void);
  void   (* glade_reserved3)   (void);
  void   (* glade_reserved4)   (void);
};


GType                 glade_signal_get_type            (void) G_GNUC_CONST;

GladeSignal          *glade_signal_new                 (const gchar        *name,
							const gchar        *handler,
							const gchar        *userdata,
							gboolean            after,
							gboolean            swapped);
gboolean              glade_signal_equal               (const GladeSignal        *sig1, 
							const GladeSignal        *sig2);
GladeSignal          *glade_signal_read                (GladeXmlNode       *node);
void                  glade_signal_write               (GladeSignal        *signal,
							GladeXmlContext    *context,
							GladeXmlNode       *node);

void                  glade_signal_set_name            (GladeSignal        *signal,
							const gchar        *name);
G_CONST_RETURN gchar *glade_signal_get_name            (const GladeSignal        *signal);
void                  glade_signal_set_handler         (GladeSignal        *signal,
							const gchar        *handler);
G_CONST_RETURN gchar *glade_signal_get_handler         (GladeSignal        *signal);
void                  glade_signal_set_userdata        (GladeSignal        *signal,
							const gchar        *userdata);
G_CONST_RETURN gchar *glade_signal_get_userdata        (GladeSignal        *signal);
void                  glade_signal_set_after           (GladeSignal        *signal,
							gboolean            after);
gboolean              glade_signal_get_after           (GladeSignal        *signal);
void                  glade_signal_set_swapped         (GladeSignal        *signal,
							gboolean            swapped);
gboolean              glade_signal_get_swapped         (GladeSignal        *signal);
void                  glade_signal_set_support_warning (GladeSignal        *signal,
							const gchar        *support_warning);
G_CONST_RETURN gchar *glade_signal_get_support_warning (GladeSignal        *signal);

G_END_DECLS

#endif /* __GLADE_SIGNAL_H__ */
