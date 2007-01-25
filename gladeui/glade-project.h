/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PROJECT_H__
#define __GLADE_PROJECT_H__

#include <gladeui/glade-widget.h>
#include <gladeui/glade-command.h>

G_BEGIN_DECLS

#define GLADE_TYPE_PROJECT            (glade_project_get_type ())
#define GLADE_PROJECT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PROJECT, GladeProject))
#define GLADE_PROJECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_PROJECT, GladeProjectClass))
#define GLADE_IS_PROJECT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PROJECT))
#define GLADE_IS_PROJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_PROJECT))
#define GLADE_PROJECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_PROJECT, GladeProjectClass))

typedef struct _GladeProjectClass  GladeProjectClass;

/* A GladeProject is well... a project nothing more nothing less. It is the
 * memory representation of a glade file
 */
struct _GladeProject
{
	GObject object;

	gchar *name;     /* The name of the project like network-conf */

	gchar *path;     /* The full path of the glade file for this project */

	gint   instance; /* How many projects with this name */

	gint   untitled_number; /* A unique number for this project if it is untitled */

	gboolean readonly; /* A flag that is set if the project is readonly */

	gboolean loading;/* A flags that is set when the project is loading */
	
	gboolean changed;    /* A flag that is set when a project has changes
			      * if this flag is not set we don't have to query
			      * for confirmation after a close or exit is
			      * requested
			      */

	GList *objects; /* A list of #GObjects that make up this project.
			 * The objects are stored in no particular order.
			 */

	GList *selection; /* We need to keep the selection in the project
			   * because we have multiple projects and when the
			   * user switchs between them, he will probably
			   * not want to loose the selection. This is a list
			   * of #GtkWidget items.
			   */

	gboolean has_selection; /* Whether the project has a selection */

	GList *undo_stack; /* A stack with the last executed commands */
	GList *prev_redo_item; /* Points to the item previous to the redo items */
	GHashTable *widget_names_allocator; /* hash table with the used widget names */
	GHashTable *widget_old_names; /* widget -> old name of the widget */
	GtkTooltips *tooltips;
	
	GtkAccelGroup *accel_group;

	GHashTable *resources; /* resource filenames & thier associated properties */
	
	gchar *comment; /* XML comment, Glade will preserve whatever comment was
			 * in file, so users can delete or change it.
			 */
			 
	time_t  mtime; /* last UTC modification time of file, or 0 if it could not be read */
};

struct _GladeProjectClass
{
	GObjectClass parent_class;

	void          (*add_object)          (GladeProject *project,
					      GladeWidget  *widget);
	void          (*remove_object)       (GladeProject *project,
					      GladeWidget  *widget);
	
	void          (*undo)                (GladeProject *project);
	void          (*redo)                (GladeProject *project);
	GladeCommand *(*next_undo_item)      (GladeProject *project);
	GladeCommand *(*next_redo_item)      (GladeProject *project);
	void          (*push_undo)           (GladeProject *project,
					      GladeCommand *command);

	void          (*changed)             (GladeProject *project,
					      GladeCommand *command,
					      gboolean      forward);

	void          (*widget_name_changed) (GladeProject *project,
					      GladeWidget  *widget);
	void          (*selection_changed)   (GladeProject *project); 
	void          (*close)               (GladeProject *project);

	void          (*resource_added)      (GladeProject *project,
					      const gchar  *resource);
	void          (*resource_removed)    (GladeProject *project,
					      const gchar  *resource);
	void          (*parse_finished)      (GladeProject *project);
};

LIBGLADEUI_API
GType         glade_project_get_type (void);

LIBGLADEUI_API
GladeProject *glade_project_new                 (gboolean     untitled);
LIBGLADEUI_API
GladeProject *glade_project_open                (const gchar  *path);
LIBGLADEUI_API
gboolean      glade_project_save                (GladeProject *project, 
						 const gchar  *path, 
						 GError      **error);

LIBGLADEUI_API
void          glade_project_undo                (GladeProject *project);
LIBGLADEUI_API
void          glade_project_redo                (GladeProject *project);
LIBGLADEUI_API
GladeCommand *glade_project_next_undo_item      (GladeProject *project);
LIBGLADEUI_API
GladeCommand *glade_project_next_redo_item      (GladeProject *project);
LIBGLADEUI_API
void          glade_project_push_undo           (GladeProject *project, 
						 GladeCommand *cmd);

LIBGLADEUI_API
void          glade_project_reset_path          (GladeProject *project);
LIBGLADEUI_API
gboolean      glade_project_get_readonly        (GladeProject *project);
LIBGLADEUI_API
void          glade_project_add_object          (GladeProject *project, 
						 GladeProject *old_project,
						 GObject      *object);
LIBGLADEUI_API
void          glade_project_remove_object       (GladeProject *project, GObject     *object);
LIBGLADEUI_API
gboolean      glade_project_has_object          (GladeProject *project, GObject     *object);
LIBGLADEUI_API
GladeWidget  *glade_project_get_widget_by_name  (GladeProject *project, const char  *name);
LIBGLADEUI_API
char         *glade_project_new_widget_name     (GladeProject *project, const char  *base_name);
LIBGLADEUI_API
void          glade_project_widget_name_changed (GladeProject *project, GladeWidget *widget,
						 const char   *old_name);
LIBGLADEUI_API
GtkTooltips  *glade_project_get_tooltips        (GladeProject *project);

/* Selection */
LIBGLADEUI_API
gboolean      glade_project_is_selected         (GladeProject *project,
						 GObject      *object);
LIBGLADEUI_API
void          glade_project_selection_set       (GladeProject *project,
						 GObject      *object,
						 gboolean      emit_signal);
LIBGLADEUI_API
void          glade_project_selection_add       (GladeProject *project,
						 GObject      *object,
						 gboolean      emit_signal);
LIBGLADEUI_API
void          glade_project_selection_remove    (GladeProject *project,
						 GObject      *object,
						 gboolean      emit_signal);
LIBGLADEUI_API
void          glade_project_selection_clear     (GladeProject *project,
						 gboolean      emit_signal);
LIBGLADEUI_API
void          glade_project_selection_changed   (GladeProject *project);
LIBGLADEUI_API
GList        *glade_project_selection_get       (GladeProject *project);
LIBGLADEUI_API
gboolean      glade_project_get_has_selection   (GladeProject *project);
LIBGLADEUI_API
void          glade_project_set_accel_group     (GladeProject  *project, 
						 GtkAccelGroup *accel_group);

LIBGLADEUI_API
void          glade_project_set_resource          (GladeProject  *project, 
						   GladeProperty *property,
						   const gchar   *resource);

LIBGLADEUI_API
GList        *glade_project_list_resources        (GladeProject  *project);

LIBGLADEUI_API
gchar        *glade_project_resource_fullpath     (GladeProject  *project,
						   const gchar   *resource);

LIBGLADEUI_API
gchar        *glade_project_display_name          (GladeProject  *project, 
						   gboolean       unsaved_changes,
						   gboolean       tab_aligned,
						   gboolean       mnemonic);

LIBGLADEUI_API 
gboolean      glade_project_is_loading            (GladeProject *project);

LIBGLADEUI_API 
time_t        glade_project_get_file_mtime        (GladeProject *project);

G_END_DECLS

#endif /* __GLADE_PROJECT_H__ */
