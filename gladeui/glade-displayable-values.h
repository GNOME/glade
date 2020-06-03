#ifndef __GLADE_DISAPLAYABLE_VALUES_H__
#define __GLADE_DISAPLAYABLE_VALUES_H__

#include <glib.h>
#include <glib-object.h>
#include <gladeui/glade-macros.h>

G_BEGIN_DECLS

GLADEUI_EXPORTS
void        glade_register_displayable_value      (GType          type, 
                                                   const gchar   *value, 
                                                   const gchar   *domain,
                                                   const gchar   *string);

GLADEUI_EXPORTS
void        glade_register_translated_value       (GType          type, 
                                                   const gchar   *value, 
                                                   const gchar   *string);

GLADEUI_EXPORTS
gboolean    glade_type_has_displayable_values     (GType          type);

GLADEUI_EXPORTS
const 
gchar      *glade_get_displayable_value           (GType          type, 
                                                   const gchar   *value);

GLADEUI_EXPORTS
gboolean    glade_displayable_value_is_disabled   (GType          type, 
                                                   const gchar   *value);

GLADEUI_EXPORTS
void        glade_displayable_value_set_disabled  (GType type,
                                                   const gchar *value,
                                                   gboolean disabled);

GLADEUI_EXPORTS
const 
gchar      *glade_get_value_from_displayable      (GType          type, 
                                                   const gchar   *displayabe);

G_END_DECLS

#endif /* __GLADE_DISAPLAYABLE_VALUES_H__ */
