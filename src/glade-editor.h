/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_EDITOR_H__
#define __GLADE_EDITOR_H__

G_BEGIN_DECLS

#define GLADE_EDITOR(obj)          GTK_CHECK_CAST (obj, glade_editor_get_type (), GladeEditor)
#define GLADE_EDITOR_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, glade_editor_get_type (), GladeEditorClass)
#define GLADE_IS_EDITOR(obj)       GTK_CHECK_TYPE (obj, glade_editor_get_type ())

typedef struct _GladeEditorClass    GladeEditorClass;
typedef struct _GladeEditorTable    GladeEditorTable;
typedef struct _GladeEditorProperty GladeEditorProperty;

guint glade_editor_get_type (void);

/* The GladeEditor is a window that is used to display and modify widget
 * properties. The glade editor contains the details of the selected
 * widget for the selected project
 */
struct _GladeEditor
{
	GtkWindow window;  /* The editor (for now) a toplevel window */
	
	GladeProjectWindow *project_window; /* This editor belongs to this
					     * project_window
					     */
					       
	GladeWidget *loaded_widget;        /* A handy pointer to the GladeWidget
					    * that is loaded in the editor. NULL
					    * if no widgets are selected
					    */
	GladeWidgetClass *loaded_class; /* A pointer to the loaded
					 * GladeWidgetClass. Note that we can
					 * have a class loaded without a
					 * loaded_widget. For this reason we
					 * can't use loaded_widget->class.
					 * When a widget is selected we load
					 * this class in the editor first and
					 * then fill the values of the inputs
					 * with the GladeProperty items.
					 * This is usefull for not having
					 * to redraw/container_add the widgets
					 * when we switch from widgets of the
					 * same class
					 */
	GtkTooltips *tooltips;   /* The tooltips for the editor. This are not
				  * beeing used ATM. I wonder if they should
				  * go into GladeWidgetClass since they are
				  * different for each class or if this
				  * will point to the tooltips for that
				  * class. Oh well, we'll see
				  */
	GtkWidget *vbox_widget;  /* The editor has (at this moment) for tabs
				  * this are pointers to the vboxes inside
				  * each tab. The vbox_widget is deparented
				  * and parented with
				  * GladeWidgetClass->table_widget
				  * when a widget is selecteed.
				  */
	GtkWidget *vbox_packing;  /* We might not need this pointer. Not yet
				   * implemeted
				   */
	GtkWidget *vbox_common;   /* We might not need this pointer. Not yet
				   * implemented
				   */
	GtkWidget *vbox_signals;  /* Widget from the GladeSignalEditor is placed
					 * here
					 */
	GladeSignalEditor *signal_editor;

	GList * widget_tables; /* A list of GladeEditorTable. We have a table
				* (gtktable) for each GladeWidgetClass, if
				* we don't have one yet, we create it when
				* we are asked to load a widget of a particular
				* GladeWidgetClass
				*/

	gboolean loading; /* Use when loading a GladeWidget into the editor
			   * we set this flag so that we can ignore the
			   * "changed" signal of the name entry text since
			   * the name has not really changed, just a new name
			   * was loaded.
			   */
};

struct _GladeEditorClass
{
	GtkWindowClass parent_class;

	void   (*select_item) (GladeEditor *editor, GladeWidget *widget);
};

/* For each glade widget class that we have modified, we create a
 * GladeEditorTable. A GladeEditorTable is mainly a gtk_table with all the
 * widgets to edit a particular GladeWidgetClass (well the first tab of the
 * gtk notebook). When a widget of is selected
 * and going to be edited, we create a GladeEditorTable, when another widget
 * of the same class is loaded so that it can be edited, we just update the
 * contents of the editor table to relfect the values of that GladeWidget
 */
struct _GladeEditorTable
{
	GladeWidgetClass *glade_widget_class; /* The GladeWidgetClass this
					       * table belongs to.
					       */

	GtkWidget *table_widget; /* This widget is a gtk_vbox that is displayed
				  * in the glade-editor when a widget of this
				  * class is selected. It is hiden when another
				  * type is selected. When we select a widget
				  * we load into the inputs inside this table
				  * the information about the selected widget.
				  */
	
	GtkWidget *name_entry; /* A pointer to the gtk_entry that holds
				* the name of the widget. This is the
				* first item _pack'ed to the table_widget.
				* We have a pointer here because it is an
				* entry which will not be created from a
				* GladeProperty but rather from code.
				*/

	GList *properties; /* A list of GladeEditorPropery items.
			    * For each row in the gtk_table, there is a
			    * corrsponding GladeEditorProperty struct.
			    */
};

/* For every GladePropertyClass we have a GladeEditorProperty that is
 * basically an input (GtkWidget) for that GladePropertyClass.
 */
struct _GladeEditorProperty
{
	GladePropertyClass *glade_property_class; /* The class this property
						   * corresponds to.
						   */

	GladeProperty *property; /* The loaded GladeProperty
				  */

	GtkWidget *input; /* The widget that modifies this property, the widget
			   * can be a GtkSpinButton, a GtkEntry, GtkMenu, etc.
			   * depending on the property type.
			   * [see glade-property.h and glade-property-class.h]
			   */

	gboolean loading; /* We set this flag when we are loading a new GladeProperty
			   * into this GladeEditorProperty. This flag is used so that
			   * when we receive a "changed" signal we know that nothing has
			   * really changed, we just loaded a new glade widget
			   */
};

typedef enum {
	GLADE_EDITOR_INTEGER,
	GLADE_EDITOR_FLOAT,
	GLADE_EDITOR_DOUBLE,
}GladeEditorNumericType;

void  glade_editor_create (GladeProjectWindow *gpw);
void  glade_editor_show   (GladeProjectWindow *gpw);
void  glade_editor_select_widget (GladeEditor *editor, GladeWidget *widget);

G_END_DECLS

#endif /* __GLADE_EDITOR_H__ */
