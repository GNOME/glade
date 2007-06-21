/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_GTK_H__
#define __GLADE_GTK_H__

#include <gladeui/glade.h>
#include <gtk/gtk.h>

/* Types */

typedef enum {
	GLADEGTK_IMAGE_FILENAME = 0,
	GLADEGTK_IMAGE_STOCK,
	GLADEGTK_IMAGE_ICONTHEME
} GladeGtkImageType;

typedef enum {
	GLADEGTK_BUTTON_LABEL = 0,
	GLADEGTK_BUTTON_STOCK,
	GLADEGTK_BUTTON_CONTAINER
} GladeGtkButtonType;

GType       glade_gtk_image_type_get_type  (void);
GType       glade_gtk_button_type_get_type (void);

GParamSpec *glade_gtk_gnome_ui_info_spec   (void);

GParamSpec *glade_gtk_image_type_spec      (void);

GParamSpec *glade_gtk_button_type_spec     (void);


void
empty (GObject *container, GladeCreateReason reason);

void        glade_gtk_widget_set_property (GladeWidgetAdaptor *adaptor,
			       GObject            *object, 
			       const gchar        *id,
			       const GValue       *value);
			       
void        glade_gtk_widget_get_property (GladeWidgetAdaptor *adaptor,
			       GObject            *object, 
			       const gchar        *id,
			       GValue             *value);
			       
void         glade_gtk_container_post_create (GladeWidgetAdaptor *adaptor,
				 GObject            *container, 
				 GladeCreateReason   reason);
				 
void         glade_gtk_container_replace_child (GladeWidgetAdaptor *adaptor,
				   GtkWidget          *container,
				   GtkWidget          *current,
				   GtkWidget          *new_widget);
				   
void         glade_gtk_container_add_child (GladeWidgetAdaptor *adaptor,
			       GtkWidget          *container,
			       GtkWidget          *child);
			       
void         glade_gtk_container_remove_child (GladeWidgetAdaptor *adaptor,
				  GtkWidget          *container,
				  GtkWidget          *child);	   

void glade_gtk_container_set_child_property (GladeWidgetAdaptor *adaptor,
					GObject            *container,
					GObject            *child,
					const gchar        *property_name,
					const GValue       *value);

void glade_gtk_container_get_child_property (GladeWidgetAdaptor *adaptor,
					GObject            *container,
					GObject            *child,
					const gchar        *property_name,
					GValue             *value);

GList *glade_gtk_container_get_children (GladeWidgetAdaptor  *adaptor,
				  GtkContainer        *container);
				  
				  
void
glade_gtk_box_post_create (GladeWidgetAdaptor *adaptor,
			   GObject            *container, 
			   GladeCreateReason   reason);
			   
void
glade_gtk_box_set_child_property (GladeWidgetAdaptor *adaptor,
				  GObject            *container,
				  GObject            *child,
				  const gchar        *property_name,
				  GValue             *value);
				  
void
glade_gtk_box_get_property (GladeWidgetAdaptor *adaptor,
			    GObject            *object, 
			    const gchar        *id,
			    GValue             *value);
			    
void
glade_gtk_box_set_property (GladeWidgetAdaptor *adaptor,
			    GObject            *object, 
			    const gchar        *id,
			    const GValue       *value);
			    
gboolean
glade_gtk_box_verify_property (GladeWidgetAdaptor *adaptor,
			       GObject            *object, 
			       const gchar        *id,
			       const GValue       *value);
			       
void
glade_gtk_box_add_child (GladeWidgetAdaptor *adaptor,
			 GObject            *object,
			 GObject            *child);
			 
GObject *
glade_gtk_box_get_internal_child (GladeWidgetAdaptor *adaptor,
				  GObject            *object, 
				  const gchar        *name);

void
glade_gtk_box_remove_child (GladeWidgetAdaptor *adaptor,
			    GObject            *object, 
			    GObject            *child);

void
glade_gtk_box_child_action_activate (GladeWidgetAdaptor *adaptor,
				     GObject            *container,
				     GObject            *object,
				     const gchar        *action_path);

