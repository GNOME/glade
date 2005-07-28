/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PROPERTY_H__
#define __GLADE_PROPERTY_H__

G_BEGIN_DECLS

#define GLADE_TYPE_PROPERTY            (glade_property_get_type())
#define GLADE_PROPERTY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PROPERTY, GladeProperty))
#define GLADE_PROPERTY_CINFO(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_PROPERTY, GladePropertyCinfo))
#define GLADE_IS_PROPERTY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PROPERTY))
#define GLADE_IS_PROPERTY_CINFO(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_PROPERTY))
#define GLADE_PROPERTY_GET_CINFO(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_PROPERTY, GladePropertyCinfo))


/* A GladeProperty is an instance of a GladePropertyClass.
 * There will be one GladePropertyClass for "GtkLabel->label" but one
 * GladeProperty for each GtkLabel in the GladeProject.
 */
struct _GladeProperty
{
	GObject             parent_instance;

	GladePropertyClass *class;     /* A pointer to the GladeProperty that this
					* setting specifies
					*/
	GladeWidget        *widget;    /* A pointer to the GladeWidget that this
					* GladeProperty is modifying
					*/
	
	GValue             *value;     /* The value of the property
					*/

	gboolean            sensitive; /* Whether this property is sensitive (if the
					* property is "optional" this takes precedence).
					*/

	gboolean            enabled;   /* Enabled is a flag that is used for GladeProperties
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
	/* A GladeProperty of type object has a child */
	GladeWidget *child;
#endif
	gboolean loading;
};


struct _GladePropertyCinfo
{
	GObjectClass  parent_class;

	/* Class methods */
	GladeProperty *         (* dup)                   (GladeProperty *, GladeWidget *);
	void                    (* set)                   (GladeProperty *, const GValue *);
	void                    (* sync)                  (GladeProperty *);
	gboolean                (* write)                 (GladeProperty *, GladeInterface *, GArray *);
	void                    (* set_sensitive)         (GladeProperty *, gboolean);
	void                    (* i18n_set_comment)      (GladeProperty *, const gchar *);
	G_CONST_RETURN gchar *  (* i18n_get_comment)      (GladeProperty *);
	void                    (* i18n_set_translatable) (GladeProperty *, gboolean);
	gboolean                (* i18n_get_translatable) (GladeProperty *);
	void                    (* i18n_set_has_context)  (GladeProperty *, gboolean);
	gboolean                (* i18n_get_has_context)  (GladeProperty *);

	/* Signals */
	void             (* value_changed)         (GladeProperty *, GValue *);
};

LIBGLADEUI_API GType               glade_property_get_type              (void);
LIBGLADEUI_API GladeProperty      *glade_property_new                   (GladePropertyClass *class,
									 GladeWidget        *widget,
									 GValue             *value);
LIBGLADEUI_API GladeProperty      *glade_property_dup                   (GladeProperty      *template,
									 GladeWidget        *widget);
LIBGLADEUI_API void                glade_property_set                   (GladeProperty      *property, 
									 const GValue       *value);
LIBGLADEUI_API void                glade_property_sync                  (GladeProperty      *property);
LIBGLADEUI_API gboolean            glade_property_write                 (GladeProperty      *property, 
									 GladeInterface     *interface, 
									 GArray             *props);
LIBGLADEUI_API void                glade_property_set_sensitive         (GladeProperty      *property,
									 gboolean            sensitive);

LIBGLADEUI_API void                   glade_property_i18n_set_comment      (GladeProperty      *property, 
									    const gchar        *str);
LIBGLADEUI_API G_CONST_RETURN gchar  *glade_property_i18n_get_comment      (GladeProperty      *property);
LIBGLADEUI_API void                   glade_property_i18n_set_translatable (GladeProperty      *property,
									    gboolean            translatable);
LIBGLADEUI_API gboolean               glade_property_i18n_get_translatable (GladeProperty      *property);
LIBGLADEUI_API void                   glade_property_i18n_set_has_context  (GladeProperty      *property,
									    gboolean            has_context);
LIBGLADEUI_API gboolean               glade_property_i18n_get_has_context  (GladeProperty      *property);

G_END_DECLS

#endif /* __GLADE_PROPERTY_H__ */
