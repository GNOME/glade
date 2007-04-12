/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_ACCUMULATORS_H__
#define __GLADE_ACCUMULATORS_H__

#include <glib-object.h>

G_BEGIN_DECLS

gboolean glade_single_object_accumulator   (GSignalInvocationHint *ihint,
					    GValue                *return_accu,
					    const GValue          *handler_return,
					    gpointer               dummy);
					    
gboolean glade_boolean_handled_accumulator (GSignalInvocationHint *ihint,
					    GValue                *return_accu,
					    const GValue          *handler_return,
					    gpointer               dummy);

gboolean glade_string_accumulator          (GSignalInvocationHint *ihint,
					    GValue                *return_accu,
					    const GValue          *handler_return,
					    gpointer               dummy);

gboolean glade_stop_emission_accumulator   (GSignalInvocationHint *ihint,
					    GValue                *return_accu,
					    const GValue          *handler_return,
					    gpointer               dummy);
G_END_DECLS

#endif   /* __GLADE_ACCUMULATORS_H__ */
