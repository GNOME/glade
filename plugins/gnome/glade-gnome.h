
#ifndef __GLADE_GNOME_H__
#define __GLADE_GNOME_H__

#include <gladeui/glade.h>
#include <gtk/gtk.h>

void
glade_gnomeui_init (void);

void
glade_gnome_icon_entry_set_property (GladeWidgetAdaptor *adaptor,
				     GObject            *object,
				     const gchar        *id,
				     const GValue       *value);

void
glade_gnome_app_post_create (GladeWidgetAdaptor *adaptor,
			     GObject            *object, 
			     GladeCreateReason   reason);
			     
GObject *
glade_gnome_app_get_internal_child (GladeWidgetAdaptor  *adaptor,
				    GObject             *object, 
				    const gchar         *name);
				    
GList *
glade_gnome_app_get_children (GladeWidgetAdaptor  *adaptor,
			      GObject             *object);
			      

void
glade_gnome_app_set_child_property (GladeWidgetAdaptor  *adaptor,
				    GObject             *container,
				    GObject             *child,
				    const gchar         *property_name,
				    GValue              *value);


void
glade_gnome_app_get_child_property (GladeWidgetAdaptor  *adaptor,
				    GObject             *container,
				    GObject             *child,
				    const gchar         *property_name,
				    GValue              *value);
				    
				    
void
glade_gnome_app_set_property (GladeWidgetAdaptor *adaptor,
			      GObject            *object,
			      const gchar        *id,
			      const GValue       *value);

GType gnome_app_bar_get_type (void);


void
glade_gnome_app_bar_post_create (GladeWidgetAdaptor  *adaptor,
				 GObject             *object, 
				 GladeCreateReason    reason);
				 

void
glade_gnome_date_edit_post_create (GladeWidgetAdaptor  *adaptor,
				   GObject             *object, 
				   GladeCreateReason    reason);
				   
void
glade_gnome_druid_post_create (GladeWidgetAdaptor  *adaptor,
			       GObject             *object, 
			       GladeCreateReason    reason);
			       
			       
void
glade_gnome_druid_add_child (GladeWidgetAdaptor  *adaptor,
			     GObject             *container, 
			     GObject             *child);


void
glade_gnome_druid_remove_child (GladeWidgetAdaptor  *adaptor,
				GObject             *container, 
				GObject             *child);

void
glade_gnome_druid_set_child_property (GladeWidgetAdaptor  *adaptor,
				      GObject             *container,
				      GObject             *child,
				      const gchar         *property_name,
				      GValue              *value);

void
glade_gnome_druid_get_child_property (GladeWidgetAdaptor  *adaptor,
				      GObject             *container,
				      GObject             *child,
				      const gchar         *property_name,
				      GValue              *value);


void
glade_gnome_dps_post_create (GladeWidgetAdaptor  *adaptor,
			     GObject             *object, 
			     GladeCreateReason    reason);
			     
GObject *
glade_gnome_dps_get_internal_child (GladeWidgetAdaptor  *adaptor,
				    GObject             *object, 
				    const gchar         *name);
				    
GList *
glade_gnome_dps_get_children (GladeWidgetAdaptor  *adaptor,
			      GObject             *object);
			      
void
glade_gnome_dps_set_property (GladeWidgetAdaptor *adaptor,
			      GObject            *object,
			      const gchar        *id,
			      const GValue       *value);

GParamSpec *
glade_gnome_dpe_position_spec (void);

void
glade_gnome_dpe_set_property (GladeWidgetAdaptor *adaptor,
			      GObject            *object,
			      const gchar        *id,
			      const GValue       *value);
			      
void
glade_gnome_canvas_set_property (GladeWidgetAdaptor *adaptor,
				 GObject            *object,
				 const gchar        *id,
				 const GValue       *value);
				 
void
glade_gnome_dialog_post_create (GladeWidgetAdaptor  *adaptor,
				GObject             *object, 
				GladeCreateReason    reason);

GObject *
glade_gnome_dialog_get_internal_child (GladeWidgetAdaptor  *adaptor,
				       GObject             *object, 
				       const gchar         *name);
				       
GList *
glade_gnome_dialog_get_children (GladeWidgetAdaptor  *adaptor,
				 GObject             *object);
				 
				 
void
glade_gnome_about_dialog_post_create (GladeWidgetAdaptor  *adaptor,
				      GObject             *object, 
				      GladeCreateReason    reason);			     
			     
