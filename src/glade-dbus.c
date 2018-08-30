/*
 * glade-dbus.c
 *
 * Copyright (C) 2018 Juan Pablo Ugarte
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#include "glade-window.h"
#include "glade-dbus.h"

typedef enum
{
  GLADE_DBUS_OK,
  GLADE_DBUS_NO_PROJECT
} GladeDBusError;

#define GLADE_DBUS_ERROR glade_dbus_error_quark()
static GQuark glade_dbus_error_quark (void);
G_DEFINE_QUARK (glade-dbus-error-quark, glade_dbus_error)

static GDBusInterfaceInfo *iface_info = NULL;

static void
add_object (GtkApplication *app,
            const gchar    *project_path,
            const gchar    *object_type,
            const gchar    *object_id,
            gboolean        is_template)
{
  GladeWidgetAdaptor *adaptor;
  GladeProject *project;
  GladeWidget *widget;

  if (!(project = glade_app_get_project_by_path (project_path)))
    return;

  if (!(adaptor = glade_widget_adaptor_get_by_name (object_type)))
    return;

  widget = glade_widget_adaptor_create_widget (adaptor, FALSE, NULL);
  if (!widget)
    return;

  glade_project_add_object (project, glade_widget_get_object (widget));

  if (is_template)
    glade_project_set_template (project, widget);

  glade_project_save (project, glade_project_get_path (project), NULL);
}

static void
glade_dbus_method_call (GDBusConnection       *connection,
                        const gchar           *sender,
                        const gchar           *object_path,
                        const gchar           *interface_name,
                        const gchar           *method_name,
                        GVariant              *parameters,
                        GDBusMethodInvocation *invocation,
                        gpointer               user_data)
{
  GtkApplication *app = user_data;

  /* gdbus call --session --dest org.gnome.Glade --object-path /org/gnome/Glade --method \
     org.gnome.Glade.AddObject '/home/xjuan/test.glade' GtkWindow MyWindow true
   */
  if (g_strcmp0 (method_name, "AddObject") == 0)
    {
      gchar *project_path, *object_type, *object_id;
      gboolean is_template;

      g_variant_get (parameters, "(sssb)",
                     &project_path,
                     &object_type,
                     &object_id,
                     &is_template);

      add_object (app, project_path, object_type, object_id, is_template);

      g_free (project_path);
      g_free (object_type);
      g_free (object_id);
    }

  g_dbus_method_invocation_return_value (invocation, NULL);
}

static GVariant *
glade_dbus_get_property (GDBusConnection *connection,
                         const gchar     *sender,
                         const gchar     *object_path,
                         const gchar     *interface_name,
                         const gchar     *property_name,
                         GError         **error,
                         gpointer         user_data)
{
  GtkApplication *app = user_data;

    /* gdbus call --session --dest org.gnome.Glade --object-path /org/gnome/Glade --method \
       org.freedesktop.DBus.Properties.Set 'org.gnome.Glade' 'Project' "<'/home/xjuan/test.glade'>"
     */
  if (g_strcmp0 (property_name, "Project") == 0)
    {
      GladeWindow *window = GLADE_WINDOW (gtk_application_get_active_window (app));
      GladeProject *project;

      if ((project = glade_window_get_active_project (window)))
        return g_variant_new_string (glade_project_get_path (project));

      *error = g_error_new_literal (GLADE_DBUS_ERROR,
                                    GLADE_DBUS_NO_PROJECT,
                                    "Project not found");
    }

  return NULL;
}

static gboolean
set_project (GtkApplication *app, const gchar *path)
{
  GladeWindow *window = GLADE_WINDOW (gtk_application_get_active_window (app));

  if (glade_app_is_project_loaded (path) ||
      g_file_test (path, G_FILE_TEST_EXISTS))
    glade_window_open_project (window, path);
  else
    {
      GladeProject *project = glade_window_new_project (window);
      glade_project_save (project, path, NULL);
    }

  return TRUE;
}

static gboolean
glade_dbus_set_property (GDBusConnection *connection,
                         const gchar     *sender,
                         const gchar     *object_path,
                         const gchar     *interface_name,
                         const gchar     *property_name,
                         GVariant        *value,
                         GError         **error,
                         gpointer         user_data)
{
  GtkApplication *app = user_data;

  if (strcmp (property_name, "Project") == 0)
    return set_project (app, g_variant_get_string (value, NULL));

  return FALSE;
}

#define DBUS_IFACE "org.gnome.Glade"

