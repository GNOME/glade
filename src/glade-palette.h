/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PALETTE_H__
#define __GLADE_PALETTE_H__

G_BEGIN_DECLS

#define GLADE_PALETTE(obj)          GTK_CHECK_CAST (obj, glade_palette_get_type (), GladePalette)
#define GLADE_PALETTE_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, glade_palette_get_type (), GladePaletteClass)
#define GLADE_IS_PALETTE(obj)       GTK_CHECK_TYPE (obj, glade_palette_get_type ())

typedef struct _GladePaletteClass	 GladePaletteClass;

/* The GladePalette is used so that the user can choose a widget to be
 * created. It is Composed by a section where you can select the
 * group of widgets to choose from an a table with icons for each
 * widget available.
 */

struct _GladePalette
{
	GtkWindow window; /* Yes a palette is a toplevel window (for now)
			   */

	GladeProjectWindow *project_window; /* A pointer to the GladeProjectWin.
					     * this palette belongs to
					     */

	GtkTooltips *tooltips; /* Tooltips, currently uniumplemented */

#if 0
	/* Not beeing used yet */
	GtkWidget *selector; /* The group selector. This is a button that is
			      * clicked to cancel the add widget action.
			      * This sets our cursor back to the "select
			      * widget" mode
			      */
#endif	
	GtkWidget *vbox; /* The main vbox of the palette */
	GtkWidget *label; /* The label contains the text of the selected
			   * widget that is goingt to be added. For examle
			   * when a button is selcted it contains GtkButton
			   * it contains the text "Selector" when we are
			   * in select widget mode
			   */
	GtkWidget *notebook; /* Where we store the different catalogs */

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
	GtkWindowClass parent_class;

};

void glade_palette_create (GladeProjectWindow *gpw);
void glade_palette_show   (GladeProjectWindow *gpw);
void glade_palette_clear  (GladeProjectWindow *gpw);

G_END_DECLS

#endif /* __GLADE_PALETTE_H__ */
