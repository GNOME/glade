#ifndef __GLADE_EPROP_ENUM_INT_H__
#define __GLADE_EPROP_ENUM_INT_H__

#include <glib-object.h>
#include <gladeui/glade.h>

G_BEGIN_DECLS

#define GLADE_TYPE_EPROP_ENUM_INT            (glade_eprop_enum_int_get_type())
#define GLADE_EPROP_ENUM_INT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EPROP_ENUM_INT, GladeEPropEnumInt))
#define GLADE_EPROP_ENUM_INT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EPROP_ENUM_INT, GladeEPropEnumIntClass))
#define GLADE_IS_EPROP_ENUM_INT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EPROP_ENUM_INT))
#define GLADE_IS_EPROP_ENUM_INT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EPROP_ENUM_INT))
#define GLADE_EPROP_ENUM_INT_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_EPROP_ENUM_INT, GladeEPropEnumIntClass))

typedef struct
{
  GladeEditorProperty eprop;
} GladeEPropEnumInt;

typedef struct {
	GladeEditorPropertyClass eprop_class;
} GladeEPropEnumIntClass;

GladeEditorProperty *glade_eprop_enum_int_new (GladePropertyClass *pclass,
					       GType               type,
					       gboolean            use_command);

G_END_DECLS

#endif   /* __GLADE_EPROP_ENUM_INT_H__ */
