/*
 * Copyright (C) 2014 Juan Pablo Ugarte.
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
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#include "config.h"
#include "glade-registration.h"
#include "glade-http.h"
#include "glade-logo.h"
#include <gladeui/glade.h>
#include <glib/gi18n.h>

struct _GladeRegistrationPrivate
{
  GtkWidget    *infobar;
  GtkWidget    *net_spinner;
  GtkLabel     *infobar_label;
  GtkLabel     *status_label;
  GladeHTTP    *http;
  GCancellable *cancellable;

  /* Form widgets */

  GtkWidget *name;
  GtkWidget *email;
  GtkWidget *country_id;
  GtkWidget *city;
  GtkWidget *contact_type;
  GtkWidget *contact_name;
  GtkWidget *contact_website;
  GtkWidget *subscribe;

  GtkWidget *validation_token;

  GtkWidget *experience;
  GtkWidget *experience_unit;
  GtkWidget *experience_not_programmer;
  GtkWidget *lang_c;
  GtkWidget *lang_cpp;
  GtkWidget *lang_csharp;
  GtkWidget *lang_java;
  GtkWidget *lang_python;
  GtkWidget *lang_javascript;
  GtkWidget *lang_vala;
  GtkWidget *lang_perl;
  GtkWidget *lang_other;
  GtkWidget *start_using;
  GtkWidget *start_using_unit;
  GtkWidget *version;
  GtkWidget *version_other;
  GtkWidget *os;
  GtkWidget *os_linux;
  GtkWidget *os_bsd;
  GtkWidget *os_windows;
  GtkWidget *os_osx;
  GtkWidget *os_solaris;
  GtkWidget *os_other;
  GtkWidget *freq;
  GtkWidget *user_level;
  GtkWidget *soft_free;
  GtkWidget *soft_open;
  GtkWidget *soft_commercial;
  GtkWidget *soft_none;
  GtkWidget *field_academic;
  GtkWidget *field_accounting;
  GtkWidget *field_desktop;
  GtkWidget *field_educational;
  GtkWidget *field_embedded;
  GtkWidget *field_medical;
  GtkWidget *field_industrial;
  GtkWidget *field_scientific;
  GtkWidget *field_other;
  GtkWidget *improvement;
  GtkWidget *problem;
  GtkWidget *problem_other;
  GtkWidget *bug;
  GtkWidget *bugzilla;
  GtkWidget *contributing;
  GtkWidget *contributing_whynot;
  GtkWidget *comments;
};

G_DEFINE_TYPE_WITH_PRIVATE (GladeRegistration, glade_registration, GTK_TYPE_DIALOG);


static void
string_append_input_key_value_tuple (GString *string,
                                     const gchar *name,
                                     GtkWidget *input)
{
  if (string->len)
    g_string_append_c (string, '&');

  g_string_append (string, name);
  g_string_append_c (string, '=');
  
  if (GTK_IS_ENTRY (input))
    g_string_append_uri_escaped (string, gtk_entry_get_text (GTK_ENTRY (input)), NULL, FALSE);
  else if (GTK_IS_TEXT_VIEW (input))
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (input));      
      GtkTextIter start, end;
      gchar *text;
      
      gtk_text_buffer_get_bounds (buffer, &start, &end);
      text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
      g_string_append_uri_escaped (string, text ? text : "", NULL, FALSE);
      g_free (text);
    }
  else if (GTK_IS_COMBO_BOX (input))
    g_string_append_uri_escaped (string, gtk_combo_box_get_active_id (GTK_COMBO_BOX (input)), NULL, FALSE);
  else if (GTK_IS_RADIO_BUTTON (input))
    {
      GSList *group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (input));

      for (; group; group = g_slist_next (group))
        {
          if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (group->data)))            
            g_string_append (string, gtk_widget_get_name (group->data));
        }
    }
  else if (GTK_IS_TOGGLE_BUTTON (input))
    g_string_append_printf (string, "%d", gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (input)) ? 1 : 0);
}

static void
glade_registration_show_message (GladeRegistration *registration,
                                 GtkMessageType     type,
                                 const gchar       *message)
{
  gtk_info_bar_set_message_type (GTK_INFO_BAR (registration->priv->infobar), type);
  gtk_label_set_text (registration->priv->infobar_label, message ? message : "");

  /* Only show the infobar if the dialog is visible */
  if (gtk_widget_is_visible (GTK_WIDGET (registration)))
    gtk_widget_show (registration->priv->infobar);
}

