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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

	GtkWindow *transient_parent; /* If set by glade_app_set_transient_parent(); this
				      * will be used as the transient parent of all toplevel
				      * GladeWidgets.
				      */
	GtkAccelGroup *accel_group;	/* Default acceleration group for this app */
	GList *undo_list, *redo_list;	/* Lists of buttons to refresh in update-ui signal */
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
glade_app_refresh_undo_redo_button (GladeApp *app,
				    GtkWidget *button,
				    gboolean undo)
{
	GladeCommand *command = NULL;
	static GtkTooltips *button_tips = NULL;
	GladeProject *project;
	gchar *desc;

	if (button_tips == NULL)
		button_tips = gtk_tooltips_new ();
	
	if ((project = glade_app_get_active_project (app)) != NULL)
	{
		if (undo)
			command = glade_command_next_undo_item (project);
		else
			command = glade_command_next_redo_item (project);
	}

	/* Change tooltips */
	desc = g_strdup_printf ((undo) ? _("Undo: %s") : _("Redo: %s"),
			command ? command->description : _("the last action"));
	gtk_tooltips_set_tip (GTK_TOOLTIPS (button_tips), button, desc, NULL);
	g_free (desc);

	/* Set sensitivity on the button */
	gtk_widget_set_sensitive (button, command != NULL);
}

void
glade_app_update_ui_default (GladeApp *app)
{
	GList *list;
	
	for (list = app->priv->undo_list; list; list = list->next)
		if (list->data)
			glade_app_refresh_undo_redo_button (app, list->data, TRUE);

	for (list = app->priv->redo_list; list; list = list->next)
		if (list->data)
			glade_app_refresh_undo_redo_button (app, list->data, FALSE);
}

