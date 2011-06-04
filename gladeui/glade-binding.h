#ifndef __GLADE_BINDING_H__
#define __GLADE_BINDING_H__

#include <glib-object.h>
#include "glade-xml-utils.h"

G_BEGIN_DECLS

#define GLADE_TYPE_BINDING            (glade_binding_get_type())
#define GLADE_BINDING(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_BINDING, GladeBinding))
#define GLADE_BINDING_KLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_BINDING, GladeBindingClass))
#define GLADE_IS_BINDING(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_BINDING))
#define GLADE_IS_BINDING_KLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_BINDING))
#define GLADE_BINDING_GET_KLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_BINDING, GladePropertyKlass))

typedef struct _GladeBinding        GladeBinding;
typedef struct _GladeBindingClass   GladeBindingClass;
typedef struct _GladeBindingPrivate GladeBindingPrivate;

/* Forward-declare GladeProperty so that
 * we don't have to include "glade-property.h",
 * which depends on this header file
 */
typedef struct _GladeProperty       GladeProperty;

struct _GladeBinding
{
  GObject              parent_instance;

  GladeBindingPrivate *priv;
};


struct _GladeBindingClass
{
  GObjectClass  parent_class;
};


GType                  glade_binding_get_type   (void) G_GNUC_CONST;

GladeBinding          *glade_binding_new        (GladeProperty *source,
                                                 GladeProperty *widget);

GladeProperty         *glade_binding_get_target (GladeBinding *binding);

GladeProperty         *glade_binding_get_source (GladeBinding *binding);

GladeBinding          *glade_binding_read       (GladeXmlNode *node,
                                                 GladeWidget *widget);

void                   glade_binding_complete   (GladeBinding *binding,
                                                 GladeProject *project);

void                   glade_binding_write       (GladeBinding *binding,
                                                  GladeXmlContext *context,
                                                  GladeXmlNode *node);

void                   glade_binding_free       (GladeBinding *binding);

G_END_DECLS

#endif /* __GLADE_BINDING_H__ */