void
glade_gnome_about_dialog_set_property (GladeWidgetAdaptor *adaptor,
				       GObject            *object,
				       const gchar        *id,
				       const GValue       *value);
				       
GParamSpec *
glade_gnome_message_box_type_spec (void);


void
glade_gnome_message_box_set_property (GladeWidgetAdaptor *adaptor,
				      GObject            *object,
				      const gchar        *id,
				      const GValue       *value);
				      
				      
GObject *
glade_gnome_entry_get_internal_child (GladeWidgetAdaptor  *adaptor,
				      GObject             *object, 
				      const gchar         *name); 
			     
void
glade_gnome_entry_post_create (GladeWidgetAdaptor  *adaptor,
			       GObject             *object, 
			       GladeCreateReason    reason);
			       
GList *
glade_gnome_entry_get_children (GladeWidgetAdaptor  *adaptor,
				GObject             *object);
				
				
void
glade_gnome_entry_set_property (GladeWidgetAdaptor *adaptor,
				GObject            *object,
				const gchar        *id,
				const GValue       *value);
				
void
glade_gnome_file_entry_set_property (GladeWidgetAdaptor *adaptor,
				     GObject            *object,
				     const gchar        *id,
				     const GValue       *value);
				     
void
glade_gnome_font_picker_set_property (GladeWidgetAdaptor *adaptor,
				      GObject            *object,
				      const gchar        *id,
				      const GValue       *value);
				      
GList *
glade_gnome_font_picker_get_children (GladeWidgetAdaptor  *adaptor,
				      GObject             *object);

void
glade_gnome_font_picker_add_child (GladeWidgetAdaptor  *adaptor,
				   GtkWidget           *container, 
				   GtkWidget           *child);				      				      											       

void
glade_gnome_font_picker_remove_child (GladeWidgetAdaptor  *adaptor,
				      GtkWidget           *container, 
				      GtkWidget           *child);
				      

void
glade_gnome_font_picker_replace_child (GladeWidgetAdaptor  *adaptor,
				       GtkWidget           *container,
				       GtkWidget           *current,
				       GtkWidget           *new_widget);
				       
void
glade_gnome_icon_list_post_create (GladeWidgetAdaptor  *adaptor,
				   GObject             *object, 
				   GladeCreateReason    reason);
				   				       
GParamSpec *
glade_gnome_icon_list_selection_mode_spec (void);


void
glade_gnome_icon_list_set_property (GladeWidgetAdaptor *adaptor,
				    GObject            *object,
				    const gchar        *id,
				    const GValue       *value);

void
glade_gnome_pixmap_set_property (GladeWidgetAdaptor *adaptor,
				 GObject            *object,
				 const gchar        *id,
				 const GValue       *value);


GParamSpec *
glade_gnome_bonobo_dock_placement_spec (void);


GParamSpec *
glade_gnome_bonobo_dock_item_behavior_spec (void);


GParamSpec *
glade_gnome_gtk_pack_type_spec (void);


void
glade_gnome_bonobodock_add_child (GladeWidgetAdaptor  *adaptor,
				  GObject             *object,
				  GObject             *child);

void
glade_gnome_bonobodock_remove_child (GladeWidgetAdaptor  *adaptor,
				     GObject             *object, 
				     GObject             *child);
				     
GList *
glade_gnome_bonobodock_get_children (GladeWidgetAdaptor  *adaptor,
				     GObject             *object);				     
				  

void
glade_gnome_bonobodock_replace_child (GladeWidgetAdaptor  *adaptor,
				      GtkWidget           *container,
				      GtkWidget           *current,
				      GtkWidget           *new_widget);

void
glade_gnome_bonobodock_set_child_property (GladeWidgetAdaptor  *adaptor,
					   GObject             *container,
					   GObject             *child,
					   const gchar         *property_name,
					   GValue              *value);
					   
void
glade_gnome_bonobodock_get_child_property (GladeWidgetAdaptor  *adaptor,
					   GObject             *container,
					   GObject             *child,
					   const gchar         *property_name,
					   GValue              *value);
					   
void
glade_gnome_bonobodock_set_property (GladeWidgetAdaptor *adaptor,
				     GObject            *object,
				     const gchar        *id,
				     const GValue       *value);	   
			       			     
#endif /* __GLADE_GNOME_H__ */
