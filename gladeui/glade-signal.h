#ifndef __GLADE_SIGNAL_H__
#define __GLADE_SIGNAL_H__

#include <glib.h>
#include <gladeui/glade-signal-def.h>
#include <gladeui/glade-widget-adaptor.h>

G_BEGIN_DECLS

#define GLADE_TYPE_SIGNAL            (glade_signal_get_type())
#define GLADE_SIGNAL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_SIGNAL, GladeSignal))
#define GLADE_SIGNAL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_SIGNAL, GladeSignalClass))
#define GLADE_IS_SIGNAL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_SIGNAL))
#define GLADE_IS_SIGNAL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_SIGNAL))
#define GLADE_SIGNAL_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_SIGNAL, GladeSignalClass))

typedef struct _GladeSignal         GladeSignal;
typedef struct _GladeSignalClass    GladeSignalClass;
typedef struct _GladeSignalPrivate  GladeSignalPrivate;

struct _GladeSignal {
  GObject object;

  GladeSignalPrivate *priv;
};

struct _GladeSignalClass {
  GObjectClass object_class;

  void   (* glade_reserved1)   (void);
  void   (* glade_reserved2)   (void);
  void   (* glade_reserved3)   (void);
  void   (* glade_reserved4)   (void);
};


GType                 glade_signal_get_type            (void) G_GNUC_CONST;

GladeSignal          *glade_signal_new                 (const GladeSignalDef *sig_def,
                                                        const gchar        *handler,
                                                        const gchar        *userdata,
                                                        gboolean            after,
                                                        gboolean            swapped);
GladeSignal          *glade_signal_clone               (const GladeSignal  *signal);
gboolean              glade_signal_equal               (const GladeSignal  *sig1, 
                                                        const GladeSignal  *sig2);
GladeSignal          *glade_signal_read                (GladeXmlNode       *node,
                                                        GladeWidgetAdaptor *adaptor);
void                  glade_signal_write               (GladeSignal        *signal,
                                                        GladeXmlContext    *context,
                                                        GladeXmlNode       *node);

const gchar *glade_signal_get_name            (const GladeSignal  *signal);
const GladeSignalDef *glade_signal_get_def    (const GladeSignal * signal);
void                  glade_signal_set_detail          (GladeSignal        *signal,
                                                        const gchar        *detail);
const gchar *glade_signal_get_detail          (const GladeSignal  *signal);
void                  glade_signal_set_handler         (GladeSignal        *signal,
                                                        const gchar        *handler);
const gchar *glade_signal_get_handler         (const GladeSignal  *signal);
void                  glade_signal_set_userdata        (GladeSignal        *signal,
                                                        const gchar        *userdata);
const gchar *glade_signal_get_userdata        (const GladeSignal  *signal);
void                  glade_signal_set_after           (GladeSignal        *signal,
                                                        gboolean            after);
gboolean              glade_signal_get_after           (const GladeSignal  *signal);
void                  glade_signal_set_swapped         (GladeSignal        *signal,
                                                        gboolean            swapped);
gboolean              glade_signal_get_swapped         (const GladeSignal  *signal);
void                  glade_signal_set_support_warning (GladeSignal        *signal,
                                                        const gchar        *support_warning);
const gchar *glade_signal_get_support_warning (const GladeSignal  *signal);

G_END_DECLS

#endif /* __GLADE_SIGNAL_H__ */