void
glade_gtk_table_post_create (GladeWidgetAdaptor *adaptor,
			     GObject            *container, 
			     GladeCreateReason   reason);

void
glade_gtk_table_add_child (GladeWidgetAdaptor *adaptor,
			   GObject            *object, 
			   GObject            *child);
			   
void
glade_gtk_table_remove_child (GladeWidgetAdaptor *adaptor,
			      GObject            *object, 
			      GObject            *child);
			      
gboolean
glade_gtk_table_verify_property (GladeWidgetAdaptor *adaptor,
				 GObject            *object, 
				 const gchar        *id,
				 const GValue       *value);

void
glade_gtk_table_replace_child (GladeWidgetAdaptor *adaptor,
			       GtkWidget          *container,
			       GtkWidget          *current,
			       GtkWidget          *new_widget);			   

void
glade_gtk_table_set_property (GladeWidgetAdaptor *adaptor,
			      GObject            *object, 
			      const gchar        *id,
			      const GValue       *value);

void
glade_gtk_table_set_child_property (GladeWidgetAdaptor *adaptor,
				    GObject            *container,
				    GObject            *child,
				    const gchar        *property_name,
				    GValue             *value);
				    
gboolean
glade_gtk_table_child_verify_property (GladeWidgetAdaptor *adaptor,
				       GObject            *container,
				       GObject            *child,
				       const gchar        *id,
				       GValue             *value);
				       
void
glade_gtk_table_child_action_activate (GladeWidgetAdaptor *adaptor,
				       GObject            *container,
				       GObject            *object,
				       const gchar        *action_path);

void
glade_gtk_frame_post_create (GladeWidgetAdaptor *adaptor,
			     GObject            *frame, 
			     GladeCreateReason   reason);

void
glade_gtk_frame_replace_child (GladeWidgetAdaptor *adaptor,
			       GtkWidget          *container,
			       GtkWidget          *current,
			       GtkWidget          *new_widget);
			       
void
glade_gtk_frame_add_child (GladeWidgetAdaptor *adaptor,
			   GObject            *object, 
			   GObject            *child);
			   
void
glade_gtk_notebook_post_create (GladeWidgetAdaptor *adaptor,
				GObject            *notebook, 
				GladeCreateReason   reason);
				
void
glade_gtk_notebook_set_property (GladeWidgetAdaptor *adaptor,
				 GObject            *object, 
				 const gchar        *id,
				 const GValue       *value);
				 
gboolean
glade_gtk_notebook_verify_property (GladeWidgetAdaptor *adaptor,
				    GObject            *object, 
				    const gchar        *id,
				    const GValue       *value);
				    
void 
glade_gtk_notebook_add_child (GladeWidgetAdaptor *adaptor,
			      GObject            *object, 
			      GObject            *child);
			      
void
glade_gtk_notebook_remove_child (GladeWidgetAdaptor *adaptor,
				 GObject            *object, 
				 GObject            *child);
				 
void
glade_gtk_notebook_replace_child (GladeWidgetAdaptor *adaptor,
				  GtkWidget          *container,
				  GtkWidget          *current,
				  GtkWidget          *new_widget);

gboolean
glade_gtk_notebook_child_verify_property (GladeWidgetAdaptor *adaptor,
					  GObject            *container,
					  GObject            *child,
					  const gchar        *id,
					  GValue             *value);

void
glade_gtk_notebook_set_child_property (GladeWidgetAdaptor *adaptor,
				       GObject            *container,
				       GObject            *child,
				       const gchar        *property_name,
				       const GValue       *value);
				       
void
glade_gtk_notebook_get_child_property (GladeWidgetAdaptor *adaptor,
				       GObject            *container,
				       GObject            *child,
				       const gchar        *property_name,
				       GValue             *value);
				       
void
glade_gtk_notebook_child_action_activate (GladeWidgetAdaptor *adaptor,
					  GObject            *container,
					  GObject            *object,
					  const gchar        *action_path);
					  
void
glade_gtk_paned_post_create (GladeWidgetAdaptor *adaptor,
			     GObject            *paned, 
			     GladeCreateReason   reason);
			     
