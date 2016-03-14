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

#define CONFIG_GROUP             "User & Survey"

/* translators: Email subject sent to the user after completing the survey */
#define MAIL_SUBJECT _("Glade User Survey")
/* translators: Email body sent to the user after completing the survey */
#define MAIL_BODY _("Thank you for taking Glade Users survey, we appreciate it!\n\nTo validate this email address open the folowing link\n\nhttps://people.gnome.org/~jpu/glade/registration.php?email=$email&validation_token=$new_validation_token\n\nIn case you want to change or update the survey, your current update token is:\n$new_token\n\nCheers\n\n	The Glade team\n")

/* translators: Email subject sent to the user after updating the survey */
#define UPDATE_MAIL_SUBJECT _("Glade User Survey (update)")
/* translators: Email body sent to the user after updating the survey */
#define UPDATE_MAIL_BODY _("Thank you for updating your Glade Users survey data, we appreciate it!\n\nIn case you want to change something again, your current update token is:\n$new_token\n\nCheers\n\n	The Glade team\n")

struct _GladeRegistrationPrivate
{
  GtkWidget    *infobar;
  GtkWidget    *net_spinner;
  GtkLabel     *infobar_label;
  GtkLabel     *status_label;
  GladeHTTP    *http;
  GladeHTTP    *sub_http;
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

  GtkWidget *update_token_checkbutton;
  GtkWidget *update_token;

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

enum 
{
  PROP_0,
  PROP_COMPLETED,
  PROP_SKIP_REMINDER
};

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
                                 const gchar       *format,
                                 ...) G_GNUC_PRINTF (3, 4);

static void
glade_registration_show_message (GladeRegistration *registration,
                                 GtkMessageType     type,
                                 const gchar       *format,
                                 ...)
{
  GladeRegistrationPrivate *priv = registration->priv;

  gtk_info_bar_set_message_type (GTK_INFO_BAR (priv->infobar), type);

  if (format)
    {
      va_list args;
      gchar *string;

      va_start (args, format);
      string = g_strdup_vprintf (format, args);
      va_end (args);

      gtk_label_set_text (priv->infobar_label, string);

      g_free (string);
    }
  else
    gtk_label_set_text (priv->infobar_label, "");
  
  /* Only show the infobar if the dialog is visible */
  if (gtk_widget_is_visible (GTK_WIDGET (registration)))
    gtk_widget_show (priv->infobar);
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
glade_registration_http_post (GladeRegistration *registration,
                              GladeHTTP         *http,
                              GCancellable      *cancellable,
                              const gchar       *url,
                              GString           *content)
{
  const gchar *lang = pango_language_to_string (pango_language_get_default ());

  glade_http_request_send_async (http, cancellable,
                                 "POST %s HTTP/1.1\r\n"
                                 "Host: %s\r\n"
                                 "User-Agent: Glade/"PACKAGE_VERSION" (%s; Gtk+ %d.%d.%d; glib %d.%d.%d; %s)\r\n"
                                 "Connection: close\r\n"
                                 "Accept: text/plain\r\n"
                                 "Accept-Language: %s\r\n"
                                 "Content-Type: application/x-www-form-urlencoded\r\n"
                                 "Content-Length: %"G_GSIZE_FORMAT"\r\n"
                                 "\r\n%s",
                                 url,                                                       /* POST url */
                                 glade_http_get_host (http),                                /* Host */
                                 get_gdk_backend (GTK_WIDGET (registration)),               /* User-Agent backend */
                                 GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION,   /* User-Agent gtk version */
                                 glib_major_version, glib_minor_version, glib_micro_version,/* User-Agent glib version */
                                 lang,                                                      /* User-Agent language */
                                 lang,                                                      /* Accept-Language */
                                 content->len,                                              /* Content-length */
                                 content->str);                                             /* content */
}


#define append_input_tuple(s,i) string_append_input_key_value_tuple (s, #i, priv->i)

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
        glade_registration_show_message (registration, GTK_MESSAGE_WARNING,
                                         "%s", error->message);
      break;
    }

  gtk_label_set_text (registration->priv->status_label, text ? text : "");
  gtk_widget_set_visible (registration->priv->net_spinner, text != NULL);
  g_free (text);
}


