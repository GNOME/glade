/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PROPERTY_H__
#define __GLADE_PROPERTY_H__

G_BEGIN_DECLS

#define GLADE_PROPERTY(obj)                 GTK_CHECK_CAST (obj, glade_property_get_type (), GladeProperty)
#define GLADE_PROPERTY_OBJECT_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, glade_property_get_type (), GladePropertyObjectClass)
#define GLADE_IS_PROPERTY(obj)              GTK_CHECK_TYPE (obj, glade_property_get_type ())

typedef struct _GladePropertyObjectClass GladePropertyObjectClass;

/* A GladeProperty is an instance of a GladePropertyClass.
 * There will be one GladePropertyClass for "GtkLabel->label" but one
 * GladeProperty for each GtkLabel in the GladeProject.
 */
struct _GladeProperty {
	
	GObject object; 

	GladePropertyClass *class; /* A pointer to the GladeProperty that this
				    * setting specifies
				    */
	GladeWidget *widget; /* A pointer to the GladeWidget that this
			      * GladeProperty is modifying
			      */

	GValue *value;
#if 0	
	gchar *value; /* A string representation of the value. Depending
		       * on the property->type it can contain an integer
		       * a "False" or "True" string or a string itself
		       */
#endif	

	gboolean enabled; /* Enables is a flag that is used for GladeProperties
			   * that have the optional flag set to let us know
			   * if this widget has this GladeSetting enabled or
			   * not. (Like default size, it can be specified or
			   * unspecified). This flag also sets the state
			   * of the property->input state for the loaded
			   * widget.
			   */

	GList *views; /* A list of GladePropertyView items */

	GladeWidget *child; /* A GladeProperty of type object has a child */

	gboolean loading;
};

struct _GladePropertyObjectClass
{
	GtkObjectClass parent_class;

	void   (*changed)          (GladeProperty *property, const gchar *value);
};

struct _GladePropertyQuery {
	gchar *window_title;
	gchar *question;
};

struct _GladePropertyQueryResult {
	GHashTable *hash;
};
	

guint glade_property_get_type (void);

GList * glade_property_list_new_from_widget_class (GladeWidgetClass *class,
						   GladeWidget *widget);
GladeProperty * glade_property_new_from_class (GladePropertyClass *class, GladeWidget *widget);
void glade_property_free (GladeProperty *property);

void glade_property_set         (GladeProperty *property, const GValue *value);
void glade_property_set_string  (GladeProperty *property, const gchar *text);
void glade_property_set_integer (GladeProperty *property, gint val);
void glade_property_set_float   (GladeProperty *property, gfloat val);
void glade_property_set_double  (GladeProperty *property, gdouble val);
void glade_property_set_boolean (GladeProperty *property, gboolean val);
void glade_property_set_enum    (GladeProperty *property, GladeChoice *choice);

const gchar * glade_property_get_string  (GladeProperty *property);
gint          glade_property_get_integer (GladeProperty *property);
gfloat        glade_property_get_float   (GladeProperty *property);
gdouble       glade_property_get_double  (GladeProperty *property);
gboolean      glade_property_get_boolean (GladeProperty *property);
GladeChoice * glade_property_get_enum    (GladeProperty *property);


void glade_property_get_from_widget (GladeProperty *property);

/* Get a GladeProperty */
GladeProperty * glade_property_get_from_id   (GList *settings_list,
					      const gchar *id);

/* Property Queries */
GladePropertyQueryResult * glade_property_query_result_new (void);
void                       glade_property_query_result_destroy (GladePropertyQueryResult *result);

void glade_property_query_result_get_int (GladePropertyQueryResult *result,
					  const gchar *name,
					  gint *return_value);
void glade_property_query_result_set_int (GladePropertyQueryResult *result,
					  const gchar *key,
					  gint value);

/* XML i/o */
GladeXmlNode * glade_property_write (GladeXmlContext *context, GladeProperty *property);


G_END_DECLS

#endif /* __GLADE_PROPERTY_H__ */
