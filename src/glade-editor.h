/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_EDITOR_H__
#define __GLADE_EDITOR_H__

G_BEGIN_DECLS


#define GLADE_TYPE_EDITOR            (glade_editor_get_type ())
#define GLADE_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EDITOR, GladeEditor))
#define GLADE_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EDITOR, GladeEditorClass))
#define GLADE_IS_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EDITOR))
#define GLADE_IS_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EDITOR))
#define GLADE_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_EDITOR, GladeEditorClass))

#define GLADE_EDITOR_TABLE(t)       ((GladeEditorTable *)t)
#define GLADE_IS_EDITOR_TABLE(t)    (t != NULL)
#define GLADE_EDITOR_PROPERTY(p)    ((GladeEditorProperty *)p)
#define GLADE_IS_EDITOR_PROPERTY(p) (p != NULL)

typedef struct _GladeEditorClass     GladeEditorClass;
typedef struct _GladeEditorTable     GladeEditorTable;
typedef struct _GladeEditorProperty  GladeEditorProperty;
typedef enum   _GladeEditorTableType GladeEditorTableType;

enum _GladeEditorTableType
{
	TABLE_TYPE_GENERAL,
	TABLE_TYPE_COMMON,
	TABLE_TYPE_PACKING,
	TABLE_TYPE_QUERY
};

/* The GladeEditor is a window that is used to display and modify widget
 * properties. The glade editor contains the details of the selected
 * widget for the selected project
 */
struct _GladeEditor
{
	GtkNotebook notebook;  /* The editor is a notebook */
	
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
	GtkWidget *vbox_widget;  /* The editor has (at this moment) four tabs
				  * this are pointers to the vboxes inside
				  * each tab. The vboxes are wrapped into a
				  * scrolled window.
				  * The vbox_widget is deparented and parented
				  * with GladeWidgetClass->table_widget
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
	GtkNotebookClass parent_class;

	void   (*add_signal) (GladeEditor *editor, const char *id_widget,
			      GType type_widget, guint id_signal,
			      const char *callback_name);
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
	GladeEditor *editor; /* Handy pointer that avoids havving to pass the
			      * editor arround.
			      */
	
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

	GladeEditorTableType type; /* Is this table to be used in the common tab, ?
				    * the general tab, a packing tab or the query popup ?
				    */
	
	gint rows;
};

/* For every GladePropertyClass we have a GladeEditorProperty that is
 * basically an input (GtkWidget) for that GladePropertyClass.
 */
struct _GladeEditorProperty
{
	GladePropertyClass *class; /* The class this property
				    * corresponds to.
				    */

	GladeProperty *property; /* The loaded GladeProperty
				  */

	GtkWidget *input; /* The widget that modifies this property, the widget
			   * can be a GtkSpinButton, a GtkEntry, GtkMenu, etc.
			   * depending on the property type.
			   * [see glade-property.h and glade-property-class.h]
			   */
#if 0
	gboolean loading; /* We set this flag when we are loading a new GladeProperty
			   * into this GladeEditorProperty. This flag is used so that
			   * when we receive a "changed" signal we know that nothing has
			   * really changed, we just loaded a new glade widget
			   */
#endif	

	GList *children; /* Used for class->type = OBJECT. Where a sigle entry corresponds
			  * to a number of inputs
			  */

	gboolean from_query_dialog; /* If this input is part of a query dialog
				     * this is TRUE.
				     */
};

GType glade_editor_get_type (void);

GladeEditor *glade_editor_new (void);

void         glade_editor_load_widget        (GladeEditor *editor,
					      GladeWidget *widget);
void         glade_editor_add_signal         (GladeEditor *editor,
					      guint        id_signal,
					      const char  *callback_name);
void         glade_editor_update_widget_name (GladeEditor *editor);
gboolean     glade_editor_query_dialog       (GladeEditor *editor,
					      GladeWidget *widget);
gboolean     glade_editor_editable_property  (GParamSpec  *pspec);

G_END_DECLS

#endif /* __GLADE_EDITOR_H__ */