void 
glade_gtk_paned_add_child (GladeWidgetAdaptor *adaptor,
			   GObject            *object, 
			   GObject            *child);
			   
			   
void 
glade_gtk_paned_remove_child (GladeWidgetAdaptor *adaptor,
			      GObject            *object,
			      GObject            *child);
			      
void
glade_gtk_paned_set_child_property (GladeWidgetAdaptor *adaptor,
				    GObject            *container,
				    GObject            *child,
				    const gchar        *property_name,
				    const GValue       *value);
				    
				    
void
glade_gtk_paned_get_child_property (GladeWidgetAdaptor *adaptor,
				    GObject            *container,
				    GObject            *child,
				    const gchar        *property_name,
				    GValue             *value);
				    
void
glade_gtk_expander_post_create (GladeWidgetAdaptor *adaptor,
				GObject            *expander, 
				GladeCreateReason   reason);
				
void
glade_gtk_expander_replace_child (GladeWidgetAdaptor *adaptor,
				  GtkWidget          *container,
				  GtkWidget          *current,
				  GtkWidget          *new_widget);
				  
void
glade_gtk_expander_add_child (GladeWidgetAdaptor *adaptor,
			      GObject            *object, 
			      GObject            *child);
			      
void
glade_gtk_expander_remove_child (GladeWidgetAdaptor *adaptor,
				 GObject            *object, 
				 GObject            *child);
				 
void
glade_gtk_entry_post_create (GladeWidgetAdaptor *adaptor,
			     GObject            *object,
			     GladeCreateReason   reason);
			     
void
glade_gtk_fixed_layout_post_create (GladeWidgetAdaptor *adaptor,
				    GObject            *object, 
				    GladeCreateReason   reason);
				    
void
glade_gtk_fixed_layout_add_child (GladeWidgetAdaptor  *adaptor,
				  GObject             *object, 
				  GObject             *child);

void
glade_gtk_fixed_layout_remove_child (GladeWidgetAdaptor  *adaptor,
				     GObject             *object, 
				     GObject             *child);
				     
void
glade_gtk_window_post_create (GladeWidgetAdaptor *adaptor,
			      GObject            *object,
			      GladeCreateReason   reason);
			      
void 
glade_gtk_dialog_post_create (GladeWidgetAdaptor *adaptor,
			      GObject            *object, 
			      GladeCreateReason   reason);
			      
GtkWidget *
glade_gtk_dialog_get_internal_child (GladeWidgetAdaptor  *adaptor,
				     GtkDialog           *dialog,
				     const gchar         *name);
				     
GList *
glade_gtk_dialog_get_children (GladeWidgetAdaptor  *adaptor,
			       GtkDialog           *dialog);
			       
void
glade_gtk_dialog_set_property (GladeWidgetAdaptor *adaptor,
			       GObject            *object, 
			       const gchar        *id,
			       const GValue       *value);
			       
void
glade_gtk_file_chooser_widget_post_create (GladeWidgetAdaptor *adaptor,
					   GObject            *object,
					   GladeCreateReason   reason);
					   
void
glade_gtk_button_post_create (GladeWidgetAdaptor  *adaptor,
			      GObject             *button, 
			      GladeCreateReason    reason);
			      
void
glade_gtk_button_set_property (GladeWidgetAdaptor *adaptor,
			       GObject            *object, 
			       const gchar        *id,
			       const GValue       *value);
			       
void
glade_gtk_button_replace_child (GladeWidgetAdaptor  *adaptor,
				GtkWidget           *container,
				GtkWidget           *current,
				GtkWidget           *new_widget);
				
void
glade_gtk_button_add_child (GladeWidgetAdaptor  *adaptor,
			    GObject             *object, 
			    GObject             *child);
			    
void
glade_gtk_button_remove_child (GladeWidgetAdaptor  *adaptor,
			       GObject             *object, 
			       GObject             *child);
			       
void
glade_gtk_image_post_create (GladeWidgetAdaptor  *adaptor,
			     GObject             *object, 
			     GladeCreateReason    reason);
			     
			     
void
glade_gtk_image_set_property (GladeWidgetAdaptor *adaptor,
			      GObject            *object, 
			      const gchar        *id,
			      const GValue       *value);