static void 
on_subscribe_http_request_done (GladeHTTP         *http,
                                gint               code,
                                const gchar      **headers,
                                const gchar      **values,
                                const gchar       *response,
                                GladeRegistration *registration)
{
  GtkDialog *dialog;
  GtkWidget *button;
  
  if (code == 200)
    return;

  dialog = GTK_DIALOG (gtk_message_dialog_new (GTK_WINDOW (registration),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
                                               "%s",
                                               _("Sorry, automatic subscription to Glade Users mailing list failed")));
  
  button = gtk_link_button_new_with_label ("http://lists.ximian.com/mailman/listinfo/glade-users",
                                           _("Open Glade Users Website"));
  gtk_widget_show (button);
  gtk_dialog_add_action_widget (dialog, button, GTK_RESPONSE_CANCEL);
  gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (gtk_dialog_get_action_area (dialog)), button, TRUE);

  gtk_dialog_run (dialog);
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
glade_registration_subscribe_email (GladeRegistration *registration)
{
  GladeRegistrationPrivate *priv = registration->priv;
  GString *post;

  if (!priv->sub_http)
    {
      priv->sub_http = glade_http_new ("lists.ximian.com", 80, FALSE);
      g_signal_connect_object (priv->sub_http, "request-done",
                               G_CALLBACK (on_subscribe_http_request_done),
                               registration, 0);
    }
  
  post = g_string_new ("");

  string_append_input_key_value_tuple (post, "email", priv->email);
  string_append_input_key_value_tuple (post, "fullname", priv->name);

  glade_registration_http_post (registration, priv->sub_http, NULL,
                                "/mailman/subscribe/glade-users",
                                post);

  g_string_free (post, TRUE);
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
          GladeRegistrationPrivate *priv = registration->priv;
          gint i;

          for (i = 0; headers[i]; i++)
            {
              if (g_strcmp0 (headers[i], "X-Glade-Status") == 0)
                status = values[i];
              else if (g_strcmp0 (headers[i], "X-Glade-Message") == 0)
                message = values[i];
            }

          if (status == NULL)
            glade_registration_show_message (registration, GTK_MESSAGE_WARNING, 
                                             "%s", message);
          else if (g_strcmp0 (status, "ok") == 0)
            {
              gtk_label_set_text (priv->status_label, "");
              gtk_widget_hide (priv->net_spinner);

              if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->subscribe)))
                glade_registration_subscribe_email (registration);

              glade_util_ui_message (GTK_WIDGET (registration), GLADE_UI_INFO, NULL,
                                     "<big>%s</big>", _("Thank you for taking the time to complete the survey, we appreciate it!"));
              gtk_widget_hide (GTK_WIDGET (registration));

              g_object_set (registration, "completed", TRUE, NULL);
              glade_app_config_save ();
            }
          else if (g_strcmp0 (status, "error_required_field") == 0)
            glade_registration_show_message (registration, GTK_MESSAGE_INFO,
                                             "%s", _("Name and Email fields are required"));
          else if (g_strcmp0 (status, "error_email_in_use") == 0)
            glade_registration_show_message (registration, GTK_MESSAGE_WARNING,
                                             "%s", _("Oops! Email address is already in use!\nTo update information you need to provide the token that was sent to your inbox."));
          else if (g_strcmp0 (status, "error_db_user_info") == 0)
            glade_registration_show_message (registration, GTK_MESSAGE_WARNING,
                                             _("Oops! Error saving user information: %s"), message);
          else if (g_strcmp0 (status, "error_db_survey_data") == 0)
            glade_registration_show_message (registration, GTK_MESSAGE_WARNING,
                                             _("Oops! Error saving survey data: %s"), message);
          else if (g_strcmp0 (status, "error_db") == 0)
            glade_registration_show_message (registration, GTK_MESSAGE_WARNING,
                                             _("Oops! Error accessing DB: %s"), message);
          else
            glade_registration_show_message (registration, GTK_MESSAGE_WARNING, "%s", message);
        }
      break;
      default:
        glade_registration_show_message (registration, GTK_MESSAGE_WARNING,
                                         "%s", response ? response : "");
      break;
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

  append_input_tuple (post, update_token_checkbutton);
  append_input_tuple (post, update_token);
  
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

  glade_registration_http_post (GLADE_REGISTRATION (dialog),
                                priv->http,
                                priv->cancellable,
                                "/~jpu/glade/registration.php",
                                post);
  
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

