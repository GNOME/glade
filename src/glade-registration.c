/*
 * Copyright (C) 2014, 2020 Juan Pablo Ugarte.
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
#include "glade-window.h"
#include "glade-http.h"
#include "glade-logo.h"
#include <gladeui/glade.h>
#include <glib/gi18n.h>

#define USER_SURVEY_CONFIG_GROUP "User & Survey"
#define SURVEY_DATA_CONFIG_GROUP "Survey Data"

struct _GladeRegistrationPrivate
{
  GKeyFile     *config;
  gchar        *user_agent;

  GtkWidget    *infobar;
  GtkWidget    *statusbar;
  GtkWidget    *net_spinner;
  GtkLabel     *infobar_label;
  GtkLabel     *status_label;
  GtkWidget    *submit_button;
  GtkContainer *user_survey_box;

  GladeHTTP    *http;
  GladeHTTP    *sub_http;
  GCancellable *cancellable;
  GCancellable *sub_cancellable;

  /* HTML Form parsing */
  GHashTable   *hidden_inputs;
  gboolean      form_loaded;
  GHashTable   *sub_hidden_inputs;
  gboolean      sub_form_loaded;

  /* Form widgets */

  GtkWidget *name;
  GtkWidget *email;
  GtkWidget *country_id;
  GtkWidget *city;
  GtkWidget *contact_type;
  GtkWidget *contact_name;
  GtkWidget *contact_website;
  GtkWidget *subscribe;

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
  GtkWidget *lang_rust;
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
glade_registration_parse_response_form (GladeRegistration *registration,
                                        const gchar       *response,
                                        const gchar       *form_action,
                                        GHashTable        *data);

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
  else if (GTK_IS_TOGGLE_BUTTON (input) &&
           gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (input)))
    g_string_append_c (string,  'Y');
}

static void
append_hidden_inputs (gpointer key, gpointer value, gpointer user_data)
{
  GString *str = user_data;
  g_string_append_printf (str, "&%s=%s", (gchar *)key, (gchar *)value);
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
      g_autofree gchar *string;
      va_list args;

      va_start (args, format);
      string = g_strdup_vprintf (format, args);
      va_end (args);

      gtk_label_set_markup (priv->infobar_label, string);
    }
  else
    gtk_label_set_markup (priv->infobar_label, "");
  
  /* Only show the infobar if the dialog is visible */
  if (gtk_widget_is_visible (GTK_WIDGET (registration)))
    gtk_widget_show (priv->infobar);
}

static void
glade_registration_http_post (GladeRegistration *registration,
                              GladeHTTP         *http,
                              GCancellable      *cancellable,
                              const gchar       *url,
                              GString           *content)
{
  const gchar *lang = pango_language_to_string (pango_language_get_default ());
  g_autofree gchar *cookies = glade_http_get_cookies (http);

  glade_http_request_send_async (http, cancellable,
                                 "POST %s HTTP/1.1\r\n"
                                 "Host: %s\r\n"
                                 "User-Agent: %s\r\n"
                                 "Connection: close\r\n"
                                 "Accept: text/html\r\n"
                                 "Accept-Language: %s\r\n"
                                 "Cookie: %s\r\n"
                                 "Origin: https://%s\r\n"
                                 "Referer: https://%s%s\r\n"
                                 "Content-Type: application/x-www-form-urlencoded\r\n"
                                 "Content-Length: %"G_GSIZE_FORMAT"\r\n"
                                 "\r\n%s",
                                 url,                            /* POST url */
                                 glade_http_get_host (http),     /* Host */
                                 registration->priv->user_agent, /* User-Agent */
                                 lang,                           /* Accept-Language */
                                 cookies,                        /* Cookie */
                                 glade_http_get_host (http),     /* Origin */
                                 glade_http_get_host (http),     /* Referer */
                                 url,                            /* Referer */
                                 content->len,                   /* Content-length */
                                 content->str);                  /* content */
}

