/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Naba Kumar <naba@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include "glade.h"
#include "glade-palette.h"
#include "glade-editor.h"
#include "glade-clipboard.h"
#include "glade-clipboard-view.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-project.h"
#include "glade-project-view.h"
#include "glade-placeholder.h"
#include "glade-command.h"
#include "glade-debug.h"
#include "glade-utils.h"
#include "glade-cursor.h"
#include "glade-catalog.h"
#include "glade-transform.h"
#include "glade-app.h"
#include "glade-paths.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtkstock.h>

struct _GladeAppPriv {
	
	/* Class private member data */
	GtkWidget *window;
	
	GladePalette *palette;         /* See glade-palette */
	GladeProject *active_project;  /* Currently active project (if there is at least one
				        * project; then this is always valid) */
	GladeEditor *editor;           /* See glade-editor */
	GladeClipboard *clipboard;     /* See glade-clipboard */
	GladeProjectView *active_view; /* See glade-project-view */
	GList *catalogs;               /* See glade-catalog */
	GladeWidgetClass *add_class;   /* The GladeWidgetClass that we are about
					* to add to a container. NULL if no
					* class is to be added. This also has to
					* be in sync with the depressed button
					* in the GladePalette
					*/
	GladeWidgetClass *alt_class;
	
	GList *views;    /* A list of GladeProjectView item */
	GList *projects; /* The list of Projects */
	
	GKeyFile *config;/* The configuration file */
};

enum
{
	UPDATE_UI_SIGNAL,
	LAST_SIGNAL
};

static guint glade_app_signals[LAST_SIGNAL] = { 0 };

gchar *glade_pixmaps_dir = GLADE_PIXMAPSDIR;
gchar *glade_catalogs_dir = GLADE_CATALOGSDIR;
gchar *glade_modules_dir = GLADE_MODULESDIR;
gchar *glade_locale_dir = GLADE_LOCALEDIR;
gchar *glade_icon_dir = GLADE_ICONDIR;
gboolean glade_verbose = FALSE;

static GObjectClass* parent_class = NULL;

static void
glade_app_dispose (GObject *app)
{
	GladeAppPriv *priv = GLADE_APP (app)->priv;
	
	if (priv->editor)
	{
		g_object_unref (priv->editor);
		priv->editor = NULL;
	}
	if (priv->palette)
	{
		g_object_unref (priv->palette);
		priv->palette = NULL;
	}
	if (priv->clipboard)
	{
		gtk_widget_destroy (GTK_WIDGET (priv->clipboard->view));
		priv->clipboard = NULL;
	}
	/* FIXME: Remove views and projects */
	
	if (priv->config)
	{
		g_key_file_free (priv->config);
		priv->config = NULL;
	}
	
	if (parent_class->dispose)
		parent_class->dispose (app);
}

static void
glade_app_finalize (GObject *app)
{
#ifdef G_OS_WIN32 
	g_free (glade_pixmaps_dir);
	g_free (glade_catalogs_dir);
	g_free (glade_modules_dir);
	g_free (glade_locale_dir);
	g_free (glade_icon_dir);
#endif

	g_free (GLADE_APP (app)->priv);
	if (parent_class->finalize)
		parent_class->finalize (app);
}

static void
glade_app_class_init (GladeAppClass * klass)
{
	GObjectClass *object_class;
	g_return_if_fail (klass != NULL);
	
	parent_class = g_type_class_peek_parent (klass);
	object_class = (GObjectClass *) klass;
	
	glade_app_signals[UPDATE_UI_SIGNAL] =
		g_signal_new ("update-ui",
					G_TYPE_FROM_CLASS (object_class),
					G_SIGNAL_RUN_FIRST,
					G_STRUCT_OFFSET (GladeAppClass,
									 update_ui_signal),
					NULL, NULL,
					g_cclosure_marshal_VOID__VOID,
					G_TYPE_NONE, 0);

	object_class->dispose = glade_app_dispose;
	object_class->finalize = glade_app_finalize;
}

static void
on_palette_button_clicked (GladePalette *palette, GladeApp *app)
{
	GladeWidgetClass *class;

	g_return_if_fail (GLADE_IS_PALETTE (palette));
	class = palette->current;

	/* class may be NULL if the selector was pressed */
	if (class && g_type_is_a (class->type, GTK_TYPE_WINDOW))
	{
		glade_command_create (class, NULL, NULL, app->priv->active_project);
		glade_palette_unselect_widget (palette);
		app->priv->add_class = NULL;
	}
	else if ((app->priv->add_class = class) != NULL)
		app->priv->alt_class = class;
}