void
glade_gtk_menu_shell_action_activate (GladeWidgetAdaptor *adaptor,
				      GObject *object,
				      const gchar *action_path);

void
glade_gtk_menu_shell_add_child (GladeWidgetAdaptor  *adaptor, 
			       GObject             *object, 
			       GObject             *child);
			       
void
glade_gtk_menu_shell_remove_child (GladeWidgetAdaptor  *adaptor,
				  GObject             *object, 
				  GObject             *child);
			      
void
glade_gtk_menu_shell_get_child_property (GladeWidgetAdaptor  *adaptor,
					 GObject             *container,
					 GObject             *child,
					 const gchar         *property_name,
					 GValue              *value);
					 
void
glade_gtk_menu_shell_set_child_property (GladeWidgetAdaptor  *adaptor,
					 GObject             *container,
					 GObject             *child,
					 const gchar         *property_name,
					 GValue              *value);
					 
GList *
glade_gtk_menu_item_get_children (GladeWidgetAdaptor *adaptor,
				 GObject *object);
				 
void
glade_gtk_menu_item_add_child (GladeWidgetAdaptor *adaptor,
			       GObject *object, GObject *child);
			       
void
glade_gtk_menu_item_remove_child (GladeWidgetAdaptor *adaptor,
				  GObject *object, GObject *child);				 
				 
void
glade_gtk_menu_item_post_create (GladeWidgetAdaptor *adaptor, 
				 GObject            *object, 
				 GladeCreateReason   reason);
				 
void
glade_gtk_menu_item_set_property (GladeWidgetAdaptor *adaptor,
				  GObject            *object, 
				  const gchar        *id,
				  const GValue       *value);
				  
GObject *
glade_gtk_image_menu_item_get_internal_child (GladeWidgetAdaptor *adaptor,
					      GObject            *parent,
					      const gchar        *name);
					      
void
glade_gtk_image_menu_item_set_property (GladeWidgetAdaptor *adaptor,
					GObject            *object, 
					const gchar        *id,
					const GValue       *value);				  

void
glade_gtk_radio_menu_item_set_property (GladeWidgetAdaptor *adaptor,
					GObject            *object, 
					const gchar        *id,
					const GValue       *value);

void
glade_gtk_menu_item_action_activate (GladeWidgetAdaptor *adaptor,
				     GObject *object,
				     const gchar *action_path);
				     
void
glade_gtk_menu_bar_post_create (GladeWidgetAdaptor *adaptor,
				GObject            *object, 
				GladeCreateReason   reason);
				
void
glade_gtk_toolbar_get_child_property (GladeWidgetAdaptor *adaptor,
				      GObject            *container,
				      GObject            *child,
				      const gchar        *property_name,
				      GValue             *value);
				      
void
glade_gtk_toolbar_set_child_property (GladeWidgetAdaptor *adaptor,
				      GObject            *container,
				      GObject            *child,
				      const gchar        *property_name,
				      GValue             *value);
				      
void
glade_gtk_toolbar_add_child (GladeWidgetAdaptor *adaptor,
			     GObject *object, GObject *child);
			     
void
glade_gtk_toolbar_remove_child (GladeWidgetAdaptor *adaptor, 
				GObject *object, GObject *child);
				
void
glade_gtk_toolbar_action_activate (GladeWidgetAdaptor *adaptor,
				   GObject *object,
				   const gchar *action_path);
				   
void
glade_gtk_tool_item_post_create (GladeWidgetAdaptor *adaptor, 
				 GObject            *object, 
				 GladeCreateReason   reason);
				 
void
glade_gtk_tool_button_set_property (GladeWidgetAdaptor *adaptor,
				    GObject            *object, 
				    const gchar        *id,
				    const GValue       *value);

void
glade_gtk_label_set_property (GladeWidgetAdaptor *adaptor,
			      GObject            *object, 
			      const gchar        *id,
			      const GValue       *value);
			      
void
glade_gtk_text_view_post_create (GladeWidgetAdaptor *adaptor,
				 GObject            *object, 
				 GladeCreateReason   reason);
				 