static void
glade_registration_http_get (GladeRegistration *registration,
                             GladeHTTP         *http,
                             GCancellable      *cancellable,
                             const gchar       *url)
{
  glade_http_request_send_async (http, cancellable,
                                 "GET %s HTTP/1.1\r\n"
                                 "Host: %s\r\n"
                                 "User-Agent: %s\r\n"
                                 "Connection: close\r\n"
                                 "Accept: text/html\r\n"
                                 "Content-Length: 0\r\n"
                                 "\r\n",
                                 url,                             /* POST url */
                                 glade_http_get_host (http),      /* Host */
                                 registration->priv->user_agent); /* User-Agent */
}

static void 
on_http_status (GladeHTTP         *http,
                GladeHTTPStatus    status,
                GError            *error,
                GladeRegistration *registration)
{
  g_autofree gchar *text = NULL;
  
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
  gtk_widget_set_visible (registration->priv->statusbar, text != NULL);
}

static void
glade_registration_subscribe_email (GladeRegistration *registration)
{
  GladeRegistrationPrivate *priv = registration->priv;
  g_autoptr(GString) post = g_string_new ("");

  string_append_input_key_value_tuple (post, "email", priv->email);
  string_append_input_key_value_tuple (post, "fullname", priv->name);

  /* Append magic token */
  g_hash_table_foreach (priv->sub_hidden_inputs, append_hidden_inputs, post);

  glade_registration_http_post (registration, priv->sub_http, NULL,
                                "/mailman/subscribe/glade-users-list",
                                post);
}

static void
on_subscribe_http_request_done (GladeHTTP         *http,
                                gint               code,
                                const gchar       *response,
                                GladeRegistration *registration)
{
  GladeRegistrationPrivate *priv = registration->priv;

  switch (code)
    {
      case 100:
        /* Ignore Continue HTTP response */
      break;
      case 200:
        {
          if (!priv->sub_form_loaded)
            {
              /* Get magic token from html form */
              glade_registration_parse_response_form (registration,
                                                      response,
                                                      "subscribe/glade-users-list",
                                                      priv->sub_hidden_inputs);
              priv->sub_form_loaded = TRUE;

              /* Now that we have the magic token we can make the actual POST */
              glade_registration_subscribe_email (registration);
            }
          else
            glade_registration_show_message (registration, GTK_MESSAGE_INFO, "%s",
                                             _("Your subscription to the users list has been received!\nCheck you email!"));
        }
      break;
      default:
        {
#define SUBSCRIPTION_FAILED "Sorry, automatic subscription to Glade Users mailing list failed.\n\
<a href=\"https://mail.gnome.org/mailman/listinfo/glade-users-list\">Open Glade Users Website</a>"

          glade_registration_show_message (registration, GTK_MESSAGE_WARNING,
                                           "%s\nHTTP response %d",
                                           _(SUBSCRIPTION_FAILED),
                                           code);
        }
      break;
    }
}

static void
glade_registration_clear (GladeRegistrationPrivate *priv)
{
  g_clear_object (&priv->cancellable);
  g_clear_object (&priv->sub_cancellable);
  
  priv->cancellable = g_cancellable_new ();
  priv->sub_cancellable = g_cancellable_new ();

  gtk_widget_hide (priv->infobar);
  g_hash_table_remove_all (priv->hidden_inputs);
  g_hash_table_remove_all (priv->sub_hidden_inputs);
  priv->form_loaded = FALSE;
  priv->sub_form_loaded = FALSE;
}

#define ADD_USER_INPUT(p,q,i) string_append_input_key_value_tuple (p, "311811X51X"q, priv->i)
#define ADD_SURVEY_INPUT(p,q,i) string_append_input_key_value_tuple (p, "311811X57X"q, priv->i)