static void
on_palette_catalog_changed (GladePalette *palette, GladeApp *app)
{
	g_return_if_fail (GLADE_IS_PALETTE (palette));

	glade_palette_unselect_widget (palette);
	app->priv->alt_class = app->priv->add_class = NULL;
}

static gboolean
clipboard_view_on_delete_cb (GtkWidget *clipboard_view, GdkEvent *e, GladeApp *app)
{
	glade_util_hide_window (GTK_WINDOW (clipboard_view));
	return TRUE;
}

/**
 * glade_app_config_load
 * @app:
 * Returns: a new GKeyFile
 */
static GKeyFile *
glade_app_config_load (GladeApp *app)
{
	GKeyFile *config = g_key_file_new ();
	gchar *filename;

	filename = g_build_filename (g_get_user_config_dir (), GLADE_CONFIG_FILENAME, NULL);

	g_key_file_load_from_file (config, filename, G_KEY_FILE_NONE, NULL);
	
	g_free (filename);
	
	return config;
}

/**
 * glade_app_config_save
 * @app:
 *
 * Saves the GKeyFile to "g_get_user_config_dir()/GLADE3_CONFIG_FILENAME"
 *
 * Return 0 on success.
 */
gint
glade_app_config_save (GladeApp *app)
{
	GIOChannel *fd;
	gchar *data, *filename;
	const gchar *config_dir = g_get_user_config_dir ();
	GError *error = NULL;
	gsize size;
	
	/* Just in case... try to create the config directory */
	g_mkdir (config_dir, S_IRWXU);
	
	filename = g_build_filename (config_dir, GLADE_CONFIG_FILENAME, NULL);

	fd = g_io_channel_new_file (filename, "w", &error);

	if (error == NULL)
		data = g_key_file_to_data (app->priv->config, &size, &error);
	
	if (error == NULL)
		g_io_channel_write_chars (fd, data, size, NULL, &error);

	/* Free resources */	
	if (error)
	{
		g_warning (error->message);
		g_error_free (error);
	}
	
	if (fd)
	{
		g_io_channel_shutdown(fd, TRUE, NULL);
		g_io_channel_unref (fd);
	}
	
	if (data) g_free (data);

	g_free (filename);
	
	return (error) ? 1 : 0;
}

static void
glade_app_init (GladeApp *app)
{
	static gboolean initialized = FALSE;
	
	if (!initialized)
	{
#ifdef G_OS_WIN32
		gchar *prefix;
	
		prefix = g_win32_get_package_installation_directory (NULL, NULL);
		glade_pixmaps_dir = g_build_filename (prefix, "share", PACKAGE, "pixmaps", NULL);
		glade_catalogs_dir = g_build_filename (prefix, "share", PACKAGE, "catalogs", NULL);
		glade_modules_dir = g_build_filename (prefix, "lib", PACKAGE, "modules", NULL);
		glade_locale_dir = g_build_filename (prefix, "share", "locale", NULL);
		glade_icon_dir = g_build_filename (prefix, "share", "pixmaps", NULL);
		g_free (prefix);
#endif
		/* register transformation functions */
		glade_register_transformations ();
	
		/*
		 * 1. Init the cursors
		 * 2. Create the catalog
		 * 3. Create the project window
		 */
		glade_cursor_init ();
		initialized = TRUE;
	}
	app->priv = g_new0 (GladeAppPriv, 1);
	
	app->priv->add_class = NULL;
	app->priv->alt_class = NULL;
	
	/* Initialize app objects */
	app->priv->catalogs = glade_catalog_load_all ();
	
	/* Create palette */
	app->priv->palette = glade_palette_new (app->priv->catalogs);
	g_object_ref (app->priv->palette);
	gtk_object_sink (GTK_OBJECT (app->priv->palette));
	gtk_widget_show_all (GTK_WIDGET (app->priv->palette));
	g_signal_connect (G_OBJECT (app->priv->palette), "toggled",
			  G_CALLBACK (on_palette_button_clicked), app);
	g_signal_connect (G_OBJECT (app->priv->palette), "catalog-changed",
			  G_CALLBACK (on_palette_catalog_changed), app);

	/* Create Editor */
	app->priv->editor = GLADE_EDITOR (glade_editor_new ());
	g_object_ref (app->priv->editor);
	gtk_object_sink (GTK_OBJECT (app->priv->editor));
	gtk_widget_show_all (GTK_WIDGET (app->priv->editor));
	
	/* Create clipboard */
	app->priv->clipboard = glade_clipboard_new ();
	app->priv->clipboard->view = glade_clipboard_view_new (app->priv->clipboard);
	gtk_window_set_title (GTK_WINDOW (app->priv->clipboard->view), _("Clipboard"));
	g_signal_connect_after (G_OBJECT (app->priv->clipboard->view), "delete_event",
			  G_CALLBACK (clipboard_view_on_delete_cb),
			  app);

	/* Load the configuration file */
	app->priv->config = glade_app_config_load (app);
}