static gboolean
on_viewport_draw (GtkWidget *viewport, cairo_t *cr, GladeRegistration *widget)
{
  GladeRegistrationPrivate *priv = GLADE_REGISTRATION (widget)->priv;
  GtkStyleContext *context = gtk_widget_get_style_context (viewport);
  GtkAllocation alloc;
  gdouble scale;
  GdkRGBA c;

  gtk_style_context_get_color (context, gtk_style_context_get_state (context), &c);

  gtk_widget_get_allocation (viewport, &alloc);
      
  scale = MIN (alloc.width/GLADE_LOGO_WIDTH, alloc.height/GLADE_LOGO_HEIGHT) - .1;
      
  cairo_save (cr);

  cairo_set_source_rgba (cr, c.red, c.green, c.blue, .04);
  cairo_scale (cr, scale, scale);
  cairo_translate (cr, (alloc.width / scale) - GLADE_LOGO_WIDTH*.95,
                   (alloc.height / scale) - GLADE_LOGO_HEIGHT);
  cairo_append_path (cr, &glade_logo_path);
  cairo_fill (cr);

  cairo_restore (cr);

  return FALSE;
}

static void
glade_registration_init (GladeRegistration *registration)
{
  GladeRegistrationPrivate *priv = glade_registration_get_instance_private (registration);

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
  g_clear_object (&priv->sub_http);
  g_clear_object (&priv->cancellable);

  G_OBJECT_CLASS (glade_registration_parent_class)->finalize (object);
}

static void
glade_registration_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  GKeyFile *config = glade_app_get_config ();
  g_return_if_fail (GLADE_IS_REGISTRATION (object));

  switch (prop_id)
    {
      case PROP_SKIP_REMINDER:
      case PROP_COMPLETED:
        g_key_file_set_boolean (config, CONFIG_GROUP, pspec->name, g_value_get_boolean (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_registration_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  GKeyFile *config = glade_app_get_config ();
  g_return_if_fail (GLADE_IS_REGISTRATION (object));

  switch (prop_id)
    {
      case PROP_SKIP_REMINDER:
      case PROP_COMPLETED:
        g_value_set_boolean (value, g_key_file_get_boolean (config, CONFIG_GROUP, pspec->name, NULL));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_registration_class_init (GladeRegistrationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkCssProvider *css_provider;

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
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, update_token_checkbutton);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, update_token);

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
  object_class->set_property = glade_registration_set_property;
  object_class->get_property = glade_registration_get_property;

  g_object_class_install_property (object_class,
                                   PROP_COMPLETED,
                                   g_param_spec_boolean ("completed",
                                                         "Completed",
                                                         "Registration was completed successfully",
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_SKIP_REMINDER,
                                   g_param_spec_boolean ("skip-reminder",
                                                         "Skip reminder",
                                                         "Skip registration reminder dialog",
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  /* Setup Custom CSS */
  css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (css_provider, "/org/gnome/glade/glade-registration.css");

  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (css_provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
 g_object_unref (css_provider);
}

GtkWidget*
glade_registration_new (void)
{
  return GTK_WIDGET (g_object_new (GLADE_TYPE_REGISTRATION, NULL));
}