static void
glade_registration_submit_survey (GladeRegistration *registration)
{
  GladeRegistrationPrivate *priv = registration->priv;
  g_autoptr(GString) post;

  post = g_string_new ("move=movesubmit&ajax=off&311811X51X843=");

  /* User Agent */
  g_string_append_uri_escaped (post, priv->user_agent, NULL, FALSE);

  /* User info */
  ADD_USER_INPUT (post, "849", name);
  ADD_USER_INPUT (post, "852", email);
  ADD_USER_INPUT (post, "1074", country_id);
  ADD_USER_INPUT (post, "858", city);
  ADD_USER_INPUT (post, "861", contact_type);
  ADD_USER_INPUT (post, "864", contact_name);
  ADD_USER_INPUT (post, "867", contact_website);

  /* Survey data */
  ADD_SURVEY_INPUT (post, "873", experience);
  ADD_SURVEY_INPUT (post, "879", experience_unit);
  ADD_SURVEY_INPUT (post, "882", experience_not_programmer);
  ADD_SURVEY_INPUT (post, "885c", lang_c);
  ADD_SURVEY_INPUT (post, "885cpp", lang_cpp);
  ADD_SURVEY_INPUT (post, "885csharp", lang_csharp);
  ADD_SURVEY_INPUT (post, "885java", lang_java);
  ADD_SURVEY_INPUT (post, "885python", lang_python);
  ADD_SURVEY_INPUT (post, "885javascript", lang_javascript);
  ADD_SURVEY_INPUT (post, "885vala", lang_vala);
  ADD_SURVEY_INPUT (post, "885perl", lang_perl);
  ADD_SURVEY_INPUT (post, "885rust", lang_rust);
  ADD_SURVEY_INPUT (post, "885other", lang_other);
  ADD_SURVEY_INPUT (post, "915", start_using);
  ADD_SURVEY_INPUT (post, "918", start_using_unit);
  ADD_SURVEY_INPUT (post, "921", version);
  ADD_SURVEY_INPUT (post, "924", version_other);
  ADD_SURVEY_INPUT (post, "927", os);

  ADD_SURVEY_INPUT (post, "930", os_linux);
  ADD_SURVEY_INPUT (post, "933", os_bsd);
  ADD_SURVEY_INPUT (post, "936", os_windows);
  ADD_SURVEY_INPUT (post, "939", os_osx);
  ADD_SURVEY_INPUT (post, "942", os_solaris);

  ADD_SURVEY_INPUT (post, "945", os_other);

  ADD_SURVEY_INPUT (post, "948", freq);
  ADD_SURVEY_INPUT (post, "951", user_level);

  ADD_SURVEY_INPUT (post, "954free", soft_free);
  ADD_SURVEY_INPUT (post, "954open", soft_open);
  ADD_SURVEY_INPUT (post, "954commercial", soft_commercial);
  ADD_SURVEY_INPUT (post, "954none", soft_none);

  ADD_SURVEY_INPUT (post, "969academic", field_academic);
  ADD_SURVEY_INPUT (post, "969accounting", field_accounting);
  ADD_SURVEY_INPUT (post, "969desktop", field_desktop);
  ADD_SURVEY_INPUT (post, "969educational", field_educational);
  ADD_SURVEY_INPUT (post, "969embedded", field_embedded);
  ADD_SURVEY_INPUT (post, "969medical", field_medical);
  ADD_SURVEY_INPUT (post, "969industrial", field_industrial);
  ADD_SURVEY_INPUT (post, "969scientific", field_scientific);

  ADD_SURVEY_INPUT (post, "1134", field_other);

  ADD_SURVEY_INPUT (post, "999", improvement);
  ADD_SURVEY_INPUT (post, "1002", problem);
  ADD_SURVEY_INPUT (post, "1140", problem_other);
  ADD_SURVEY_INPUT (post, "1005", bug);

  ADD_SURVEY_INPUT (post, "1008", bugzilla);
  ADD_SURVEY_INPUT (post, "1011", contributing);
  ADD_SURVEY_INPUT (post, "1014", contributing_whynot);
  ADD_SURVEY_INPUT (post, "1017", comments);

  /* Append hidden inputs */
  g_hash_table_foreach (priv->hidden_inputs, append_hidden_inputs, post);

  glade_registration_http_post (GLADE_REGISTRATION (registration),
                                priv->http,
                                priv->cancellable,
                                "/index.php/311811",
                                post);
}

