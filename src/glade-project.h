/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PROJECT_H__
#define __GLADE_PROJECT_H__

#include "glade-widget.h"

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
	GObject object; /* We emit signals */

	gchar *name;    /* The name of the project like network-conf */

	gchar *path;    /* The full path of the xml file for this project*/

	GtkToggleAction *action; /* The menu item action */

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

	GList *undo_stack; /* A stack with the last executed commands */
	GList *prev_redo_item; /* Points to the item previous to the redo items */
	GHashTable *widget_names_allocator; /* hash table with the used widget names */
	GHashTable *widget_old_names; /* widget -> old name of the widget */
	GtkTooltips *tooltips;
};

struct _GladeProjectClass
{
	GObjectClass parent_class;

	void   (*add_object)          (GladeProject *project,
				       GladeWidget  *widget);
	void   (*remove_object)       (GladeProject *project,
				       GladeWidget  *widget);
	void   (*widget_name_changed) (GladeProject *project,
				       GladeWidget  *widget);
	void   (*selection_changed)   (GladeProject *project); 
};

LIBGLADEUI_API GType         glade_project_get_type (void);

LIBGLADEUI_API GladeProject *glade_project_new                 (gboolean     untitled);
LIBGLADEUI_API GladeProject *glade_project_open                (const gchar  *path);
LIBGLADEUI_API gboolean      glade_project_save                (GladeProject *project, 
								const gchar  *path, 
								GError      **error);
LIBGLADEUI_API void          glade_project_add_object          (GladeProject *project, GObject     *object);
LIBGLADEUI_API void          glade_project_remove_object       (GladeProject *project, GObject     *object);
LIBGLADEUI_API gboolean      glade_project_has_object          (GladeProject *project, GObject     *object);
LIBGLADEUI_API GladeWidget  *glade_project_get_widget_by_name  (GladeProject *project, const char  *name);
LIBGLADEUI_API char         *glade_project_new_widget_name     (GladeProject *project, const char  *base_name);
LIBGLADEUI_API void          glade_project_widget_name_changed (GladeProject *project, GladeWidget *widget,
								const char   *old_name);
LIBGLADEUI_API GtkTooltips  *glade_project_get_tooltips        (GladeProject *project);

LIBGLADEUI_API void          glade_project_changed             (GladeProject *project);

/* Selection */
LIBGLADEUI_API gboolean      glade_project_is_selected         (GladeProject *project,
								GObject      *object);
LIBGLADEUI_API void          glade_project_selection_set       (GladeProject *project,
								GObject      *object,
								gboolean      emit_signal);
LIBGLADEUI_API void          glade_project_selection_add       (GladeProject *project,
								GObject      *object,
								gboolean      emit_signal);
LIBGLADEUI_API void          glade_project_selection_remove    (GladeProject *project,
								GObject      *object,
								gboolean      emit_signal);
LIBGLADEUI_API void          glade_project_selection_clear     (GladeProject *project,
								gboolean      emit_signal);
LIBGLADEUI_API void          glade_project_selection_changed   (GladeProject *project);
LIBGLADEUI_API GList        *glade_project_selection_get       (GladeProject *project);

LIBGLADEUI_API GtkWidget    *glade_project_get_menuitem          (GladeProject *project);
LIBGLADEUI_API guint         glade_project_get_menuitem_merge_id (GladeProject *project);

G_END_DECLS

#endif /* __GLADE_PROJECT_H__ */