void
glade_dbus_register (GApplication *app)
{
  const static GDBusInterfaceVTable vtable = {
    glade_dbus_method_call,
    glade_dbus_get_property,
    glade_dbus_set_property
  };
  GDBusNodeInfo *info;
  GError *error = NULL;
  GBytes *xml;

  if (!(xml = g_resources_lookup_data ("/org/gnome/glade/glade-dbus.xml", 0, NULL)))
    return;

  if (!(info = g_dbus_node_info_new_for_xml (g_bytes_get_data (xml, NULL), &error)))
    {
      g_debug ("%s %s", __func__, error->message);
      return;
    }

  iface_info = g_dbus_node_info_lookup_interface (info, DBUS_IFACE);
  g_assert (iface_info != NULL);
  g_dbus_interface_info_ref (iface_info);
  g_dbus_node_info_unref (info);

  g_dbus_connection_register_object (g_application_get_dbus_connection (app),
                                     g_application_get_dbus_object_path (app),
                                     iface_info,
                                     &vtable,
                                     app,
                                     NULL,
                                     &error);
  if (error)
    g_debug ("%s %s", __func__, error->message);
}

static inline void
glade_dbus_emit (GApplication *app,
                 GError      **error,
                 const gchar  *signal,
                 const gchar  *format,
                 ...)
{
  va_list params;
  va_start (params, format);
  g_dbus_connection_emit_signal (g_application_get_dbus_connection (app),
                                 NULL,
                                 g_application_get_dbus_object_path (app),
                                 DBUS_IFACE,
                                 signal,
                                 g_variant_new_va (format, NULL, &params),
                                 error);
  va_end (params);
}

static const gchar *
widget_get_class (GladeWidget *widget)
{
  GObject *object = glade_widget_get_object (widget);
  return object ? G_OBJECT_TYPE_NAME (object) : NULL;
}

static inline void
glade_dbus_emit_plain (GApplication *app,
                       const gchar  *dbus_signal,
                       GladeWidget  *widget,
                       GladeSignal  *signal,
                       GError      **error)
{
  const gchar *path, *widget_class, *widget_name, *handler, *signal_name;
  GladeProject *project;

  if ((project = glade_widget_get_project (widget)) &&
      (path = glade_project_get_path (project)) &&
      (widget_class = widget_get_class (widget)) &&
      (widget_name = glade_widget_get_name (widget)) &&
      (handler = glade_signal_get_handler (signal)) &&
      (signal_name = glade_signal_get_name (signal)))
    glade_dbus_emit (app,
                     error,
                     dbus_signal,
                     "(sssss)",
                     path,
                     widget_class,
                     widget_name,
                     handler,
                     signal_name);
}

void
glade_dbus_emit_handler_added (GApplication *app,
                               GladeWidget  *widget,
                               GladeSignal  *signal,
                               GError      **error)
{
  const gchar *path, *widget_class, *widget_name, *handler, *signal_name, *userdata;
  GladeProject *project;

  userdata = glade_signal_get_userdata (signal);

  if ((project = glade_widget_get_project (widget)) &&
      (path = glade_project_get_path (project)) &&
      (widget_class = widget_get_class (widget)) &&
      (widget_name = glade_widget_get_name (widget)) &&
      (handler = glade_signal_get_handler (signal)) &&
      (signal_name = glade_signal_get_name (signal)))
    glade_dbus_emit (app,
                     error,
                     "HandlerAdded",
                     "(ssssssb)",
                     path,
                     widget_class,
                     widget_name,
                     handler,
                     signal_name,
                     userdata ? userdata : "",
                     glade_signal_get_swapped (signal));
}

void
glade_dbus_emit_handler_removed (GApplication *app,
                                 GladeWidget  *widget,
                                 GladeSignal  *signal,
                                 GError      **error)
{
  glade_dbus_emit_plain (app, "HandlerRemoved", widget, signal, error);
}

void
glade_dbus_emit_handler_activated (GApplication *app,
                                   GladeWidget  *widget,
                                   GladeSignal  *signal,
                                   GError      **error)
{
  glade_dbus_emit_plain (app, "HandlerActivated", widget, signal, error);
}

void
glade_dbus_emit_handler_changed (GApplication *app,
                                 GladeWidget  *widget,
                                 GladeSignal  *old_signal,
                                 GladeSignal  *new_signal,
                                 GError      **error)
{
  const gchar *path, *widget_class, *widget_name, *old_handler, *new_handler, *signal_name, *userdata;
  GladeProject *project;

  userdata = glade_signal_get_userdata (new_signal);

  if ((project = glade_widget_get_project (widget)) &&
      (path = glade_project_get_path (project)) &&
      (widget_class = widget_get_class (widget)) &&
      (widget_name = glade_widget_get_name (widget)) &&
      (old_handler = glade_signal_get_handler (old_signal)) &&
      (new_handler = glade_signal_get_handler (new_signal)) &&
      (signal_name = glade_signal_get_name (new_signal)))
    glade_dbus_emit (app,
                     error,
                     "HandlerChanged",
                     "(sssssssb)",
                     path,
                     widget_class,
                     widget_name,
                     old_handler,
                     new_handler,
                     signal_name,
                     userdata ? userdata : "",
                     glade_signal_get_swapped (new_signal));
}