static void
on_http_request_done (GladeHTTP         *http,
                      gint               code,
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
          GladeRegistrationPrivate *priv = registration->priv;

          if (!priv->form_loaded)
            {
              glade_registration_parse_response_form (registration,
                                                      response,
                                                      "311811",
                                                      priv->hidden_inputs);
              priv->form_loaded = TRUE;

              /* Now that we have the magic token we can make the actual POST */
              glade_registration_submit_survey (registration);
              return;
            }

          if (g_strstr_len (response, -1, "GLADE_USER_REGISTRATION_SURVEY_OK") != NULL)
            {
              gtk_label_set_text (priv->status_label, "");
              gtk_widget_hide (priv->statusbar);

              g_object_set (registration, "completed", TRUE, NULL);
              glade_app_config_save ();

              glade_util_ui_message (GTK_WIDGET (registration), GLADE_UI_INFO, NULL,
                                     "<big>%s</big>", _("Thank you for taking the time to complete the survey, we appreciate it!"));
              gtk_widget_hide (GTK_WIDGET (registration));
            }
          else
            glade_registration_show_message (registration, GTK_MESSAGE_WARNING,
                                             "Internal server error");
        }
      break;
      default:
        glade_registration_show_message (registration, GTK_MESSAGE_WARNING,
                                         "Got HTTP response %d", code);
      break;
    }
}

static inline void
survey_data_set_string (GKeyFile *config, const gchar *id, const gchar *value)
{
  if (value)
    g_key_file_set_string (config, SURVEY_DATA_CONFIG_GROUP, id, value);
}

void
glade_registration_save_state_foreach (GtkWidget *widget, gpointer data)
{
  GladeRegistration *registration = GLADE_REGISTRATION (data);
  const gchar *id = gtk_buildable_get_name (GTK_BUILDABLE (widget));

  if (id)
    {
      GKeyFile *config = registration->priv->config;

      if (GTK_IS_ENTRY (widget))
        survey_data_set_string (config, id, gtk_entry_get_text (GTK_ENTRY (widget)));
      else if (GTK_IS_TEXT_VIEW (widget))
        {
          GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
          GtkTextIter start, end;
          g_autofree gchar *text;

          gtk_text_buffer_get_bounds (buffer, &start, &end);
          text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
          survey_data_set_string (config, id, text);
        }
      else if (GTK_IS_COMBO_BOX (widget))
        survey_data_set_string (config, id, gtk_combo_box_get_active_id (GTK_COMBO_BOX (widget)));
      else if (GTK_IS_TOGGLE_BUTTON (widget))
        g_key_file_set_boolean (config, SURVEY_DATA_CONFIG_GROUP, id,
                                gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
    }

  /* Recurse all containers */
  if (GTK_IS_CONTAINER (widget))
    gtk_container_foreach (GTK_CONTAINER (widget),
                           glade_registration_save_state_foreach,
                           data);
}

void
glade_registration_load_state_foreach (GtkWidget *widget, gpointer data)
{
  GladeRegistration *registration = GLADE_REGISTRATION (data);
  const gchar *id = gtk_buildable_get_name (GTK_BUILDABLE (widget));

  if (id)
    {
      GKeyFile *config = registration->priv->config;

      if (GTK_IS_SPIN_BUTTON (widget))
        {
          gint val = g_key_file_get_integer (config, SURVEY_DATA_CONFIG_GROUP, id, NULL);
          gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), val);
        }
      else if (GTK_IS_ENTRY (widget))
        {
          g_autofree gchar *val = NULL;
          val = g_key_file_get_string (config, SURVEY_DATA_CONFIG_GROUP, id, NULL);
          gtk_entry_set_text (GTK_ENTRY (widget), val ? val : "");
        }
      else if (GTK_IS_TEXT_VIEW (widget))
        {
          g_autofree gchar *val = NULL;

          val = g_key_file_get_string (config, SURVEY_DATA_CONFIG_GROUP, id, NULL);
          GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));

          gtk_text_buffer_set_text (buffer, val ? val : "", -1);
        }
      else if (GTK_IS_COMBO_BOX (widget)){
        g_autofree gchar *val = NULL;

        val = g_key_file_get_string (config, SURVEY_DATA_CONFIG_GROUP, id, NULL);
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (widget), val ? val : "");
      }
      else if (GTK_IS_TOGGLE_BUTTON (widget)){
        gboolean val = g_key_file_get_boolean (config, SURVEY_DATA_CONFIG_GROUP, id, NULL);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), val);
      }
    }

  /* Recurse all containers */
  if (GTK_IS_CONTAINER (widget))
    gtk_container_foreach (GTK_CONTAINER (widget),
                           glade_registration_load_state_foreach,
                           data);
}

