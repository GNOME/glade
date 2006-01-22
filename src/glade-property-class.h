/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PROPERTY_CLASS_H__
#define __GLADE_PROPERTY_CLASS_H__

G_BEGIN_DECLS

/* The GladePropertyClass structure parameters of a GladeProperty.
 * All entries in the GladeEditor are GladeProperties (except signals)
 * All GladeProperties are associated with a GParamSpec.
 */

#define GLADE_PROPERTY_CLASS(gpc) ((GladePropertyClass *) gpc)
#define GLADE_IS_PROPERTY_CLASS(gpc) (gpc != NULL)

typedef struct _GladePropertyClass GladePropertyClass;

struct _GladePropertyClass
{
	GParamSpec *pspec; /* The Parameter Specification for this property.
			    */

	gchar *id;       /* The id of the property. Like "label" or "xpad"
			  * this is a non-translatable string
			  */
	gchar *name;     /* The name of the property. Like "Label" or "X Pad"
			  * this is a translatable string
			  */
	gchar *tooltip; /* The default tooltip for the property editor rows.
			 */
	GValue *def;      /* The default value for this property */

	GValue *orig_def; /* The real default value obtained through introspection.
			   * (used to decide whether we should write to the
			   * glade file or not).
			   */

	GList *parameters; /* list of GladeParameter objects. This list
			    * provides with an extra set of key-value
			    * pairs to specify aspects of this property.
			    *
			    * This is unused by glade and only maintained
			    * to be of possible use in plugin code.
			    */

	GArray *displayable_values; /* If this property's value is an enumeration/flags and 
				     * there is some value name overridden in a catalog
				     * then it will point to a GEnumValue array with the
				     * modified names, otherwise NULL.
				     */

	gboolean query; /* Whether we should explicitly ask the user about this property
			 * when instantiating a widget with this property (through a popup
			 * dialog).
			 */

	gboolean optional; /* Some properties are optional by nature like
			    * default width. It can be set or not set. A
			    * default property has a check box in the
			    * left that enables/disables the input
			    */

	gboolean optional_default; /* For optional values, what the default is */

	gboolean construct_only; /* Whether this property is G_PARAM_CONSTRUCT_ONLY or not */
	
	gboolean common; /* Common properties go in the common tab */
	gboolean packing; /* Packing properties go in the packing tab */

	gboolean translatable; /* The property should be translatable, which
				* means that it needs extra parameters in the
				* UI.
				*/

	gint  visible_lines; /* When this pspec calls for a text editor, how many
			      * lines should be visible in the editor.
			      */

	/* These three are the master switches for the glade-file output,
	 * property editor availability & live object updates in the glade environment.
	 */
	gboolean save;      /* Whether we should save to the glade file or not
			     * (mostly just for custom glade properties)
			     */
	gboolean visible;   /* Whether or not to show this property in the editor
			     */
	gboolean ignore;    /* When true, we will not sync the object when the property
			     * changes.
			     */


	gboolean is_modified; /* If true, this property_class has been "modified" from the
			       * the standard property by a xml file. */

	gboolean resource;  /* Some property types; such as some file specifying
			     * string properties or GDK_TYPE_PIXBUF properties; are
			     * resource files and are treated specialy (a filechooser
			     * popup is used and the resource is copied to the project
			     * directory).
			     */
	
	/* Delagate to verify if this is a valid value for this property,
	 * if this function exists and returns FALSE, then glade_property_set
	 * will abort before making any changes
	 */
	gboolean (*verify_function) (GObject *object,
				     const GValue *value);
	
	/* If this property can't be set with g_object_set then
	 * we need to implement it inside glade. This is a pointer
	 * to the function that can set this property. The functions
	 * to work arround these problems are inside glade-gtk.c
	 */
	void (*set_function) (GObject *object,
			      const GValue *value);

	/* If this property can't be get with g_object_get then
	 * we need to implement it inside glade. This is a pointer
	 * to the function that can get this property. The functions
	 * to work arround these problems are inside glade-gtk.c
	 *
	 * Note that since glade knows what the property values are 
	 * at all times regardless of the objects copy, this is currently
	 * only used to obtain the values of packing properties that are
	 * set by the said object's parent at "container_add" time.
	 */
	void (*get_function) (GObject *object,
			      GValue *value);


};

LIBGLADEUI_API GladePropertyClass *glade_property_class_new                     (void);
LIBGLADEUI_API GladePropertyClass *glade_property_class_new_from_spec           (GParamSpec          *spec);
LIBGLADEUI_API GladePropertyClass *glade_property_class_clone                   (GladePropertyClass  *property_class);
LIBGLADEUI_API void                glade_property_class_free                    (GladePropertyClass  *property_class);
LIBGLADEUI_API gboolean            glade_property_class_is_visible              (GladePropertyClass  *property_class);
LIBGLADEUI_API gboolean            glade_property_class_is_object               (GladePropertyClass  *property_class);
LIBGLADEUI_API GValue             *glade_property_class_make_gvalue_from_string (GladePropertyClass  *property_class,
										 const gchar         *string,
										 GladeProject        *project);
LIBGLADEUI_API gchar              *glade_property_class_make_string_from_gvalue (GladePropertyClass  *property_class,
										 const GValue        *value);
LIBGLADEUI_API GValue             *glade_property_class_make_gvalue_from_vl     (GladePropertyClass  *property_class,
										 va_list              vl);
LIBGLADEUI_API void                glade_property_class_set_vl_from_gvalue      (GladePropertyClass  *class,
										 GValue              *value,
										 va_list              vl);
LIBGLADEUI_API gboolean            glade_property_class_update_from_node        (GladeXmlNode        *node,
										 GModule             *module,
										 GType                widget_type,
										 GladePropertyClass **property_class,
										 const gchar         *domain);
LIBGLADEUI_API gchar              *glade_property_class_make_string_from_flags  (GladePropertyClass *class, 
										 guint               fvals, 
										 gboolean            displayables);
LIBGLADEUI_API gchar              *glade_property_class_get_displayable_value   (GladePropertyClass *class, 
										 gint                value);
LIBGLADEUI_API GtkAdjustment      *glade_property_class_make_adjustment         (GladePropertyClass *class);

G_END_DECLS

#endif /* __GLADE_PROPERTY_CLASS_H__ */