static void
glade_app_class_init (GladeAppClass * klass)
{
	GObjectClass *object_class;
	g_return_if_fail (klass != NULL);
	
	parent_class = g_type_class_peek_parent (klass);
	object_class = (GObjectClass *) klass;
	
	object_class->dispose   = glade_app_dispose;
	object_class->finalize  = glade_app_finalize;

	klass->update_ui_signal = glade_app_update_ui_default;
	klass->show_properties  = NULL;
	klass->hide_properties  = NULL;


	/**
	 * GladeApp::update-ui:
	 * @gladeapp: the #GladeApp which received the signal.
	 *
	 * Emitted when a project name changes or a cut/copy/paste/delete occurred.
	 */
	glade_app_signals[UPDATE_UI_SIGNAL] =
		g_signal_new ("update-ui",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GladeAppClass,
					       update_ui_signal),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
on_palette_button_clicked (GladePalette *palette, GladeApp *app)
{
	GladeWidgetClass *class;
	GladeWidget *widget;

	g_return_if_fail (GLADE_IS_PALETTE (palette));
	class = palette->current;

	/* class may be NULL if the selector was pressed */
	if (class && g_type_is_a (class->type, GTK_TYPE_WINDOW))
	{
		widget = glade_command_create (class, NULL, NULL, app->priv->active_project);
		
		/* if this is a top level widget set the accel group */
		if (app->priv->accel_group && GTK_IS_WINDOW (widget->object))
		{
			gtk_window_add_accel_group (GTK_WINDOW (widget->object),
						    app->priv->accel_group);
		}

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
 * Saves the GKeyFile to "g_get_user_config_dir()/GLADE_CONFIG_FILENAME"
 *
 * Return 0 on success.
 */
gint
glade_app_config_save (GladeApp *app)
{
	GIOChannel *channel;
	GIOStatus   status;
	gchar *data=NULL, *filename;
	const gchar *config_dir = g_get_user_config_dir ();
	GError *error = NULL;
	gsize size, written, bytes_written = 0;
	static gboolean error_shown = FALSE;

	/* If we had any errors; wait untill next session to retry.
	 */
	if (error_shown) return -1;
	
	/* Just in case... try to create the config directory */
	if (g_file_test (config_dir, G_FILE_TEST_IS_DIR) == FALSE)
	{
		if (g_file_test (config_dir, G_FILE_TEST_EXISTS))
		{
			/* Config dir exists but is not a directory */
			glade_util_ui_message
				(glade_default_app_get_window(),
				 GLADE_UI_ERROR,
				 _("Trying to save private data to %s directory "
				   "but it is a regular file.\n"
				   "No private data will be saved in this session"), 
				 config_dir);
				 error_shown = TRUE;
			return -1;
		}
		else if (g_mkdir (config_dir, S_IRWXU) != 0)
		{
			/* Doesnt exist; failed to create */
			glade_util_ui_message
				(glade_default_app_get_window(),
				 GLADE_UI_ERROR,
				 _("Failed to create directory %s to save private data.\n"
				   "No private data will be saved in this session"), config_dir);
				 error_shown = TRUE;
			return -1;
		}
	} 

	filename = g_build_filename (config_dir, GLADE_CONFIG_FILENAME, NULL);
	
	if ((channel = g_io_channel_new_file (filename, "w", &error)) != NULL)
	{
		if ((data = g_key_file_to_data (app->priv->config, &size, &error)) != NULL)
		{
			
			/* Implement loop here */
			while ((status = g_io_channel_write_chars
				(channel, 
				 data + bytes_written, /* Offset of write */
				 size - bytes_written, /* Size left to write */
				 &written, &error)) != G_IO_STATUS_ERROR &&
			       (bytes_written + written) < size)
				bytes_written += written;

			if (status == G_IO_STATUS_ERROR)
			{
				glade_util_ui_message
					(glade_default_app_get_window(),
					 GLADE_UI_ERROR,
					 _("Error writing private data to %s (%s).\n"
					   "No private data will be saved in this session"), 
					 filename, error->message);
				 error_shown = TRUE;
			}
			g_free (data);
		} 
		else
		{
			glade_util_ui_message
				(glade_default_app_get_window(),
				 GLADE_UI_ERROR,
				 _("Error serializing configuration data to save (%s).\n"
				   "No private data will be saved in this session"), 
				 error->message);
			error_shown = TRUE;
		}
		g_io_channel_shutdown(channel, TRUE, NULL);
		g_io_channel_unref (channel);
	}
	else
	{
		glade_util_ui_message
			(glade_default_app_get_window(),
			 GLADE_UI_ERROR,
			 _("Error opening %s to write private data (%s).\n"
			   "No private data will be saved in this session"), 
			 filename, error->message);
		error_shown = TRUE;
	}
	g_free (filename);
	
	if (error)
	{
		g_error_free (error);
		return -1;
	}
	return 0;
}

void
glade_app_set_transient_parent (GladeApp  *app, 
				GtkWindow *parent)
{
	GList        *projects, *objects;

	g_return_if_fail (GLADE_IS_APP (app));
	g_return_if_fail (GTK_IS_WINDOW (parent));

	app->priv->transient_parent = parent;

	/* Loop over all projects/widgets and set_transient_for the toplevels.
	 */
	for (projects = glade_app_get_projects(app); // projects
	     projects; projects = projects->next) 
		for (objects = GLADE_PROJECT (projects->data)->objects;  // widgets
		     objects; objects = objects->next)
			if (GTK_IS_WINDOW (objects->data))
				gtk_window_set_transient_for (GTK_WINDOW (objects->data), parent);
}

GtkWindow *
glade_app_get_transient_parent (GladeApp  *app)
{
	g_return_val_if_fail (GLADE_IS_APP (app), NULL);
	return app->priv->transient_parent;
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
	app->priv->accel_group = NULL;
	
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
	glade_editor_refresh (app->priv->editor);
	
	/* Create clipboard */
	app->priv->clipboard = glade_clipboard_new ();
	app->priv->clipboard->view = glade_clipboard_view_new (app->priv->clipboard);
	gtk_window_set_title (GTK_WINDOW (app->priv->clipboard->view), _("Clipboard"));
	g_signal_connect_after (G_OBJECT (app->priv->clipboard->view), "delete_event",
			  G_CALLBACK (clipboard_view_on_delete_cb),
			  app);

	/* Load the configuration file */
	app->priv->config = glade_app_config_load (app);
	
	/* Undo/Redo button list */
	app->priv->undo_list = app->priv->redo_list = NULL;
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
	g_signal_emit (G_OBJECT (app),
		       glade_app_signals[UPDATE_UI_SIGNAL], 0);
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

	/* Only update the editor if the selection has changed on
	 * the currently active project.
	 */
	if (app->priv->editor &&
	    (project == glade_app_get_active_project (app)))
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
	gboolean  loaded = FALSE;

	if (project_path == NULL) return FALSE;

	for (list = app->priv->projects; list; list = list->next)
	{
		GladeProject *cur_project = GLADE_PROJECT (list->data);

		if ((loaded = cur_project->path && 
		     (strcmp (cur_project->path, project_path) == 0)))
			break;
	}

	return loaded;
}


void
glade_app_show_properties (GladeApp* app, gboolean raise)
{
	g_return_if_fail (GLADE_IS_APP (app));

	if (GLADE_APP_GET_CLASS (app)->show_properties)
		GLADE_APP_GET_CLASS (app)->show_properties (app, raise);
	else 
		g_critical ("%s not implemented\n", G_GNUC_FUNCTION);
}

void
glade_app_hide_properties (GladeApp* app)
{
	g_return_if_fail (GLADE_IS_APP (app));

	if (GLADE_APP_GET_CLASS (app)->hide_properties)
		GLADE_APP_GET_CLASS (app)->hide_properties (app);
	else 
		g_critical ("%s not implemented\n", G_GNUC_FUNCTION);
}

void
glade_app_update_instance_count (GladeApp *app, GladeProject *project)
{
	GladeProject *prj;
	GList *l;
	gint temp, max = 0, i = 0, uncounted_projects = 0;
		
	if (project->instance > 0) return;

	for (l = app->priv->projects; l; l = l->next)
	{
		prj = l->data;

		if (prj != project &&
		    prj->name && !strcmp (prj->name, project->name))
		{
			i++;
			temp = MAX (prj->instance + 1, i);
			max  = MAX (temp, max);

			if (prj->instance < 1)
				uncounted_projects++;
		}
	}

	/* Dont reset the initially opened project */
	if (uncounted_projects > 1 || 
	    g_list_find (app->priv->projects, project) == NULL)
		project->instance = MAX (max, i);
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

	glade_app_update_instance_count (app, project);

	app->priv->projects = g_list_append (app->priv->projects, project);
	
	/* connect to the project signals so that the editor can be updated */
	g_signal_connect (G_OBJECT (project), "widget_name_changed",
			  G_CALLBACK (on_widget_name_changed_cb), app->priv->editor);
	g_signal_connect (G_OBJECT (project), "selection_changed",
			  G_CALLBACK (on_project_selection_changed_cb), app);

	/* add acceleration groups to every top level widget */
	if (app->priv->accel_group)
		glade_project_set_accel_group (project, app->priv->accel_group);
	
	glade_app_set_project (app, project);

	/* XXX I think the palette should detect this by itself */
	gtk_widget_set_sensitive (GTK_WIDGET (app->priv->palette), TRUE);
}

void
glade_app_remove_project (GladeApp *app, GladeProject *project)
{
	g_return_if_fail (GLADE_IS_APP (app));
	g_return_if_fail (GLADE_IS_PROJECT (project));

	app->priv->projects = g_list_remove (app->priv->projects, project);
	
	/* this is needed to prevent clearing the selection of a closed project 
	 */
	app->priv->active_project = NULL;
	
	/* If no more projects */
	if (app->priv->projects == NULL)
	{
		/* XXX I think the palette should detect this. */
		gtk_widget_set_sensitive (GTK_WIDGET (app->priv->palette), FALSE);
	} 
	else
		glade_app_set_project (app, g_list_last (app->priv->projects)->data);

	/* Its safe to just release the project as the project emits a
	 * "close" signal and everyone is responsable for cleaning up at
	 * that point.
	 */
	g_object_unref (project);
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

/**
 * glade_app_command_copy:
 * @app: A #GladeApp
 *
 * Copy the active project's selection (the new copies
 * will end up on the clipboard and will be set as
 * the clipboards selection).
 */
void
glade_app_command_copy (GladeApp *app)
{
	GList              *widgets = NULL, *list;
	GladeWidget        *widget;
	gboolean            failed = FALSE;

	g_return_if_fail (GLADE_IS_APP (app));
	if (app->priv->active_project == NULL) return;

	for (list = glade_default_app_get_selection ();
	     list && list->data; list = list->next)
	{
		widget  = glade_widget_get_from_gobject (GTK_WIDGET (list->data));
		widgets = g_list_prepend (widgets, widget);
		
		g_assert (widget);
		if (widget->internal)
		{
			glade_util_ui_message
				(glade_app_get_window(app),
				 GLADE_UI_INFO,
				 _("You cannot copy a widget "
				   "internal to a composite widget."));
			failed = TRUE;
			break;
		}
	}

	if (failed == FALSE && widgets != NULL)
	{
		glade_command_copy (widgets);
		glade_app_update_ui (app);
	}
	else if (widgets == NULL)
		glade_util_ui_message (glade_app_get_window(app),
				       GLADE_UI_INFO,
				       _("No widget selected."));

	if (widgets) g_list_free (widgets);
}

/**
 * glade_app_command_cut:
 * @app: A #GladeApp
 *
 * Cut the active project's selection (the cut objects
 * will end up on the clipboard and will be set as
 * the clipboards selection).
 */
void
glade_app_command_cut (GladeApp *app)
{
	GList              *widgets = NULL, *list;
	GladeWidget        *widget;
	gboolean            failed = FALSE;

	g_return_if_fail (GLADE_IS_APP (app));
	if (app->priv->active_project == NULL) return;

	for (list = glade_default_app_get_selection ();
	     list && list->data; list = list->next)
	{
		widget  = glade_widget_get_from_gobject (GTK_WIDGET (list->data));
		widgets = g_list_prepend (widgets, widget);
		
		g_assert (widget);
		if (widget->internal)
		{
			glade_util_ui_message
				(glade_app_get_window(app),
				 GLADE_UI_INFO,
				 _("You cannot cut a widget "
				   "internal to a composite widget."));
			failed = TRUE;
			break;
		}
	}

	if (failed == FALSE && widgets != NULL)
	{
		glade_command_cut (widgets);
		glade_app_update_ui (app);
	}
	else if (widgets == NULL)
		glade_util_ui_message (glade_app_get_window(app),
				       GLADE_UI_INFO,
				       _("No widget selected."));

	if (widgets) g_list_free (widgets);
}

/**
 * glade_app_command_paste:
 * @app: A #GladeApp
 *
 * Paste the clipboard selection to the active project's 
 * selection (the project must have only one object selected).
 */
void
glade_app_command_paste (GladeApp *app)
{
	GladeClipboard     *clipboard;
	GList              *list;
	GladeWidget        *widget = NULL;
	gint                gtkcontainer_relations = 0;
	GladePlaceholder   *placeholder;
	GladeWidget        *parent;


	g_return_if_fail (GLADE_IS_APP (app));
	if (app->priv->active_project == NULL) return;

	list      = glade_project_selection_get (app->priv->active_project);
	clipboard = glade_app_get_clipboard (app);

	/* If there is a selection, dont use a placeholder to paste into */
	placeholder = list ? NULL : glade_util_selected_placeholder ();

	/* If there is a selection, paste in to the selected widget, otherwise
	 * paste into the placeholder's parent.
	 */
	parent      = list ? glade_widget_get_from_gobject (list->data) :
		(placeholder ? glade_placeholder_get_parent (placeholder) : NULL);


	/* Check if selection is good */
	if ((list = glade_default_app_get_selection ()) != NULL)
	{
		if (placeholder == NULL &&
		    g_list_length (list) != 1)
		{
			glade_util_ui_message (glade_app_get_window(app),
					       GLADE_UI_INFO,
					       _("Unable to paste to multiple widgets"));
			return;
		}
	}
	
	/* Check if we have anything to paste */
	if (g_list_length (clipboard->selection) == 0)
	{
		glade_util_ui_message (glade_app_get_window (app), GLADE_UI_INFO,
				    _("No widget selected on the clipboard"));
		return;
	}
	
	/* Check that we have compatible heirarchies */
	for (list = clipboard->selection; 
	     list && list->data; list = list->next)
	{
		widget = list->data;

		if (GTK_WIDGET_TOPLEVEL (widget->object) == FALSE && parent)
		{
			/* Ensure a paste is supported
			 */
			if (glade_util_widget_pastable (widget, parent) == FALSE)
			{
				glade_util_ui_message (glade_default_app_get_window (),
						       GLADE_UI_ERROR, 
						       _("Unable to paste widget %s to parent %s"),
						       widget->name, parent->name);
				return;
			}

			/* Count gtk container relations
			 */
			if (glade_util_gtkcontainer_relation (parent, widget))
				gtkcontainer_relations++;
		}
	}

	g_assert (widget);

	/* Check that GladeFixedManager will cope */
	if (GTK_WIDGET_TOPLEVEL (widget->object) == FALSE &&
	    parent && parent->manager != NULL &&
	    gtkcontainer_relations != 1) 
	{
		glade_util_ui_message (glade_default_app_get_window (), 
				       GLADE_UI_INFO,
				       _("Only one widget can be pasted at a "
					 "time to this container"));
		return;
	}

	/* Check that enough placeholders are available */
	if (parent && parent->manager == NULL &&
	    glade_util_count_placeholders (parent) < gtkcontainer_relations)
	{
		glade_util_ui_message (glade_default_app_get_window (), 
				       GLADE_UI_INFO,
				       _("Insufficient amount of placeholders in "
					 "target container"));
		return;
	}

	glade_command_paste (clipboard->selection, parent, placeholder);
	glade_app_update_ui (app);
}


/**
 * glade_app_command_delete:
 * @app: A #GladeApp
 *
 * Delete the active project's selection.
 */
void
glade_app_command_delete (GladeApp *app)
{
	GList              *widgets = NULL, *list;
	GladeWidget        *widget;
	gboolean            failed = FALSE;

	g_return_if_fail (GLADE_IS_APP (app));
	if (app->priv->active_project == NULL) return;

	for (list = glade_default_app_get_selection ();
	     list && list->data; list = list->next)
	{
		widget  = glade_widget_get_from_gobject (GTK_WIDGET (list->data));
		widgets = g_list_prepend (widgets, widget);
		
		g_assert (widget);
		if (widget->internal)
		{
			glade_util_ui_message
				(glade_app_get_window(app),
				 GLADE_UI_INFO,
				 _("You cannot delete a widget "
				   "internal to a composite widget."));
			failed = TRUE;
			break;
		}
	}

	if (failed == FALSE && widgets != NULL)
	{
		glade_command_delete (widgets);
		glade_app_update_ui (app);
	}
	else if (widgets == NULL)
		glade_util_ui_message (glade_app_get_window(app),
				       GLADE_UI_INFO,
				       _("No widget selected."));

	if (widgets) g_list_free (widgets);
}

/**
 * glade_app_command_delete_clipboard:
 * @app: A #GladeApp
 *
 * Delete the clipboard's selection.
 */
void
glade_app_command_delete_clipboard (GladeApp *app)
{
	GladeClipboard     *clipboard;
	GladeWidget        *gwidget;
	GList              *list;

	g_return_if_fail (GLADE_IS_APP (app));

	clipboard = glade_app_get_clipboard (app);

	if (clipboard->selection == NULL)
		glade_util_ui_message (glade_app_get_window (app), GLADE_UI_INFO,
				    _("No widget selected on the clipboard"));

	for (list = clipboard->selection; list; list = list->next)
	{
		gwidget = list->data;
		if (gwidget->internal)
		{
			glade_util_ui_message
				(glade_default_app_get_window(),
				 GLADE_UI_INFO,
				 _("You cannot delete a widget "
				   "internal to a composite widget."));
			return;
		}
	}

	glade_command_delete (clipboard->selection);
	glade_app_update_ui (app);
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

/*
 * glade_app_set_accel_group:
 *
 * Sets @accel_group to @app.
 * The acceleration group will be atached to every toplevel widget in this application.
 */
void
glade_app_set_accel_group (GladeApp *app, GtkAccelGroup *accel_group)
{
	GList *l;
	GladeProject *project;
	g_return_if_fail(GLADE_IS_APP(app) &&
			 GTK_IS_ACCEL_GROUP (accel_group));

	for (l = app->priv->projects; l; l = l->next)
	{
		project = l->data;
		glade_project_set_accel_group (project, accel_group);
	}
	
	app->priv->accel_group = accel_group;
}

static gboolean
glade_app_undo_button_destroyed (GtkWidget *button, GladeApp *app)
{
	app->priv->undo_list = g_list_remove (app->priv->undo_list, button);
	return FALSE;
}

static gboolean
glade_app_redo_button_destroyed (GtkWidget *button, GladeApp *app)
{
	app->priv->redo_list = g_list_remove (app->priv->redo_list, button);
	return FALSE;
}

static GtkWidget *
glade_app_undo_redo_button_new (GladeApp *app, gboolean undo)
{
	GtkWidget *button;
	
	button = gtk_button_new_from_stock ((undo) ? 
					    GTK_STOCK_UNDO : 
					    GTK_STOCK_REDO);
	
	g_signal_connect_swapped (button, "clicked",
				  (undo) ? G_CALLBACK (glade_app_command_undo) : 
					   G_CALLBACK (glade_app_command_redo),
				  app);
	
	if (undo)
	{
		app->priv->undo_list = g_list_prepend (app->priv->undo_list, button);
		g_signal_connect (button, "destroy",
				  G_CALLBACK (glade_app_undo_button_destroyed),
				  app);
	}
	else
	{
		app->priv->redo_list = g_list_prepend (app->priv->redo_list, button);
		g_signal_connect (button, "destroy",
				  G_CALLBACK (glade_app_redo_button_destroyed),
				  app);
	}
	
	glade_app_refresh_undo_redo_button (app, button, undo);
	
	return button;
}

/*
 * glade_app_undo_button_new:
 *
 * Creates a new GtkButton undo widget.
 * The button will be automatically updated with @app's undo stack.
 */
GtkWidget *
glade_app_undo_button_new (GladeApp *app)
{
	return glade_app_undo_redo_button_new (app, TRUE);
}

/*
 * glade_app_redo_button_new:
 *
 * Creates a new GtkButton redo widget.
 * The button will be automatically updated with @app's redo stack.
 */
GtkWidget *
glade_app_redo_button_new (GladeApp *app)
{
	return glade_app_undo_redo_button_new (app, FALSE);
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

void
glade_default_app_show_properties (gboolean raise)
{
	g_return_if_fail (glade_default_app != NULL);
	return glade_app_show_properties (glade_default_app, raise);
}

void
glade_default_app_hide_properties (void)
{
	g_return_if_fail (glade_default_app != NULL);
	return glade_app_hide_properties (glade_default_app);
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

void
glade_default_app_set_transient_parent (GtkWindow *parent)
{
	g_return_if_fail (glade_default_app != NULL);
	glade_app_set_transient_parent (glade_default_app, parent);
}

GtkWindow *
glade_default_app_get_transient_parent (void)
{
	g_return_val_if_fail (glade_default_app != NULL, NULL);
	return glade_app_get_transient_parent (glade_default_app);
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

	glade_util_clear_selection ();
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

GtkWidget *
glade_default_app_undo_button_new (void)
{
	g_return_val_if_fail (glade_default_app != NULL, NULL);
	return glade_app_undo_redo_button_new (glade_default_app, TRUE);
}

GtkWidget *
glade_default_app_redo_button_new (void)
{
	g_return_val_if_fail (glade_default_app != NULL, NULL);
	return glade_app_undo_redo_button_new (glade_default_app, FALSE);
}

void
glade_default_app_command_cut (void)
{
	g_return_if_fail (glade_default_app != NULL);
	return glade_app_command_cut (glade_default_app);
}

void
glade_default_app_command_copy (void)
{
	g_return_if_fail (glade_default_app != NULL);
	return glade_app_command_copy (glade_default_app);
}

void
glade_default_app_command_paste (void)
{
	g_return_if_fail (glade_default_app != NULL);
	return glade_app_command_paste (glade_default_app);
}

void
glade_default_app_command_delete (void)
{
	g_return_if_fail (glade_default_app != NULL);
	return glade_app_command_delete (glade_default_app);
}

void
glade_default_app_command_delete_clipboard (void)
{
	g_return_if_fail (glade_default_app != NULL);
	return glade_app_command_delete_clipboard (glade_default_app);
}