static gboolean
entry_is_empty (GtkWidget *entry)
{
  const gchar *text = gtk_entry_get_text (GTK_ENTRY (entry));
  g_autofree gchar *str = NULL;

  if (!text || *text == '\0')
    return TRUE;

  str = g_strstrip (g_strdup (text));

  return (!str || *str == '\0');
}

static gboolean
glade_registration_verify (GtkWidget *entry)
{
  GtkStyleContext *ctx = gtk_widget_get_style_context (entry);

  gtk_style_context_remove_class (ctx, "error");

  if (entry_is_empty (entry))
    {
      gtk_style_context_add_class (ctx, "error");
      gtk_widget_grab_focus (entry);
      return TRUE;
    }

  if (gtk_entry_get_input_purpose (GTK_ENTRY (entry)) == GTK_INPUT_PURPOSE_EMAIL)
    {
      const char *text = gtk_entry_get_text (GTK_ENTRY (entry));
      g_auto(GStrv) email = NULL;

      /* Check if this looks like an email */
      if (g_strstr_len (text, -1, " ") ||
          g_strstr_len (text, -1, "\t") ||
          g_strstr_len (text, -1, "\n") ||
          !(email = g_strsplit (text, "@", 2)) ||
          g_strv_length (email) != 2 ||
          *email[0] == '\0'|| *email[1] == '\0')
        {
          gtk_style_context_add_class (ctx, "error");
          gtk_widget_grab_focus (entry);
          return TRUE;
        }
    }

  return FALSE;
}

static void
glade_registration_verify_entry (GtkEntry *entry, GladeRegistration *registration)
{
  glade_registration_verify (GTK_WIDGET (entry));
}

static void
on_registration_dialog_response (GtkDialog *dialog, gint response_id)
{
  GladeRegistration *registration = GLADE_REGISTRATION (dialog);
  GladeRegistrationPrivate *priv = registration->priv;

  gtk_widget_hide (priv->infobar);

  /* Save state */
  gtk_container_foreach (priv->user_survey_box,
                         glade_registration_save_state_foreach,
                         registration);
  glade_app_config_save ();

  if (response_id != GTK_RESPONSE_OK)
    {
      gtk_widget_hide (GTK_WIDGET (dialog));
      glade_registration_clear (priv);
      return;
    }

  if (glade_registration_verify (priv->name) ||
      glade_registration_verify (priv->email) ||
      g_key_file_get_boolean (priv->config, USER_SURVEY_CONFIG_GROUP, "completed", NULL))
      return;

  /* Init state */
  glade_registration_clear (priv);


  /* Get survey form */
  glade_registration_http_get (registration,
                               priv->http,
                               priv->cancellable,
                               "/index.php/311811");

  /* Get mailman form */
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->subscribe)))
    glade_registration_http_get (registration,
                                 priv->sub_http,
                                 priv->sub_cancellable,
                                 "/mailman/listinfo/glade-users-list");
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
glade_registration_update_state (GladeRegistration *registration)
{
  GladeRegistrationPrivate *priv = glade_registration_get_instance_private (registration);

  if (g_key_file_get_boolean (priv->config, USER_SURVEY_CONFIG_GROUP, "completed", NULL))
    {
      GtkWidget *headerbar = gtk_dialog_get_header_bar (GTK_DIALOG (registration));

      gtk_widget_set_sensitive (GTK_WIDGET (priv->user_survey_box), FALSE);
      gtk_widget_set_sensitive (GTK_WIDGET (priv->submit_button), FALSE);

      if (headerbar)
        gtk_header_bar_set_subtitle(GTK_HEADER_BAR (headerbar),
                                    _("Completed and submitted!"));
    }
}