static void 
on_http_status (GladeHTTP         *http,
                GladeHTTPStatus    status,
                GError            *error,
                GladeRegistration *registration)
{
  gchar *text = NULL;
  
  switch (status)
    {
      case GLADE_HTTP_READY:
      break;
      case GLADE_HTTP_CONNECTING:
        text = g_strdup_printf (_("Connecting to %s"), glade_http_get_host (http));
      break;
      case GLADE_HTTP_SENDING:
        text = g_strdup_printf (_("Sending data to %s"), glade_http_get_host (http));
      break;
      case GLADE_HTTP_WAITING:
        text = g_strdup_printf (_("Waiting for %s"), glade_http_get_host (http));
      break;
      case GLADE_HTTP_RECEIVING:
        text = g_strdup_printf (_("Receiving data from %s"), glade_http_get_host (http));
      break;
      case GLADE_HTTP_ERROR:
        glade_registration_show_message (registration, GTK_MESSAGE_WARNING, error->message);
      break;
    }

  gtk_label_set_text (registration->priv->status_label, text ? text : "");
  gtk_widget_set_visible (registration->priv->net_spinner, text != NULL);
  g_free (text);
}

#define append_input_tuple(s,i) string_append_input_key_value_tuple (s, #i, priv->i)

static void 
on_http_request_done (GladeHTTP         *http,
                      gint               code,
                      const gchar      **headers,
                      const gchar      **values,
                      const gchar       *response,
                      GladeRegistration *registration)
{  
  switch (code)
    {
      case 100:
        /* Ignore Continue HTTP response */
      break;
      case 200:
        {
          const gchar *status = NULL, *message = _("Internal server error");
          gint i;

          for (i = 0; headers[i]; i++)
            {
              if (g_strcmp0 (headers[i], "X-Glade-Status") == 0)
                status = values[i];
              else if (g_strcmp0 (headers[i], "X-Glade-Message") == 0)
                message = values[i];
            }

          if (status == NULL)
            {
              glade_registration_show_message (registration, GTK_MESSAGE_WARNING, message);
              return;
            }
          
          if (g_strcmp0 (status, "ok") == 0)
            {
              gtk_widget_hide (GTK_WIDGET (registration));
              glade_util_ui_message (glade_app_get_window (), GLADE_UI_INFO, NULL, "%s", message);
            }
          else if (g_strcmp0 (status, "error") == 0)
            glade_registration_show_message (registration, GTK_MESSAGE_INFO, message);
        }
      break;
      default:
        glade_registration_show_message (registration, GTK_MESSAGE_INFO, response);
      break;
    }
}

#ifdef GDK_WINDOWING_X11
#include "gdk/gdkx.h"
#endif

#ifdef GDK_WINDOWING_QUARTZ
#include "gdk/gdkquartz.h"
#endif

#ifdef GDK_WINDOWING_WIN32
#include "gdk/gdkwin32.h"
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include "gdk/gdkwayland.h"
#endif

#ifdef GDK_WINDOWING_BROADWAY
#include "gdk/gdkbroadway.h"
#endif

static const gchar *
get_gdk_backend (GtkWidget *widget)
{
  GdkDisplay *display = gtk_widget_get_display (widget);

#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (display))
    return "X11";
  else
#endif

#ifdef GDK_WINDOWING_QUARTZ
  if (GDK_IS_QUARTZ_DISPLAY (display))
    return "Quartz";
  else
#endif

#ifdef GDK_WINDOWING_WIN32
  if (GDK_IS_WIN32_DISPLAY (display))
    return "Win32";
  else
#endif

#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (display))
    return "Wayland";
  else
#endif

#ifdef GDK_WINDOWING_BROADWAY
  if (GDK_IS_BROADWAY_DISPLAY (display))
    return "Broadway";
  else
#endif
  {
    return "Unknown";
  }
}

static void 
glade_registration_clear_cancellable (GladeRegistrationPrivate *priv)
{
  if (priv->cancellable)
    {
      g_cancellable_cancel (priv->cancellable);
      g_clear_object (&priv->cancellable);
    }
}

