#ifndef __GLADE_SIGNAL_H__
#define __GLADE_SIGNAL_H__

#include <glib.h>

G_BEGIN_DECLS

#define GLADE_SIGNAL(s) ((GladeSignal *)s)
#define GLADE_IS_SIGNAL(s) (s != NULL)

typedef struct _GladeSignal  GladeSignal;

GladeSignal          *glade_signal_new                 (const gchar        *name,
							const gchar        *handler,
							const gchar        *userdata,
							gboolean            after,
							gboolean            swapped);
GladeSignal          *glade_signal_clone               (const GladeSignal  *signal);
void                  glade_signal_free                (GladeSignal        *signal);
gboolean              glade_signal_equal               (GladeSignal        *sig1, 
							GladeSignal        *sig2);
GladeSignal          *glade_signal_read                (GladeXmlNode       *node);
void                  glade_signal_write               (GladeSignal        *signal,
							GladeXmlContext    *context,
							GladeXmlNode       *node);

void                  glade_signal_set_name            (GladeSignal        *signal,
							const gchar        *name);
G_CONST_RETURN gchar *glade_signal_get_name            (GladeSignal        *signal);
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