GType
glade_app_get_type ()
{
	static GType obj_type = 0;
	
	if (!obj_type)
	{
		static const GTypeInfo obj_info = 
		{
			sizeof (GladeAppClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) glade_app_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,           /* class_data */
			sizeof (GladeApp),
			0,              /* n_preallocs */
			(GInstanceInitFunc) glade_app_init,
			NULL            /* value_table */
		};
		obj_type = g_type_register_static (G_TYPE_OBJECT,
		                                   "GladeApp", &obj_info, 0);
	}
	return obj_type;
}

GladeApp *
glade_app_new (void)
{
	GladeApp *app;
	
	app = g_object_new (GLADE_TYPE_APP, NULL);
	return app;
}

void
glade_app_update_ui (GladeApp* app)
{
	g_signal_emit_by_name (app, "update-ui");
}

void
glade_app_set_window (GladeApp* app, GtkWidget *window)
{
	app->priv->window = window;
}

GtkWidget*
glade_app_get_window (GladeApp* app)
{
	return app->priv->window;
}

GladeEditor*
glade_app_get_editor (GladeApp* app)
{
	return app->priv->editor;
}

GladeWidgetClass*
glade_app_get_add_class (GladeApp* app)
{
	return app->priv->add_class;
}

GladeWidgetClass*
glade_app_get_alt_class (GladeApp* app)
{
	return app->priv->alt_class;
}

GladePalette*
glade_app_get_palette (GladeApp* app)
{
	return app->priv->palette;
}

GladeClipboard*
glade_app_get_clipboard (GladeApp* app)
{
	return app->priv->clipboard;
}

GtkWidget*
glade_app_get_clipboard_view (GladeApp* app)
{
	return app->priv->clipboard->view;
}

GladeProject*
glade_app_get_active_project (GladeApp* app)
{
	return app->priv->active_project;
}

GList*
glade_app_get_projects (GladeApp *app)
{
	return app->priv->projects;
}

GKeyFile*
glade_app_get_config (GladeApp *app)
{
	return app->priv->config;
}

void
glade_app_add_project_view (GladeApp *app, GladeProjectView *view)
{
	app->priv->views = g_list_prepend (app->priv->views, view);
	app->priv->active_view = view;
	if (app->priv->active_project)
		glade_project_view_set_project (view, app->priv->active_project);
}

static void
on_widget_name_changed_cb (GladeProject *project,
			    GladeWidget *widget,
			    GladeEditor *editor)
{
	glade_editor_update_widget_name (editor);
}

static void
on_project_selection_changed_cb (GladeProject *project, GladeApp *app)
{
	GList *list;
	gint num;

	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (GLADE_IS_APP (app));

	if (app->priv->editor)
	{
		list = glade_project_selection_get (project);
		num = g_list_length (list);
		if (num == 1 && !GLADE_IS_PLACEHOLDER (list->data))
			glade_editor_load_widget (app->priv->editor,
						  glade_widget_get_from_gobject
						  (G_OBJECT (list->data)));
		else
			glade_editor_load_widget (app->priv->editor, NULL);
	}
}

gboolean
glade_app_is_project_loaded (GladeApp *app, const gchar *project_path)
{
	GList    *list;
	gchar    *project_base;
	gboolean  loaded = FALSE;

	if (project_path == NULL) return FALSE;

	/* Only one project with the same basename can be loaded simultainiously */
	project_base = g_path_get_basename (project_path);
	
	for (list = app->priv->projects; list; list = list->next)
	{
		GladeProject *cur_project = GLADE_PROJECT (list->data);
		gchar        *this_base;

		if (cur_project->path) 
		{
			this_base = g_path_get_basename (cur_project->path);

			if (!strcmp (this_base, project_base))
				loaded = TRUE;

			g_free (this_base);

			if (loaded) 
				break;
		}
	}

	g_free (project_base);
	return loaded;
}

