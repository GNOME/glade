/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_SIGNAL_H__
#define __GLADE_SIGNAL_H__

G_BEGIN_DECLS

#define GLADE_SIGNAL(s) ((GladeSignal *)s)
#define GLADE_IS_SIGNAL(s) (s != NULL)

struct _GladeSignal {
	gchar *name;         /* Signal name eg "clicked" */
	gchar *handler;      /* Handler function eg "gtk_main_quit" */
	gboolean after;      /* Connect after TRUE or FALSE */
};

GladeXmlNode * glade_signal_write (GladeXmlContext *context, GladeSignal *signal);
void glade_signal_free (GladeSignal *signal);

G_END_DECLS

#endif /* __GLADE_SIGNAL_H__ */