static void 
on_registration_dialog_response (GtkDialog *dialog, gint response_id)
{
  GladeRegistrationPrivate *priv = GLADE_REGISTRATION (dialog)->priv;
  GString *post;

  gtk_widget_hide (priv->infobar);
  
  if (response_id != GTK_RESPONSE_APPLY)
    {
      gtk_widget_hide (GTK_WIDGET (dialog));
      glade_registration_clear_cancellable (priv);
      return;
    }

  glade_registration_clear_cancellable (priv);
  priv->cancellable = g_cancellable_new ();
  
  post = g_string_new ("");

  append_input_tuple (post, name);
  append_input_tuple (post, email);
  append_input_tuple (post, country_id);
  append_input_tuple (post, city);
  append_input_tuple (post, contact_type);
  append_input_tuple (post, contact_name);
  append_input_tuple (post, contact_website);

  append_input_tuple (post, validation_token);
  
  append_input_tuple (post, experience);
  append_input_tuple (post, experience_unit);
  append_input_tuple (post, experience_not_programmer);
  append_input_tuple (post, lang_c);
  append_input_tuple (post, lang_cpp);
  append_input_tuple (post, lang_csharp);
  append_input_tuple (post, lang_java);
  append_input_tuple (post, lang_python);
  append_input_tuple (post, lang_javascript);
  append_input_tuple (post, lang_vala);
  append_input_tuple (post, lang_perl);
  append_input_tuple (post, lang_other);
  append_input_tuple (post, start_using);
  append_input_tuple (post, start_using_unit);
  append_input_tuple (post, version);
  append_input_tuple (post, version_other);
  append_input_tuple (post, os);
  string_append_input_key_value_tuple (post, "linux", priv->os_linux);
  string_append_input_key_value_tuple (post, "bsd", priv->os_bsd);
  string_append_input_key_value_tuple (post, "windows", priv->os_windows);
  string_append_input_key_value_tuple (post, "osx", priv->os_osx);
  string_append_input_key_value_tuple (post, "solaris", priv->os_solaris);
  append_input_tuple (post, os_other);
  append_input_tuple (post, freq);
  append_input_tuple (post, user_level);
  append_input_tuple (post, soft_free);
  append_input_tuple (post, soft_open);
  append_input_tuple (post, soft_commercial);
  append_input_tuple (post, soft_none);
  append_input_tuple (post, field_academic);
  append_input_tuple (post, field_accounting);
  append_input_tuple (post, field_desktop);
  append_input_tuple (post, field_educational);
  append_input_tuple (post, field_embedded);
  append_input_tuple (post, field_medical);
  append_input_tuple (post, field_industrial);
  append_input_tuple (post, field_scientific);
  append_input_tuple (post, field_other);
  append_input_tuple (post, improvement);
  append_input_tuple (post, problem);
  append_input_tuple (post, problem_other);
  append_input_tuple (post, bug);
  append_input_tuple (post, bugzilla);
  append_input_tuple (post, contributing);
  append_input_tuple (post, contributing_whynot);
  append_input_tuple (post, comments);
  
  glade_http_request_send_async (priv->http,  priv->cancellable,
                                 "POST /~jpu/glade/registration_master.php HTTP/1.1\r\n"
                                 "Host: %s\r\n"
                                 "User-Agent: Glade/"PACKAGE_VERSION" (%s; Gtk+ %d.%d.%d; glib %d.%d.%d)\r\n"
                                 "Connection: close\r\n"
                                 "Accept: text/plain\r\n"
                                 "Content-Type: application/x-www-form-urlencoded\r\n"
                                 "Content-Length: %d\r\n"
                                 "\r\n"
                                 "%s",
                                 glade_http_get_host (priv->http),
                                 get_gdk_backend (GTK_WIDGET (dialog)),
                                 GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION,
                                 glib_major_version, glib_minor_version, glib_micro_version,
                                 post->len,
                                 post->str);
  
  g_string_free (post, TRUE);
}

static void
on_privacy_button_clicked (GtkButton *button, GtkScrolledWindow *swindow)
{
  GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment (swindow);
  gtk_adjustment_set_value (adj, gtk_adjustment_get_upper (adj));
}

static void
toggle_button_set_visible_on_toggle (GtkToggleButton *button, GtkWidget *widget)
{
  gtk_widget_set_visible (widget, gtk_toggle_button_get_active (button));
}

static void
toggle_button_set_sensitive_on_toggle (GtkToggleButton *button, GtkWidget *widget)
{
  gtk_widget_set_sensitive (widget, gtk_toggle_button_get_active (button));
}

static void 
glade_registration_set_css_provider_forall (GtkWidget *widget, gpointer data)
{
  gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
                                  GTK_STYLE_PROVIDER (data),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  
  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget), glade_registration_set_css_provider_forall, data);
}