static void
glade_registration_init (GladeRegistration *registration)
{
  GladeRegistrationPrivate *priv = glade_registration_get_instance_private (registration);

  registration->priv = priv;

  priv->config = glade_app_get_config ();

  /* Create user agent */
  priv->user_agent =
    g_strdup_printf ("Glade/"PACKAGE_VERSION" (%s; Gtk+ %d.%d.%d; glib %d.%d.%d; %s)",
                     glade_window_get_gdk_backend (),                           /* Gdk backend */
                     GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION,   /* Gtk version */
                     glib_major_version, glib_minor_version, glib_micro_version,/* Glib version */
                     pango_language_to_string (pango_language_get_default ()));

  priv->hidden_inputs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  priv->sub_hidden_inputs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  /* HTTPS default port is 443 */
  priv->http = glade_http_new ("surveys.gnome.org", 443, TRUE);

  g_signal_connect_object (priv->http, "request-done",
                           G_CALLBACK (on_http_request_done),
                           registration, 0);
  g_signal_connect_object (priv->http, "status",
                           G_CALLBACK (on_http_status),
                           registration, 0);
  
  priv->sub_http = glade_http_new ("mail.gnome.org", 443, TRUE);
  g_signal_connect_object (priv->sub_http, "request-done",
                           G_CALLBACK (on_subscribe_http_request_done),
                           registration, 0);

  gtk_widget_init_template (GTK_WIDGET (registration));

  /* Generate Glade versions */
  if (GTK_IS_COMBO_BOX_TEXT (priv->version_other))
    {
      GtkComboBoxText *combo = GTK_COMBO_BOX_TEXT (priv->version_other);
      gchar id[16], text[18];
      gint minor;

      for (minor = 0; minor <= GLADE_MINOR_VERSION; minor+=2)
        {
          g_snprintf (id, 16, "%d", minor);
          g_snprintf (text, 18, "3.%d", minor);
          gtk_combo_box_text_prepend (combo, id, text);

          /* Skip non released version 24-34 */
          if (minor == 22)
            minor = 34;
        }

      gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
    }

  /* Load survey state */
  gtk_container_foreach (priv->user_survey_box,
                         glade_registration_load_state_foreach,
                         registration);

  gtk_dialog_set_default_response (GTK_DIALOG (registration), GTK_RESPONSE_OK);

  glade_registration_update_state (registration);
}

static void
glade_registration_finalize (GObject *object)
{
  GladeRegistrationPrivate *priv = GLADE_REGISTRATION (object)->priv;

  g_clear_pointer (&priv->user_agent, g_free);
  g_clear_object (&priv->http);
  g_clear_object (&priv->sub_http);
  g_clear_object (&priv->cancellable);
  g_clear_object (&priv->sub_cancellable);
  g_hash_table_destroy (priv->hidden_inputs);
  g_hash_table_destroy (priv->sub_hidden_inputs);

  G_OBJECT_CLASS (glade_registration_parent_class)->finalize (object);
}

