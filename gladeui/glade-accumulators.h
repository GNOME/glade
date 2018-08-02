#ifndef __GLADE_ACCUMULATORS_H__
#define __GLADE_ACCUMULATORS_H__

#include <glib-object.h>

G_BEGIN_DECLS

gboolean _glade_single_object_accumulator   (GSignalInvocationHint *ihint,
                                             GValue                *return_accu,
                                             const GValue          *handler_return,
                                             gpointer               dummy);

gboolean _glade_integer_handled_accumulator (GSignalInvocationHint *ihint,
                                             GValue                *return_accu,
                                             const GValue          *handler_return,
                                             gpointer               dummy);
                                            
gboolean _glade_boolean_handled_accumulator (GSignalInvocationHint *ihint,
                                             GValue                *return_accu,
                                             const GValue          *handler_return,
                                             gpointer               dummy);

gboolean _glade_string_accumulator          (GSignalInvocationHint *ihint,
                                             GValue                *return_accu,
                                             const GValue          *handler_return,
                                             gpointer               dummy);

gboolean _glade_strv_handled_accumulator    (GSignalInvocationHint *ihint,
                                             GValue                *return_accu,
                                             const GValue          *handler_return,
                                             gpointer               dummy);

gboolean _glade_stop_emission_accumulator   (GSignalInvocationHint *ihint,
                                             GValue                *return_accu,
                                             const GValue          *handler_return,
                                             gpointer               dummy);
G_END_DECLS

#endif   /* __GLADE_ACCUMULATORS_H__ */