static gboolean
on_viewport_draw (GtkWidget *widget, cairo_t *cr)
{
  GdkWindow *window = gtk_viewport_get_bin_window (GTK_VIEWPORT (widget));

  if (gtk_cairo_should_draw_window (cr, window))
    { 
      GtkAllocation alloc;
      gdouble scale;

      gtk_widget_get_allocation (widget, &alloc);
      
      scale = MIN (alloc.width/GLADE_LOGO_WIDTH, alloc.height/GLADE_LOGO_HEIGHT) - .1;
      
      cairo_save (cr);

      cairo_set_source_rgba (cr, 0, 0, 0, .04);
      cairo_scale (cr, scale, scale);
      cairo_translate (cr, (alloc.width / scale) - GLADE_LOGO_WIDTH*.95,
                       (alloc.height / scale) - GLADE_LOGO_HEIGHT);
      cairo_append_path (cr, &glade_logo_path);
      cairo_fill (cr);

      cairo_restore (cr);
    }

  gtk_container_propagate_draw (GTK_CONTAINER (widget),
                                gtk_bin_get_child (GTK_BIN (widget)),
                                cr);
  return TRUE;
}

static void
glade_registration_init (GladeRegistration *registration)
{
  GladeRegistrationPrivate *priv = glade_registration_get_instance_private (registration);
  GtkCssProvider *css_provider;
  GFile *file;

  registration->priv = priv;

  /* HTTPS default port is 443 */
  priv->http = glade_http_new ("people.gnome.org", 443, TRUE);

  g_signal_connect_object (priv->http, "request-done",
                           G_CALLBACK (on_http_request_done),
                           registration, 0);
  g_signal_connect_object (priv->http, "status",
                           G_CALLBACK (on_http_status),
                           registration, 0);
  
  gtk_widget_init_template (GTK_WIDGET (registration));

  /* Apply Custom CSS */
  css_provider = gtk_css_provider_new ();
  file = g_file_new_for_uri ("resource:///org/gnome/glade/glade-registration.css");
  if (gtk_css_provider_load_from_file (css_provider, file, NULL))
    glade_registration_set_css_provider_forall (GTK_WIDGET (registration), css_provider);
  g_object_unref (file);

  if (GTK_IS_COMBO_BOX_TEXT (priv->version_other))
    {
      GtkComboBoxText *combo = GTK_COMBO_BOX_TEXT (priv->version_other);
      gchar id[16], text[18];
      gint minor;

      for (minor = 0; minor <= GLADE_MINOR_VERSION; minor+=2)
        {
          g_snprintf (id, 16, "%d", minor);
          g_snprintf (text, 18, "3.%d", minor);
          gtk_combo_box_text_append (combo, id, text);
        }

      gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
    }
}

static void
glade_registration_finalize (GObject *object)
{
  GladeRegistrationPrivate *priv = GLADE_REGISTRATION (object)->priv;

  g_clear_object (&priv->http);
  g_clear_object (&priv->cancellable);

  G_OBJECT_CLASS (glade_registration_parent_class)->finalize (object);
}

static void
glade_registration_class_init (GladeRegistrationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/glade/glade-registration.glade");

  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, infobar);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, net_spinner);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, infobar_label);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, status_label);
  
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, name);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, email);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, country_id);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, city);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, contact_type);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, contact_name);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, contact_website);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, subscribe);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, validation_token);

  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, experience);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, experience_unit);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, experience_not_programmer);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, lang_c);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, lang_cpp);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, lang_csharp);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, lang_java);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, lang_python);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, lang_javascript);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, lang_vala);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, lang_perl);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, lang_other);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, start_using);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, start_using_unit);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, version);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, version_other);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, os);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, os_linux);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, os_bsd);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, os_windows);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, os_osx);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, os_solaris);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, os_other);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, freq);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, user_level);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, soft_free);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, soft_open);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, soft_commercial);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, soft_none);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, field_academic);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, field_accounting);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, field_desktop);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, field_educational);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, field_embedded);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, field_medical);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, field_industrial);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, field_scientific);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, field_other);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, improvement);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, problem);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, problem_other);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, bug);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, bugzilla);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, contributing);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, contributing_whynot);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, comments);
  
  gtk_widget_class_bind_template_callback (widget_class, on_registration_dialog_response);
  gtk_widget_class_bind_template_callback (widget_class, on_privacy_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, toggle_button_set_visible_on_toggle);
  gtk_widget_class_bind_template_callback (widget_class, toggle_button_set_sensitive_on_toggle);
  gtk_widget_class_bind_template_callback (widget_class, on_viewport_draw);
  
  object_class->finalize = glade_registration_finalize;
}

GtkWidget*
glade_registration_new (void)
{
  return GTK_WIDGET (g_object_new (GLADE_TYPE_REGISTRATION, NULL));
}

