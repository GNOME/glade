/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PALETTE_H__
#define __GLADE_PALETTE_H__

G_BEGIN_DECLS


#define GLADE_TYPE_PALETTE              (glade_palette_get_type ())
#define GLADE_PALETTE(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PALETTE, GladePalette))
#define GLADE_PALETTE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_PALETTE, GladePaletteClass))
#define GLADE_IS_PALETTE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PALETTE))
#define GLADE_IS_PALETTE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_PALETTE))
#define GLADE_PALETTE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_PALETTE, GladePaletteClass))

typedef struct _GladePaletteClass	GladePaletteClass;


/* The GladePalette is used so that the user can choose a widget to be
 * created. It is Composed by a section where you can select the
 * group of widgets to choose from an a table with icons for each
 * widget available.
 */
struct _GladePalette
{
	GtkVBox vbox; /* The parent is a vbox
		       */

	GtkTooltips *tooltips; /* Tooltips, currently unimplemented */

#if 0
	/* Not beeing used yet */
	GtkWidget *selector; /* The group selector. This is a button that is
			      * clicked to cancel the add widget action.
			      * This sets our cursor back to the "select
			      * widget" mode
			      */
#endif	
	GtkWidget *groups_vbox; /* The vbox that contains the titles of the sections */
	GtkWidget *label; /* The label contains the text of the selected
			   * widget that is going to be added. For examle
			   * when a button is selcted it contains GtkButton
			   * it contains the text "Selector" when we are
			   * in select widget mode
			   */
	GtkWidget *notebook; /* Where we store the different catalogs */

	gint nb_sections; /* The number of sections in this palette */
	GSList *sections_button_group; /* Each section of the palette has
					* a button. This is the button_group_list
					*/
	GSList *widgets_button_group; /* Each widget in a section has a button
				       * in the palette. This is the button
				       * group. This will move away when
				       * multiple sections are implemented.
				       * most likely into GladeCatalog
				       */
				       
	GtkWidget *dummy_button; /* Each button_group needs a button
				  * selected all the time. This dummy button
				  * is never disaplayed nor _contained_add'ed
				  * and it is selected when we don't want
				  * to have a button "pressed in" for the group.
				  * Will also move to GladeCatalog since it
				  * is part of the widgets_button_group
				  */
};

struct _GladePaletteClass
{
	GtkVBoxClass parent_class;

	void (*widget_class_chosen) (GladePalette *palette, GladeWidgetClass *class);
};

GType glade_palette_get_type (void);

GladePalette *glade_palette_new    (GList *catalogs);

void glade_palette_append_catalog  (GladePalette *palette, GladeCatalog *catalog);

void glade_palette_unselect_widget (GladePalette *palette);


G_END_DECLS

#endif /* __GLADE_PALETTE_H__ */
