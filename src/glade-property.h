/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PROPERTY_H__
#define __GLADE_PROPERTY_H__

G_BEGIN_DECLS


#define GLADE_PROPERTY(p) ((GladeProperty *)p)
#define GLADE_IS_PROPERTY(p) (p != NULL)


/* A GladeProperty is an instance of a GladePropertyClass.
 * There will be one GladePropertyClass for "GtkLabel->label" but one
 * GladeProperty for each GtkLabel in the GladeProject.
 */
struct _GladeProperty
{
	GladePropertyClass *class; /* A pointer to the GladeProperty that this
				    * setting specifies
				    */
	GladeWidget *widget; /* A pointer to the GladeWidget that this
			      * GladeProperty is modifying
			      */

	GValue *value; /* The value of the property
			*/

	gboolean enabled; /* Enabled is a flag that is used for GladeProperties
			   * that have the optional flag set to let us know
			   * if this widget has this GladeSetting enabled or
			   * not. (Like default size, it can be specified or
			   * unspecified). This flag also sets the state
			   * of the property->input state for the loaded
			   * widget.
			   */

	/* Used only for translatable strings. */
	gboolean  i18n_translatable;
	gboolean  i18n_has_context;
	gchar    *i18n_comment;
		
#if 0
	GladeWidget *child; /* A GladeProperty of type object has a child */
#endif
	gboolean loading;
};


GladeProperty *glade_property_new (GladePropertyClass *class,
				   GladeWidget        *widget,
				   GValue             *value);

void glade_property_free (GladeProperty *property);

void glade_property_set (GladeProperty *property, const GValue *value);

void glade_property_sync (GladeProperty *property);

gboolean glade_property_write (GArray *props, GladeProperty *property,
			       GladeInterface *interface);

GladeProperty *glade_property_dup (GladeProperty *template, GladeWidget *widget);

void glade_property_i18n_set_comment (GladeProperty *property, const gchar *str);

const gchar *glade_property_i18n_get_comment (GladeProperty *property);

void glade_property_i18n_set_translatable (GladeProperty *property, gboolean translatable);

gboolean glade_property_i18n_get_translatable (GladeProperty *property);

void glade_property_i18n_set_has_context (GladeProperty *property, gboolean has_context);

gboolean glade_property_i18n_get_has_context (GladeProperty *property);

G_END_DECLS

#endif /* __GLADE_PROPERTY_H__ */