void
glade_app_add_project (GladeApp *app, GladeProject *project)
{
 	g_return_if_fail (GLADE_IS_PROJECT (project));

	/* If the project was previously loaded, don't re-load */
	if (glade_app_is_project_loaded (app, project->path))
	{
		glade_app_set_project (app, project);
		return;
	}
	
 	app->priv->projects = g_list_prepend (app->priv->projects, project);
	
	/* connect to the project signals so that the editor can be updated */
	g_signal_connect (G_OBJECT (project), "widget_name_changed",
			  G_CALLBACK (on_widget_name_changed_cb), app->priv->editor);
	g_signal_connect (G_OBJECT (project), "selection_changed",
			  G_CALLBACK (on_project_selection_changed_cb), app);
	
	glade_app_set_project (app, project);
	/* make sure the palette is sensitive */
	gtk_widget_set_sensitive (GTK_WIDGET (app->priv->palette), TRUE);
}

void
glade_app_remove_project (GladeApp *app, GladeProject *project)
{
	GList *list;
	for (list = project->objects; list; list = list->next)
	{
		GObject *object = list->data;
		if (GTK_IS_WIDGET (object) && GTK_WIDGET_TOPLEVEL (object))
			gtk_widget_destroy (GTK_WIDGET(object));
	}

	app->priv->projects = g_list_remove (app->priv->projects, project);
	
	/* this is needed to prevent clearing the selection of a closed project 
     */
	app->priv->active_project = NULL;
	
	/* If no more projects */
	if (app->priv->projects == NULL)
	{
		GList *list;
		for (list = app->priv->views; list; list = list->next)
		{
			GladeProjectView *view;

			view = GLADE_PROJECT_VIEW (list->data);
			glade_project_view_set_project (view, NULL);
		}
		glade_editor_load_widget (app->priv->editor, NULL);
		gtk_widget_set_sensitive (GTK_WIDGET (app->priv->palette), FALSE);
		return;
	}
	glade_app_set_project (app, app->priv->projects->data);
}

void
glade_app_set_project (GladeApp *app, GladeProject *project)
{
	GList *list;

	g_return_if_fail (GLADE_IS_PROJECT (project));

	if (app->priv->active_project == project)
		return;

	if (!g_list_find (app->priv->projects, project))
	{
		g_warning ("Could not set project because it could not "
			   " be found in the app->priv->project list\n");
		return;
	}

	/* clear the selection in the previous project */
	if (app->priv->active_project)
		glade_project_selection_clear (app->priv->active_project, FALSE);

	app->priv->active_project = project;

	for (list = app->priv->views; list; list = list->next)
	{
		GladeProjectView *view = list->data;
		glade_project_view_set_project (view, project);
	}

	/* trigger the selection changed signal to update the editor */
	glade_project_selection_changed (project);
	
	/* Update UI */
	glade_app_update_ui (app);
}

void
glade_app_command_copy (GladeApp *app)
{
	if (app->priv->active_project)
	{
		glade_util_copy_selection ();
		/* update UI. */
		glade_app_update_ui (app);
	}
}

void
glade_app_command_cut (GladeApp *app)
{
	if (app->priv->active_project)
	{
		glade_util_cut_selection ();
		/* Update UI. */
		glade_app_update_ui (app);
	}
}

void
glade_app_command_paste (GladeApp *app)
{
	if (app->priv->active_project)
	{
		GList *list = glade_project_selection_get (app->priv->active_project);

		glade_util_paste_clipboard (NULL, list ? list->data : NULL);
		/* Update UI. */
		glade_app_update_ui (app);
	}
}

void
glade_app_command_delete (GladeApp *app)
{
	/* glade_util_delete_selection performs a glade_command_delete
	 * on each of the selected widgets */
	if (app->priv->active_project)
	{
		glade_util_delete_selection ();
		/* Update UI. */
		glade_app_update_ui (app);
	}
}

void
glade_app_command_undo (GladeApp *app)
{
	if (app->priv->active_project)
	{
		glade_command_undo (app->priv->active_project);
		glade_editor_refresh (app->priv->editor);
		/* Update UI. */
		glade_app_update_ui (app);
	}
}

void
glade_app_command_redo (GladeApp *app)
{
	if (app->priv->active_project)
	{
		glade_command_redo (app->priv->active_project);
		glade_editor_refresh (app->priv->editor);
		/* Update UI. */
		glade_app_update_ui (app);
	}
}

/* Default application convinience functions */

static GladeApp *glade_default_app = NULL;

