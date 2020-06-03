#ifndef __GLADE_NAME_CONTEXT_H__
#define __GLADE_NAME_CONTEXT_H__

#include <glib.h>
#include <gladeui/glade-macros.h>

G_BEGIN_DECLS

typedef struct _GladeNameContext GladeNameContext;

GLADEUI_EXPORTS
GladeNameContext  *glade_name_context_new                 (void);
GLADEUI_EXPORTS
void               glade_name_context_destroy             (GladeNameContext *context);

GLADEUI_EXPORTS
gchar             *glade_name_context_new_name            (GladeNameContext *context,
                                                           const gchar      *base_name);

GLADEUI_EXPORTS
guint              glade_name_context_n_names             (GladeNameContext *context);

GLADEUI_EXPORTS
gboolean           glade_name_context_has_name            (GladeNameContext *context,
                                                           const gchar      *name);

GLADEUI_EXPORTS
gboolean           glade_name_context_add_name            (GladeNameContext *context,
                                                           const gchar      *name);

GLADEUI_EXPORTS
void               glade_name_context_release_name        (GladeNameContext *context,
                                                           const gchar      *name);

G_END_DECLS

#endif /* __GLADE_NAME_CONTEXT_H__ */