static void
glade_registration_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  GladeRegistrationPrivate *priv;

  g_return_if_fail (GLADE_IS_REGISTRATION (object));

  priv = GLADE_REGISTRATION (object)->priv;

  switch (prop_id)
    {
      case PROP_SKIP_REMINDER:
        g_key_file_set_boolean (priv->config, USER_SURVEY_CONFIG_GROUP,
                                pspec->name, g_value_get_boolean (value));
        break;
      case PROP_COMPLETED:
        g_key_file_set_boolean (priv->config, USER_SURVEY_CONFIG_GROUP,
                                pspec->name, g_value_get_boolean (value));
        glade_registration_update_state (GLADE_REGISTRATION (object));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_registration_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  GladeRegistrationPrivate *priv;

  g_return_if_fail (GLADE_IS_REGISTRATION (object));

  priv = GLADE_REGISTRATION (object)->priv;

  switch (prop_id)
    {
      case PROP_SKIP_REMINDER:
      case PROP_COMPLETED:
        g_value_set_boolean (value, g_key_file_get_boolean (priv->config, USER_SURVEY_CONFIG_GROUP, pspec->name, NULL));
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
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, statusbar);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, net_spinner);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, infobar_label);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, status_label);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, submit_button);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, user_survey_box);
  
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, name);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, email);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, country_id);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, city);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, contact_type);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, contact_name);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, contact_website);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, subscribe);

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
  gtk_widget_class_bind_template_child_private (widget_class, GladeRegistration, lang_rust);
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
  
  gtk_widget_class_bind_template_callback (widget_class, glade_registration_verify_entry);
  gtk_widget_class_bind_template_callback (widget_class, on_registration_dialog_response);
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
  return GTK_WIDGET (g_object_new (GLADE_TYPE_REGISTRATION,
                                   "use-header-bar", TRUE,
                                   NULL));
}

/* HTML Form parsing utils */
#include <libxml/HTMLparser.h>

typedef struct {
  const gchar *form_action;
  GHashTable  *inputs;

  gboolean     in_form;
} ParseData;

static void
start_element (ParseData *data, const gchar *name, const gchar **atts)
{
  gboolean is_hidden = FALSE;
  const gchar *input_name = NULL, *input_value = NULL;
  gint i;

  if (g_strcmp0 (name, "form") == 0)
    {
      for (i = 0; atts[i]; i++)
        if (g_strcmp0 (atts[i], "action") == 0 &&
            g_strstr_len (atts[i+1], -1, data->form_action))
          data->in_form = TRUE;

      return;
    }

  /* Ignore elements outside of the form */
  if (!data->in_form)
    return;

  if (g_strcmp0 (name, "input"))
    return;

  for (i = 0; atts[i]; i += 2)
    {
      gint val = i + 1;

      if (!g_strcmp0 (atts[i], "type") && !g_strcmp0 (atts[val], "hidden"))
        is_hidden = TRUE;

      if (!g_strcmp0 (atts[i], "name"))
        input_name = atts[val];

      if (!g_strcmp0 (atts[i], "value"))
        input_value = atts[val];
    }

  if (is_hidden && input_name && input_value)
    g_hash_table_insert (data->inputs,
                         g_uri_escape_string (input_name, NULL, FALSE),
                         g_uri_escape_string (input_value, NULL, FALSE));
}

static void
end_element (ParseData *data, const gchar *name)
{
  if (g_strcmp0 (name, "form") == 0)
    data->in_form = FALSE;
}

static void
glade_registration_parse_response_form (GladeRegistration *registration,
                                        const gchar       *response,
                                        const gchar       *form_action,
                                        GHashTable        *inputs)
{
  ParseData data = { .form_action = form_action, .inputs = inputs };
  htmlSAXHandler sax = { NULL, };

  sax.startElement = (startElementSAXFunc) start_element;
  sax.endElement = (endElementSAXFunc) end_element;

  /* Parse response and collect hidden inputs in the hash table */
  xmlFreeDoc (htmlSAXParseDoc ((guchar *)response, NULL, &sax, &data));
}