void
glade_default_app_set (GladeApp *app)
{
	if (app)
		g_object_ref (G_OBJECT (app));
	if (glade_default_app)
		g_object_unref (glade_default_app);
	glade_default_app = app;
}

GtkWidget*
glade_default_app_get_window (void)
{
	g_return_val_if_fail (glade_default_app != NULL, NULL);
	return glade_app_get_window (glade_default_app);
}

GladeEditor*
glade_default_app_get_editor (void)
{
	g_return_val_if_fail (glade_default_app != NULL, NULL);
	return glade_app_get_editor (glade_default_app);
}

GladeWidgetClass*
glade_default_app_get_add_class (void)
{
	g_return_val_if_fail (glade_default_app != NULL, NULL);
	return glade_app_get_add_class (glade_default_app);
}

GladeWidgetClass*
glade_default_app_get_alt_class (void)
{
	g_return_val_if_fail (glade_default_app != NULL, NULL);
	return glade_app_get_alt_class (glade_default_app);
}

GladePalette*
glade_default_app_get_palette (void)
{
	g_return_val_if_fail (glade_default_app != NULL, NULL);
	return glade_app_get_palette (glade_default_app);
}

GladeClipboard*
glade_default_app_get_clipboard (void)
{
	g_return_val_if_fail (glade_default_app != NULL, NULL);
	return glade_app_get_clipboard (glade_default_app);
}

GList*
glade_default_app_get_projects (void)
{
	g_return_val_if_fail (glade_default_app != NULL, NULL);
	return glade_app_get_projects (glade_default_app);
}

GladeProject*
glade_default_app_get_active_project (void)
{
	g_return_val_if_fail (glade_default_app != NULL, NULL);
	return glade_app_get_active_project (glade_default_app);
}

void
glade_default_app_update_ui (void)
{
	g_return_if_fail (glade_default_app != NULL);
	glade_app_update_ui (glade_default_app);
}

GList*
glade_default_app_get_selection (void)
{
	GList *selection = NULL, *list;
	GladeProject *project;

	for (list = glade_default_app_get_projects (); 
	     list && list->data; list = list->next)
	{
		/* Only one project may have selection at a time
		 */
		project = list->data;
		if (glade_project_selection_get (project))
		{
			selection = glade_project_selection_get (project);
			break;
		}
	}
	return selection;
}


gboolean
glade_default_app_is_selected (GObject      *object)
{
	return (g_list_find (glade_default_app_get_selection (), 
			     object) != NULL);
}

void
glade_default_app_selection_set (GObject      *object,
				 gboolean      emit_signal)
{
	GList        *list;
	GladeProject *project;

	for (list = glade_default_app_get_projects ();
	     list && list->data; list = list->next)
	{
		project = list->data;
		if (glade_project_has_object (project, object))
			glade_project_selection_set (project, 
						     object, 
						     emit_signal);
		else
			glade_project_selection_clear (project, emit_signal);
	}

	/* Instead of calling selection_set after all
	 * the selection_clear calls (lazy).
	 */
	if (GTK_IS_WIDGET (object))
		glade_util_add_selection (GTK_WIDGET (object));
}

void
glade_default_app_selection_add (GObject      *object,
				 gboolean      emit_signal)
{
	GList        *list;
	GladeWidget  *widget   = glade_widget_get_from_gobject (object),  
		*selected;
	GladeProject *project  = glade_widget_get_project (widget);

	/* Ignore request if the there is a selection 
	 * from another project.
	 */
	if ((list = glade_default_app_get_selection ()) != NULL)
	{
		selected = glade_widget_get_from_gobject (list->data);
		if (glade_widget_get_project (selected) != project)
			return;
	}
	glade_project_selection_add (project, object, emit_signal);
}

void
glade_default_app_selection_remove (GObject      *object,
				    gboolean      emit_signal)
{
	GladeWidget  *widget   = glade_widget_get_from_gobject (object);
	GladeProject *project  = glade_widget_get_project (widget);;

	glade_project_selection_remove (project, object, emit_signal);
}

void
glade_default_app_selection_clear (gboolean      emit_signal)
{
	GList        *list;
	GladeProject *project;

	for (list = glade_default_app_get_projects ();
	     list && list->data; list = list->next)
	{
		project = list->data;
		glade_project_selection_clear (project, emit_signal);
	}
}

void
glade_default_app_selection_changed (void)
{
	GList        *list;
	GladeProject *project;

	for (list = glade_default_app_get_projects ();
	     list && list->data; list = list->next)
	{
		project = list->data;
		glade_project_selection_changed (project);
	}
}


G_END_DECLS
