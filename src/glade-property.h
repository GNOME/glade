/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PROPERTY_H__
#define __GLADE_PROPERTY_H__

G_BEGIN_DECLS

#define GLADE_PROPERTY(p)    ((GladeProperty *)p)
#define GLADE_IS_PROPERTY(p) (p != NULL)


/* A GladeProperty is an instance of a GladePropertyClass.
 * There will be one GladePropertyClass for "GtkLabel->label" but one
 * GladeProperty for each GtkLabel in the GladeProject.
 */
struct _GladeProperty {

	GladePropertyClass *class; /* A pointer to the GladeProperty that this
				    * setting specifies
				    */
	GladeWidget *widget; /* A pointer to the GladeWidget that this
			      * GladeProperty is modifying
			      */
	
	gchar *value; /* A string representation of the value. Depending
		       * on the property->type it can contain an integer
		       * a "False" or "True" string or a string itself
		       */

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
		       
};

struct _GladePropertyQuery {
	gchar *window_title;
	gchar *question;
};

struct _GladePropertyQueryResult {
	GHashTable *hash;
};
	

GList * glade_property_list_new_from_widget_class (GladeWidgetClass *class,
						   GladeWidget *widget);
GladeProperty * glade_property_new_from_class (GladePropertyClass *class, GladeWidget *widget);
void glade_property_free (GladeProperty *property);

void glade_property_changed_text    (GladeProperty *property, const gchar *text);
void glade_property_changed_integer (GladeProperty *property, gint val);
void glade_property_changed_float   (GladeProperty *property, gfloat val);
void glade_property_changed_double  (GladeProperty *property, gdouble val);
void glade_property_changed_boolean (GladeProperty *property, gboolean val);
void glade_property_changed_choice  (GladeProperty *property, GladeChoice *choice);

const gchar * glade_property_get_text    (GladeProperty *property);
gint          glade_property_get_integer (GladeProperty *property);
gfloat        glade_property_get_float   (GladeProperty *property);
gdouble       glade_property_get_double  (GladeProperty *property);
gboolean      glade_property_get_boolean (GladeProperty *property);
GladeChoice * glade_property_get_choice  (GladeProperty *property);


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
