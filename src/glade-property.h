/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PROPERTY_H__
#define __GLADE_PROPERTY_H__

G_BEGIN_DECLS


#define GLADE_TYPE_PROPERTY                (glade_property_get_type ())
#define GLADE_PROPERTY(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PROPERTY, GladeProperty))
#define GLADE_PROPERTY_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_PROPERTY, GladePropertyObjectClass))
#define GLADE_IS_PROPERTY(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PROPERTY))

typedef struct _GladePropertyObjectClass GladePropertyObjectClass;

/* A GladeProperty is an instance of a GladePropertyClass.
 * There will be one GladePropertyClass for "GtkLabel->label" but one
 * GladeProperty for each GtkLabel in the GladeProject.
 */
struct _GladeProperty
{
	GObject object; 

	GladePropertyClass *class; /* A pointer to the GladeProperty that this
				    * setting specifies
				    */
	GladeWidget *widget; /* A pointer to the GladeWidget that this
			      * GladeProperty is modifying
			      */

	GValue *value; /* The value of the property
			*/

	gboolean enabled; /* Enables is a flag that is used for GladeProperties
			   * that have the optional flag set to let us know
			   * if this widget has this GladeSetting enabled or
			   * not. (Like default size, it can be specified or
			   * unspecified). This flag also sets the state
			   * of the property->input state for the loaded
			   * widget.
			   */
#if 0
	GladeWidget *child; /* A GladeProperty of type object has a child */
#endif
	gboolean loading;
};

struct _GladePropertyObjectClass
{
	GtkObjectClass parent_class;

	void   (*changed)          (GladeProperty *property, const gchar *value);
};

GType glade_property_get_type (void);

GladeProperty *glade_property_new (GladePropertyClass *class, GladeWidget *widget);

void glade_property_free (GladeProperty *property);

void glade_property_set (GladeProperty *property, const GValue *value);

/* XML i/o */
GladeXmlNode *glade_property_write (GladeXmlContext *context, GladeProperty *property);


G_END_DECLS

#endif /* __GLADE_PROPERTY_H__ */
