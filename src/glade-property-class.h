/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PROPERTY_CLASS_H__
#define __GLADE_PROPERTY_CLASS_H__

G_BEGIN_DECLS

/* The GladeProperty object describes a settable parameter of a widget.
 * All entries that the user can change in the first page of the GladeEditor
 * make are a GladeProperty (except for the widget name)
 * GladeProperties can be of any type of GladePropertyType

 Sample xml
 
 Integer Type : (float is very similar)

     <Property>
      <Type>Integer</Type>
	 <Name>Border Width</Name>
	 <Tooltip>The width of the border arround the container</Tooltip>
	 <GtkArg>border</GtkArg>
	 <Parameters>
	   <Parameter Key="Min" Value="0"/>
	   <Parameter Key="Max" Value="10000"/>
	   <Parameter Key="Default" Value="0"/>
	   <Parameter Key="StepIncrement" Value="1"/>
	   <Parameter Key="PageIncrement" Value="10"/>
	   <Parameter Key="ClibmRate" Value="1"/>
	 </Parameters>
    </Property>

 Text Type :

     <Property>
      <Type>Text</Type>
	 <Name>Title</Name>
	 <Tooltip>The title of the window</Tooltip>
	 <GtkArg>title</GtkArg>
	 <Parameters>
	   <Parameter Key="VisibleLines" Value="1"/>
	   <Parameter Key="Default" Value=""/>
	 </Parameters>
    </Property>

 Boolean Type :

    <Property>
      <Type>Boolean</Type>
	 <Name>Grow</Name>
	 <Tooltip>If the window can be enlarged</Tooltip>
	 <GtkArg>fixme</GtkArg>
	 <Parameters>
	   <Parameter Key="Default" Value="True"/>
	 </Parameters>
    </Property>


 Enum Type :

        <Property>
      <Type>Choice</Type>
	 <Name>Position</Name>
	 <Tooltip>The initial position of the window</Tooltip>
	 <GtkArg>fixme</GtkArg>
	 <Parameters>
	   <Parameter Key="Default" Value="GTK_WIN_POS_NONE"/>
	 </Parameters>
	 <Enums>
	   <Enum>String Equiv</Enum>
	 <Choices>
	   <Choice>
	     <Name>None</Name>
		<Symbol>GTK_WIN_POS_NONE</Symbol>
	   </Choice>
	   <Choice>
	     <Name>Center</Name>
		<Symbol>GTK_WIN_POS_CENTER</Symbol>
	   </Choice>
	   <Choice>
	     <Name>Mouse</Name>
		<Symbol>GTK_WIN_POS_MOUSE</Symbol>
	   </Choice>
	 </Choices>
    </Property>

 */

#define GLADE_PROPERTY_CLASS(gpc) ((GladePropertyClass *) gpc)
#define GLADE_IS_PROPERTY_CLASS(gpc) (gpc != NULL)

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
	gchar *tooltip; /* The tooltip. Currently unimplemented. Not sure if
			 * it should go here
			 */

	GValue *def; /* The default value for this property */

	GValue *orig_def; /* If def is overridden by a xml file,
			     it is the original value, otherwise NULL. */

	GList *parameters; /* list of GladeParameter objects. This list
			    * provides with an extra set of key-value
			    * pairs to specify aspects of this property.
			    * Like the number of "VisibleLines" of a text
			    * property. Or the range and default values of
			    * a spin button entry. Also the default choice
			    * for a type == CHOICE
			    */

	gboolean query; /* Whether we should explicitly ask the user about this property
			 * when instantiating a widget with this property (through a popup
			 * dialog).
			 */

	gboolean optional; /* Some properties are optional by nature like
			    * default width. It can be set or not set. A
			    * default property has a check box in the
			    * left that enables/disables de input
			    */

	gboolean optional_default; /* For optional values, what the default is */
	gboolean construct_only; /* Whether this property is G_PARAM_CONSTRUCT_ONLY or not */
	
	gboolean common; /* Common properties go in the common tab */
	gboolean packing; /* Packing properties go in the packing tab */

	gboolean is_modified; /* If true, this property_class has been "modified" from the
			       * the standard property by a xml file. */

	gboolean (*verify_function) (GObject *object,
				     const GValue *value);
                       /* Delagate to verify if this is a valid value for this property,
		       * if this function exists and returns FALSE, then glade_property_set
		       * will abort before making any changes
		       */
	
	void (*set_function) (GObject *object,
			      const GValue *value);
	               /* If this property can't be set with g_object_set then
		       * we need to implement it inside glade. This is a pointer
		       * to the function that can set this property. The functions
		       * to work arround these problems are inside glade-gtk.c
		       */
	void (*get_function) (GObject *object,
			      GValue *value);
		       /* If this property can't be get with g_object_get then
		       * we need to implement it inside glade. This is a pointer
		       * to the function that can get this property. The functions
		       * to work arround these problems are inside glade-gtk.c
		       */
	gboolean (*visible) (GladeWidgetClass *widget_class);
};

GladePropertyClass * glade_property_class_new (void);
GladePropertyClass * glade_property_class_new_from_spec (GParamSpec *spec);
GladePropertyClass *glade_property_class_clone (GladePropertyClass *property_class);

void glade_property_class_free (GladePropertyClass *property_class);

gboolean glade_property_class_is_visible (GladePropertyClass *property_class, GladeWidgetClass *widget_class);

GValue * glade_property_class_make_gvalue_from_string (GladePropertyClass *property_class,
						       const gchar *string);
gchar *  glade_property_class_make_string_from_gvalue (GladePropertyClass *property_class,
						       const GValue *value);

gboolean glade_property_class_update_from_node (GladeXmlNode *node,
						GladeWidgetClass *widget_class,
						GladePropertyClass **property_class);

G_END_DECLS

#endif /* __GLADE_PROPERTY_CLASS_H__ */
