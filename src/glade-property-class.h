/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PROPERTY_CLASS_H__
#define __GLADE_PROPERTY_CLASS_H__

#include "glade-xml-utils.h"

G_BEGIN_DECLS

typedef enum {
	GLADE_PROPERTY_TYPE_BOOLEAN,
	GLADE_PROPERTY_TYPE_FLOAT,
	GLADE_PROPERTY_TYPE_INTEGER,
	GLADE_PROPERTY_TYPE_DOUBLE,
	GLADE_PROPERTY_TYPE_TEXT,
	GLADE_PROPERTY_TYPE_CHOICE,
	GLADE_PROPERTY_TYPE_OTHER_WIDGETS,
	GLADE_PROPERTY_TYPE_OBJECT,
	GLADE_PROPERTY_TYPE_ERROR
} GladePropertyType;

/* The GladeProperty object describes a settable paramter of a widget.
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


 Choice Type :

        <Property>
      <Type>Choice</Type>
	 <Name>Position</Name>
	 <Tooltip>The initial position of the window</Tooltip>
	 <GtkArg>fixme</GtkArg>
	 <Parameters>
	   <Parameter Key="Default" Value="GTK_WIN_POS_NONE"/>
	 </Parameters>
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

struct _GladePropertyClass {

	GladePropertyType type; /* The type of property from GladePropertyType
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

	GList *parameters; /* list of GladeParameter objects. This list
			    * provides with an extra set of key-value
			    * pairs to specify aspects of this property.
			    * Like the number of "VisibleLines" of a text
			    * property. Or the range and default values of
			    * a spin button entry. Also the default choice
			    * for a type == CHOICE
			    */
	GList *choices;    /* list of GladeChoice items. This is only used
			    * for propeties of type GLADE_PROPERYT_TYPE_CHOICE
			    * and is NULL for other poperties.
			    * [See glade-choice.h]
			    */

	gboolean optional; /* Some properties are optional by nature like
			    * default width. It can be set or not set. A
			    * default property has a check box in the
			    * left that enables/disables de input
			    */

	GladePropertyQuery *query;

	GladeWidgetClass *child; /* A  GladeWidgetClass pointer of objects
				  * that we need to set for this widget
				  * for example : GtkSpinButton has a Adjustment inside
				  * a GtkCombo has an entry inside and a GtkClist which
				  * makes a drop dowm menu. This is only valid with
				  * the type is object.
				  */

	void (*set_function) (GObject *object,
			      const gchar *value);
		       /* If this property can't be set with g_object_set then
		       * we need to implement it inside glade. This is a pointer
		       * to the function that can set this property. The functions
		       * to work arround this problems are inside glade-gtk.c
		       */
};

GtkWidget * glade_property_class_create_label (GladePropertyClass *pclass);
GtkWidget * glade_property_class_create_input (GladePropertyClass *pclass);
GList * glade_property_class_list_new_from_node (xmlNodePtr node, GladeWidgetClass *class);

GParamSpec * glade_property_class_find_spec (GladeWidgetClass *class, const gchar *name);

gchar * glade_property_type_enum_to_string (GladePropertyType type);

G_END_DECLS

#endif /* __GLADE_PROPERTY_CLASS_H__ */
