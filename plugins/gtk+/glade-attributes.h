/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_ATTRIBUTES_H__
#define __GLADE_ATTRIBUTES_H__

#include <glib-object.h>
#include <gladeui/glade.h>

G_BEGIN_DECLS


#define GLADE_TYPE_EPROP_ATTRS            (glade_eprop_attrs_get_type())
#define	GLADE_TYPE_PARAM_ATTRIBUTES       (glade_param_attributes_get_type())
#define	GLADE_TYPE_ATTR_GLIST             (glade_attr_glist_get_type())

#define GLADE_IS_PARAM_SPEC_ATTRIBUTES(pspec)     \
        (G_TYPE_CHECK_INSTANCE_TYPE ((pspec),  \
         GLADE_TYPE_PARAM_ATTRIBUTES))
#define GLADE_PARAM_SPEC_ATTRIBUTES(pspec)        \
        (G_TYPE_CHECK_INSTANCE_CAST ((pspec),  \
         GLADE_TYPE_PARAM_ATTRIBUTES, GladeParamSpecAttributes))

/* The GladeParamSpecAttributes is a GList * of GladeAttribute structs */
typedef struct _GladeParamSpecAttributes   GladeParamSpecAttributes;
typedef struct _GladeAttribute             GladeAttribute;

struct _GladeAttribute {
	PangoAttrType   type;   /* The type of pango attribute */

	GValue          value;  /* The coresponding value */

	guint           start;  /* The text offsets where the attributes should apply to */
	guint           end;
};

GType        glade_param_attributes_get_type    (void) G_GNUC_CONST;
GType        glade_eprop_attrs_get_type         (void) G_GNUC_CONST;
GType        glade_attr_glist_get_type          (void) G_GNUC_CONST;


GParamSpec  *glade_param_spec_attributes        (const gchar     *name,
						 const gchar     *nick,
						 const gchar     *blurb,
						 GParamFlags      flags);

GParamSpec  *glade_gtk_attributes_spec          (void);

GladeAttribute *glade_gtk_attribute_from_string (PangoAttrType    type,
						 const gchar     *strval);
gchar       *glade_gtk_string_from_attr         (GladeAttribute  *gattr);
void         glade_attr_list_free               (GList           *attrs);


G_END_DECLS

#endif   /* __GLADE_ATTRIBUTES_H__ */