void
glade_gtk_text_view_set_property (GladeWidgetAdaptor *adaptor,
				  GObject            *object, 
				  const gchar        *id,
				  const GValue       *value);
	
GList *
glade_gtk_combo_get_children (GtkCombo *combo);	
	
				  
void
glade_gtk_combo_box_post_create (GladeWidgetAdaptor *adaptor,
				 GObject            *object, 
				 GladeCreateReason   reason);

void
glade_gtk_combo_box_set_property (GladeWidgetAdaptor *adaptor,
				  GObject            *object, 
				  const gchar        *id,
				  const GValue       *value);

void
glade_gtk_combo_box_entry_post_create (GladeWidgetAdaptor *adaptor,
				       GObject            *object, 
				       GladeCreateReason   reason);
				       
GObject *
glade_gtk_combo_box_entry_get_internal_child (GladeWidgetAdaptor *adaptor,
					      GObject *object, 
					      const gchar *name);

void
glade_gtk_spin_button_set_property (GladeWidgetAdaptor *adaptor,
				    GObject            *object, 
				    const gchar        *id,
				    const GValue       *value);
				    
void
glade_gtk_tree_view_post_create (GladeWidgetAdaptor *adaptor,
				 GObject            *object, 
				 GladeCreateReason   reason);
				 
void
glade_gtk_combo_post_create (GladeWidgetAdaptor *adaptor,
			     GObject            *object,
			     GladeCreateReason   reason);
			     
GObject *
glade_gtk_combo_get_internal_child (GladeWidgetAdaptor *adaptor,
				    GtkCombo           *combo,
				    const gchar        *name);
				    
void
glade_gtk_list_item_post_create (GladeWidgetAdaptor *adaptor,
				 GObject            *object, 
				 GladeCreateReason   reason);
				 
void
glade_gtk_list_item_set_property (GladeWidgetAdaptor *adaptor,
				  GObject            *object, 
				  const gchar        *id,
				  const GValue       *value);
				  
void
glade_gtk_list_item_get_property (GladeWidgetAdaptor *adaptor,
				  GObject            *object, 
				  const gchar        *id,
				  GValue             *value);
				  
void
glade_gtk_listitem_add_child (GladeWidgetAdaptor  *adaptor,
			      GObject             *object, 
			      GObject             *child);
			      
void
glade_gtk_listitem_remove_child (GladeWidgetAdaptor  *adaptor,
				 GObject             *object, 
				 GObject             *child);
				 
void
glade_gtk_assistant_post_create (GladeWidgetAdaptor *adaptor,
				 GObject *object, 
				 GladeCreateReason reason);
				 
void
glade_gtk_assistant_add_child (GladeWidgetAdaptor *adaptor,
			       GObject *container,
			       GObject *child);
			       
void
glade_gtk_assistant_remove_child (GladeWidgetAdaptor *adaptor,
				  GObject *container, 
				  GObject *child);
				  
void
glade_gtk_assistant_replace_child (GladeWidgetAdaptor *adaptor,
				   GObject *container,
				   GObject *current,
				   GObject *new_object);
				   
gboolean
glade_gtk_assistant_verify_property (GladeWidgetAdaptor *adaptor,
				     GObject *object, 
				     const gchar *property_name,
				     const GValue *value);
				     
void
glade_gtk_assistant_set_property (GladeWidgetAdaptor *adaptor,
				  GObject *object,
				  const gchar *property_name,
				  const GValue *value);
				  
void
glade_gtk_assistant_get_property (GladeWidgetAdaptor *adaptor,
				  GObject *object,
				  const gchar *property_name,
				  GValue *value);
				  
void
glade_gtk_assistant_set_child_property (GladeWidgetAdaptor *adaptor,
					GObject            *container,
					GObject            *child,
					const gchar        *property_name,
					const GValue       *value);
					
void
glade_gtk_assistant_get_child_property (GladeWidgetAdaptor *adaptor,
					GObject            *container,
					GObject            *child,
					const gchar        *property_name,
					GValue       *value);
			 		    
#endif /* __GLADE_GTK_H__ */
