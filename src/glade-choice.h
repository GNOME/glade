#ifndef __GLADE_CHOICE_H__
#define __GLADE_CHOICE_H__

G_BEGIN_DECLS

/* GladeChoice is used for GladeProperties of type "choice"
 * each property of type choice has a list of GladeChoice objects.
 * [see glade-property ]

    <Property>
      ...
	 ...
 	 <Choices>
	   <Choice>
	     <Name>Top Level</Name>
		<Symbol>GTK_WINDOW_TOPLEVEL</Symbol>
	   </Choice>
	   <Choice>
	     <Name>Dialog</Name>
	   ...

 */
struct _GladeChoice {

	gchar *name;     /* Name of the choice to be used in the gui. This
				   * field is translated. Like "Top Level" or "Ok"
				   * (for GTK_WINDOW_TOPLEVEL & GNOME_STOCK_BUTTON_OK
				   */
	gchar *symbol;   /* Symbol for the choice. Like GTK_WINDOW_TOPLEVEL (which
				   * is an integer) or GNOME_STOCK_BUTTON_FOO (which is
				   * not an integer). For integers which are enum'ed values
				   * this symbol is converted to its value
				   */

	gint value;       /* The enum value of the symbol. The symbol GTK_WINDOW_
				    * TOPLEVEL will be 0 and GTK_WINDOW_POPUP will be 1
				    */

	const gchar *string; /* For non-integer values like GNOME_STOCK_BUTTON_OK
					  * it points to a string inside the librarry that
					  * contains it.
					  * See glade-choice.c#glade_string_from_sring
					  */
};


GladeChoice * glade_choice_new (void);

GList * glade_choice_list_new_from_node (xmlNodePtr node);

G_END_DECLS

#endif /* __GLADE_CHOICE_H__ */
