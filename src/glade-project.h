/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PROJECT_H__
#define __GLADE_PROJECT_H__

G_BEGIN_DECLS

#define GLADE_PROJECT(obj)          GTK_CHECK_CAST (obj, glade_project_get_type (), GladeProject)
#define GLADE_PROJECT_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, glade_project_get_type (), GladeProjectClass)
#define GLADE_IS_PROJECT(obj)       GTK_CHECK_TYPE (obj, glade_project_get_type ())

typedef struct _GladeProjectClass  GladeProjectClass;

/* A GladeProject is well... a project nothing more nothing less. It is the
 * memory representation of a glade file */
struct _GladeProject
{
	GtkObject object; /* We emit signals */

	gchar *name;         /* The name of the project like network-conf */
	gchar *path;         /* The full path of the xml file for this project*/

	GtkItemFactoryEntry entry; /* The menu entry in the /Project menu */

	gboolean changed;    /* A flag that is set when a project has changes
			      * if this flag is not set we don't have to query
			      * for confirmation after a close or exit is
			      * requested
			      */

	GList *widgets; /* A list of GladeWidgets that make up this project.
			 * The widgets are stored in no particular order.
			 */

	GList *selection; /* We need to keep the selection in the project
			   * because we have multiple projects and when the
			   * user switchs between them, he will probably
			   * not want to loose the selection. This is a list
			   * of GladeWidget items.
			   */

	GList *undo_stack; /* A stack with the last executed commands */
	GList *prev_redo_item; /* Points to the item previous to the redo items */
	GHashTable *unique_id_hash; /* hash table with the latest used widget name */
};

struct _GladeProjectClass
{
	GtkObjectClass parent_class;

	void   (*add_widget)          (GladeProject *project,
				       GladeWidget  *widget);
	void   (*remove_widget)       (GladeProject *project,
				       GladeWidget  *widget);
	void   (*widget_name_changed) (GladeProject *project,
				       GladeWidget  *widget);
	void   (*selection_changed)   (GladeProject *project); 
};


guint glade_project_get_type (void);

GladeProject *glade_project_new (gboolean untitled);


/* Project operations */
gboolean glade_project_save (GladeProject *project, const gchar *path);
gboolean glade_project_open (const gchar *path);


/* Widget related stuff */
void glade_project_remove_widget (GladeWidget *widget);
void glade_project_add_widget (GladeProject  *project,
			       GladeWidget *glade_widget);

GladeWidget *glade_project_get_widget_by_name (GladeProject *project, const char *name);
char *glade_project_new_widget_name (GladeProject *project, const char *base_name);

void glade_project_widget_name_changed (GladeProject *project,
					GladeWidget *widget);

/* Selection */
void glade_project_selection_set     (GladeWidget *widget, gboolean emit_signal);
void glade_project_selection_add     (GladeWidget *widget, gboolean emit_signal);
void glade_project_selection_remove  (GladeWidget *widget, gboolean emit_signal);
void glade_project_selection_clear   (GladeProject *project, gboolean emit_signal);
void glade_project_selection_changed (GladeProject *project);

GList *glade_project_selection_get (GladeProject *project);

G_END_DECLS

#endif /* __GLADE_PROJECT_H__ */

