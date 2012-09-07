/*
 * Copyright (C) 2008 Tristan Van Berkom
 * Copyright (C) 2004 Joaquin Cuenca Abela
 * Copyright (C) 2001, 2002, 2003 Ximian, Inc.
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
 *   Joaquin Cuenca Abela <e98cuenc@yahoo.com>
 *   Chema Celorio <chema@celorio.com>
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/**
 * SECTION:glade-widget
 * @Short_Description: An object wrapper for the Glade runtime environment.
 *
 * #GladeWidget is the proxy between the instantiated runtime object and
 * the Glade core metadata. This api will be mostly usefull for its
 * convenience api for getting and setting properties (mostly from the plugin).
 */

#include <string.h>
#include <glib-object.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n-lib.h>
#include "glade.h"
#include "glade-accumulators.h"
#include "glade-project.h"
#include "glade-widget-adaptor.h"
#include "glade-widget.h"
#include "glade-marshallers.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-placeholder.h"
#include "glade-signal.h"
#include "glade-popup.h"
#include "glade-editor.h"
#include "glade-app.h"
#include "glade-design-view.h"
#include "glade-widget-action.h"
#include "glade-signal-model.h"
#include "glade-object-stub.h"

static void glade_widget_set_adaptor (GladeWidget * widget,
                                      GladeWidgetAdaptor * adaptor);
static void glade_widget_set_properties (GladeWidget * widget,
                                         GList * properties);
static void glade_widget_set_object (GladeWidget * gwidget,
                                     GObject * new_object, gboolean destroy);


struct _GladeWidgetPrivate {

  GladeWidgetAdaptor *adaptor; /* An adaptor class for the object type */

  GladeProject       *project; /* A pointer to the project that this 
				  widget currently belongs to. */

  GladeWidget  *parent;  /* A pointer to the parent widget in the hierarchy */
	
  gchar *name; /* The name of the widget. For example window1 or
		* button2. This is a unique name and is the one
		* used when loading widget with libglade
		*/

  gchar *support_warning; /* A warning message for version incompatabilities
			   * in this widget
			   */

  gchar *internal; /* If the widget is an internal child of 
		    * another widget this is the name of the 
		    * internal child, otherwise is NULL.
		    * Internal children cannot be deleted.
		    */

  gboolean anarchist; /* Some composite widgets have internal children
		       * that are not part of the same hierarchy; hence 'anarchists',
		       * typicly a popup window or its child (we need to mark
		       * them so we can avoid bookkeeping packing props on them etc.).
		       */

  GObject *object; /* A pointer to the object that was created.
		    * if it is a GtkWidget; it is shown as a "view"
		    * of the GladeWidget. This object is updated as
		    * the properties are modified for the GladeWidget.
		    */

  GList *properties; /* A list of GladeProperty. A GladeProperty is an
		      * instance of a GladePropertyClass. If a
		      * GladePropertyClass for a gtkbutton is label, its
		      * property is "Ok". 
		      */

  GList *packing_properties; /* A list of GladeProperty. Note that these
			      * properties are related to the container
			      * of the widget, thus they change after
			      * pasting the widget to a different
			      * container. Toplevels widget do not have
			      * packing properties.
			      * See also child_properties of 
			      * GladeWidgetClass.
			      */

  GHashTable *props_hash; /* A Quick reference table to speed up calls to glade_widget_get_property()
			   */	
  GHashTable *pack_props_hash; /* A Quick reference table to speed up calls to glade_widget_get_pack_property()
				*/

  GHashTable *signals; /* A table with a GPtrArray of GladeSignals (signal handlers),
			* indexed by its name */

  GList     *prop_refs; /* List of properties in the project who's value are `this object'
			 * (this is used to set/unset those properties when the object is
			 * added/removed from the project).
			 */

  gint               width;   /* Current size used in the UI, this is only */
  gint               height;  /* usefull for parentless widgets in the
			       * GladeDesignLayout */

  GList *actions;		/* A GladeWidgetAction list */

  GList *packing_actions;	/* A GladeWidgetAction list, this actions are
				 * related to the container and they are not always present.
				 */

  GladeWidget    *lock; /* The glade widget that has locked this widget down.
			 */
  GList          *locked_widgets; /* A list of widgets this widget has locked down.
				   */

  GtkTreeModel   *signal_model; /* Signal model (or NULL if not yet requested) */
    
  /* Construct parameters: */
  GladeWidget       *construct_template;
  GladeCreateReason  construct_reason;
  gchar             *construct_internal;
  guint              construct_exact : 1;

  guint              in_project : 1;

  guint              visible : 1; /* Local copy of widget visibility, we need to keep track of this
				   * since the objects copy may be invalid due to a rebuild.
				   */
};

enum
{
  ADD_SIGNAL_HANDLER,
  REMOVE_SIGNAL_HANDLER,
  CHANGE_SIGNAL_HANDLER,
  BUTTON_PRESS_EVENT,
  BUTTON_RELEASE_EVENT,
  MOTION_NOTIFY_EVENT,
  SUPPORT_CHANGED,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_NAME,
  PROP_INTERNAL,
  PROP_ANARCHIST,
  PROP_OBJECT,
  PROP_ADAPTOR,
  PROP_PROJECT,
  PROP_PROPERTIES,
  PROP_PARENT,
  PROP_INTERNAL_NAME,
  PROP_TEMPLATE,
  PROP_TEMPLATE_EXACT,
  PROP_REASON,
  PROP_TOPLEVEL_WIDTH,
  PROP_TOPLEVEL_HEIGHT,
  PROP_SUPPORT_WARNING,
  PROP_VISIBLE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];
static guint glade_widget_signals[LAST_SIGNAL] = { 0 };

static GQuark glade_widget_name_quark = 0;

G_DEFINE_TYPE (GladeWidget, glade_widget, G_TYPE_INITIALLY_UNOWNED)
/*******************************************************************************
                           GladeWidget class methods
 *******************************************************************************/
static void
glade_widget_set_packing_actions (GladeWidget * widget,
				  GladeWidget * parent)
{
  if (widget->priv->packing_actions)
    {
      g_list_foreach (widget->priv->packing_actions, (GFunc) g_object_unref, NULL);
      g_list_free (widget->priv->packing_actions);
      widget->priv->packing_actions = NULL;
    }

  widget->priv->packing_actions =
    glade_widget_adaptor_pack_actions_new (parent->priv->adaptor);
}

static void
glade_widget_add_child_impl (GladeWidget * widget,
                             GladeWidget * child, gboolean at_mouse)
{
  g_object_ref (child);

  /* Safe to set the parent first... setting it afterwards
   * creates packing properties, and that is not always
   * desirable.
   */
  glade_widget_set_parent (child, widget);

  /* Set packing actions first so we have access from the plugin
   */
  glade_widget_set_packing_actions (child, widget);

  glade_widget_adaptor_add (widget->priv->adaptor, 
			    widget->priv->object, 
			    child->priv->object);

  /* XXX FIXME:
   * We have a fundamental flaw here, we set packing props
   * after parenting the widget so that we can introspect the
   * values setup by the runtime widget, in which case the plugin
   * cannot access its packing properties and set them sensitive
   * or connect to thier signals etc. maybe its not so important
   * but its a flaw worthy of note, some kind of double pass api
   * would be needed to accomadate this.
   */


  /* Setup packing properties here so we can introspect the new
   * values from the backend.
   */
  glade_widget_set_packing_properties (child, widget);
}

static void
glade_widget_remove_child_impl (GladeWidget * widget, GladeWidget * child)
{
  glade_widget_adaptor_remove (widget->priv->adaptor, widget->priv->object, child->priv->object);

  child->priv->parent = NULL;

  g_object_unref (child);
}

static void
glade_widget_replace_child_impl (GladeWidget * widget,
                                 GObject * old_object, GObject * new_object)
{
  GladeWidget *gnew_widget = glade_widget_get_from_gobject (new_object);
  GladeWidget *gold_widget = glade_widget_get_from_gobject (old_object);

  if (gnew_widget)
    {
      g_object_ref (gnew_widget);

      gnew_widget->priv->parent = widget;

      /* Set packing actions first so we have access from the plugin
       */
      glade_widget_set_packing_actions (gnew_widget, widget);
    }

  if (gold_widget)
    {
      g_object_unref (gold_widget);

      if (gold_widget != gnew_widget)
        gold_widget->priv->parent = NULL;
    }

  glade_widget_adaptor_replace_child
      (widget->priv->adaptor, widget->priv->object, old_object, new_object);

  /* Setup packing properties here so we can introspect the new
   * values from the backend.
   */
  if (gnew_widget)
    glade_widget_set_packing_properties (gnew_widget, widget);
}

/**
 * glade_widget_add_signal_handler:
 * @widget: A #GladeWidget
 * @signal_handler: The #GladeSignal
 *
 * Adds a signal handler for @widget 
 */
void
glade_widget_add_signal_handler (GladeWidget *widget, const GladeSignal *signal_handler)
{
  GPtrArray *signals;
  GladeSignal *new_signal_handler;

  g_return_if_fail (GLADE_IS_WIDGET (widget));
  g_return_if_fail (GLADE_IS_SIGNAL (signal_handler));

  signals = glade_widget_list_signal_handlers (widget, glade_signal_get_name (signal_handler));
  if (!signals)
    {
      signals = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
      g_hash_table_insert (widget->priv->signals, 
			   g_strdup (glade_signal_get_name (signal_handler)),
                           signals);
    }

  new_signal_handler = glade_signal_clone (signal_handler);
  g_ptr_array_add (signals, new_signal_handler);
  g_signal_emit (widget, glade_widget_signals[ADD_SIGNAL_HANDLER], 0, new_signal_handler);

  glade_project_verify_signal (widget, new_signal_handler);
}

/**
 * glade_widget_remove_signal_handler:
 * @widget: A #GladeWidget
 * @signal_handler: The #GladeSignal
 *
 * Removes a signal handler from @widget 
 */

void
glade_widget_remove_signal_handler (GladeWidget * widget,
                                    const GladeSignal * signal_handler)
{
  GPtrArray *signals;
  GladeSignal *tmp_signal_handler;
  guint i;

  g_return_if_fail (GLADE_IS_WIDGET (widget));
  g_return_if_fail (GLADE_IS_SIGNAL (signal_handler));

  signals = glade_widget_list_signal_handlers (widget, glade_signal_get_name (signal_handler));

  /* trying to remove an inexistent signal? */
  g_assert (signals);

  for (i = 0; i < signals->len; i++)
    {
      tmp_signal_handler = g_ptr_array_index (signals, i);
      if (glade_signal_equal (tmp_signal_handler, signal_handler))
        {
	  g_signal_emit (widget, glade_widget_signals[REMOVE_SIGNAL_HANDLER], 0, tmp_signal_handler);
          g_object_unref (tmp_signal_handler);
          g_ptr_array_remove_index (signals, i);
          break;
        }
    }
}

/**
 * glade_widget_change_signal_handler:
 * @widget: A #GladeWidget
 * @old_signal_handler: the old #GladeSignal
 * @new_signal_handler: the new #GladeSignal
 *
 * Changes a #GladeSignal on @widget 
 */
void
glade_widget_change_signal_handler (GladeWidget * widget,
                                    const GladeSignal * old_signal_handler,
                                    const GladeSignal * new_signal_handler)
{
  GPtrArray *signals;
  GladeSignal *signal_handler_iter;
  guint i;

  g_return_if_fail (GLADE_IS_WIDGET (widget));
  g_return_if_fail (GLADE_IS_SIGNAL (old_signal_handler));
  g_return_if_fail (GLADE_IS_SIGNAL (new_signal_handler));
  g_return_if_fail (strcmp (glade_signal_get_name (old_signal_handler), 
			    glade_signal_get_name (new_signal_handler)) == 0);

  signals =
    glade_widget_list_signal_handlers (widget, glade_signal_get_name (old_signal_handler));

  /* trying to remove an inexistent signal? */
  g_assert (signals);

  for (i = 0; i < signals->len; i++)
    {
      signal_handler_iter = g_ptr_array_index (signals, i);
      if (glade_signal_equal (signal_handler_iter, old_signal_handler))
        {
          /* Detail */
	  glade_signal_set_detail (signal_handler_iter, 
				   glade_signal_get_detail (new_signal_handler));
          
          /* Handler */
	  glade_signal_set_handler (signal_handler_iter, 
				    glade_signal_get_handler (new_signal_handler));

          /* Object */
	  glade_signal_set_userdata (signal_handler_iter, 
				     glade_signal_get_userdata (new_signal_handler));

	  /* Flags */
	  glade_signal_set_after (signal_handler_iter, 
				  glade_signal_get_after (new_signal_handler));
	  glade_signal_set_swapped (signal_handler_iter, 
				    glade_signal_get_swapped (new_signal_handler));

	  g_signal_emit (widget, glade_widget_signals[CHANGE_SIGNAL_HANDLER], 0, 
	                 signal_handler_iter);

	  break;
        }
    }
}

static gboolean
glade_widget_button_press_event_impl (GladeWidget * gwidget,
                                      GdkEvent * base_event)
{
  GtkWidget *widget;
  GdkEventButton *event = (GdkEventButton *) base_event;
  gboolean handled = FALSE;

  /* make sure to grab focus, since we may stop default handlers */
  widget = GTK_WIDGET (glade_widget_get_object (gwidget));
  if (gtk_widget_get_can_focus (widget) && !gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  /* if it's already selected don't stop default handlers, e.g. toggle button */
  if (event->button == 1)
    {
      if (event->state & GDK_CONTROL_MASK)
        {
          if (glade_project_is_selected (gwidget->priv->project, gwidget->priv->object))
            glade_project_selection_remove (gwidget->priv->project, gwidget->priv->object, TRUE);
          else
            glade_project_selection_add (gwidget->priv->project, gwidget->priv->object, TRUE);
          handled = TRUE;
        }
      else if (glade_project_is_selected (gwidget->priv->project,
                                          gwidget->priv->object) == FALSE)
        {
          glade_project_selection_set (gwidget->priv->project, 
				       gwidget->priv->object, TRUE);

          /* Add selection without interrupting event flow 
           * when shift is down, this allows better behaviour
           * for GladeFixed children 
           */
          handled = !(event->state & GDK_SHIFT_MASK);
        }
    }

  /* Give some kind of access in case of missing right button */
  if (!handled && glade_popup_is_popup_event (event))
    {
      glade_popup_widget_pop (gwidget, event, TRUE);
      handled = TRUE;
    }

  return handled;
}

static gboolean
glade_widget_event_impl (GladeWidget * gwidget, GdkEvent * event)
{
  gboolean handled = FALSE;

  g_return_val_if_fail (GLADE_IS_WIDGET (gwidget), FALSE);

  switch (event->type)
    {
      case GDK_BUTTON_PRESS:
        g_signal_emit (gwidget,
                       glade_widget_signals[BUTTON_PRESS_EVENT], 0,
                       event, &handled);
        break;
      case GDK_BUTTON_RELEASE:
        g_signal_emit (gwidget,
                       glade_widget_signals[BUTTON_RELEASE_EVENT], 0,
                       event, &handled);
        break;
      case GDK_MOTION_NOTIFY:
        g_signal_emit (gwidget,
                       glade_widget_signals[MOTION_NOTIFY_EVENT], 0,
                       event, &handled);
        break;
      default:
        break;
    }

  return handled;
}


/**
 * glade_widget_event:
 * @event: A #GdkEvent
 *
 * Feed an event to be handled on the project GladeWidget
 * hierarchy.
 *
 * Returns: whether the event was handled or not.
 */
gboolean
glade_widget_event (GladeWidget * gwidget, GdkEvent * event)
{
  gboolean handled = FALSE;

  /* Lets just avoid some synthetic events (like focus-change) */
  if (((GdkEventAny *) event)->window == NULL)
    return FALSE;

  handled = GLADE_WIDGET_GET_CLASS (gwidget)->event (gwidget, event);

#if 0
  if (event->type != GDK_EXPOSE)
    g_print ("event widget '%s' handled '%d' event '%d'\n",
             gwidget->priv->name, handled, event->type);
#endif

  return handled;
}

/*******************************************************************************
                      GObjectClass & Object Construction
 *******************************************************************************/

/*
 * This function creates new GObject parameters based on the GType of the 
 * GladeWidgetAdaptor and its default values.
 *
 * If a GladeWidget is specified, it will be used to apply the
 * values currently in use.
 */
static GParameter *
glade_widget_template_params (GladeWidget * widget,
                              gboolean construct, guint * n_params)
{
  GladeWidgetAdaptor *adaptor;
  GArray             *params;
  GObjectClass       *oclass;
  GParamSpec        **pspec;
  GladeProperty      *glade_property;
  GladePropertyClass *pclass;
  guint               n_props, i;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (n_params != NULL, NULL);

  adaptor = widget->priv->adaptor;

  /* As a slight optimization, we never unref the class
   */
  oclass = g_type_class_ref (glade_widget_adaptor_get_object_type (adaptor));
  pspec = g_object_class_list_properties (oclass, &n_props);
  params = g_array_new (FALSE, FALSE, sizeof (GParameter));

  for (i = 0; i < n_props; i++)
    {
      GParameter parameter = { 0, };

      if ((glade_property =
           glade_widget_get_property (widget, pspec[i]->name)) == NULL)
        continue;

      pclass = glade_property_get_class (glade_property);

      /* Ignore properties based on some criteria
       */
      if (!glade_property_get_enabled (glade_property) || 
	  pclass == NULL ||     /* Unaccounted for in the builder */
          glade_property_class_get_virtual (pclass) ||  /* should not be set before 
							   GladeWidget wrapper exists */
          glade_property_class_get_ignore (pclass))     /* Catalog explicitly ignores the object */
        continue;

      if (construct &&
          (pspec[i]->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY)) == 0)
        continue;
      else if (!construct &&
               (pspec[i]->flags &
                (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY)) != 0)
        continue;

      if (g_value_type_compatible (G_VALUE_TYPE (glade_property_class_get_default (pclass)),
                                   pspec[i]->value_type) == FALSE)
        {
          g_critical ("Type mismatch on %s property of %s",
                      parameter.name, 
		      glade_widget_adaptor_get_name (adaptor));
          continue;
        }

      if (g_param_values_cmp (pspec[i],
                              glade_property_inline_value (glade_property), 
			      glade_property_class_get_original_default (pclass)) == 0)
        continue;

      /* Not sure if it's safe to use glade_property_get_value() instead as the 
       * value type might differ than the real pspec 
       */
      parameter.name = pspec[i]->name;  /* These are not copied/freed */
      g_value_init (&parameter.value, pspec[i]->value_type);
      g_value_copy (glade_property_inline_value (glade_property), &parameter.value);

      g_array_append_val (params, parameter);
    }
  g_free (pspec);

  *n_params = params->len;
  return (GParameter *) g_array_free (params, FALSE);
}

static void
free_params (GParameter * params, guint n_params)
{
  gint i;
  for (i = 0; i < n_params; i++)
    g_value_unset (&(params[i].value));
  g_free (params);

}

static GObject *
glade_widget_build_object (GladeWidget * widget,
                           GladeWidget * template, GladeCreateReason reason)
{
  GParameter *params;
  GObject *object;
  guint n_params, i;

  if (reason == GLADE_CREATE_LOAD)
    {
      object = glade_widget_adaptor_construct_object (widget->priv->adaptor, 0, NULL);
      glade_widget_set_object (widget, object, TRUE);
      return object;
    }

  if (template)
    params = glade_widget_template_params (widget, TRUE, &n_params);
  else
    params =
        glade_widget_adaptor_default_params (widget->priv->adaptor, TRUE, &n_params);

  /* Create the new object with the correct parameters.
   */
  object =
      glade_widget_adaptor_construct_object (widget->priv->adaptor, n_params, params);

  free_params (params, n_params);

  /* Dont destroy toplevels when rebuilding, handle that separately */
  glade_widget_set_object (widget, object, reason != GLADE_CREATE_REBUILD);

  if (template)
    params = glade_widget_template_params (widget, FALSE, &n_params);
  else
    params =
        glade_widget_adaptor_default_params (widget->priv->adaptor, FALSE, &n_params);

  for (i = 0; i < n_params; i++)
    glade_widget_adaptor_set_property (widget->priv->adaptor, object, params[i].name,
                                       &(params[i].value));

  free_params (params, n_params);

  return object;
}

/**
 * glade_widget_dup_properties:
 * @dest_widget: the widget we are copying properties for
 * @template_props: the #GladeProperty list to copy
 * @as_load: whether to behave as if loading the project
 * @copy_parentless: whether to copy reffed widgets at all
 * @exact: whether to copy reffed widgets exactly
 *
 * Copies a list of properties, if @as_load is specified, then
 * properties that are not saved to the glade file are ignored.
 *
 * Returns: A newly allocated #GList of new #GladeProperty objects.
 */
GList *
glade_widget_dup_properties (GladeWidget * dest_widget, GList * template_props,
                             gboolean as_load, gboolean copy_parentless,
                             gboolean exact)
{
  GList *list, *properties = NULL;

  for (list = template_props; list && list->data; list = list->next)
    {
      GladeProperty      *prop = list->data;
      GladePropertyClass *pclass = glade_property_get_class (prop);

      if (glade_property_class_save (pclass) == FALSE && as_load)
        continue;

      if (glade_property_class_parentless_widget (pclass) && copy_parentless)
        {
          GObject *object = NULL;
          GladeWidget *parentless;

          glade_property_get (prop, &object);

          prop = glade_property_dup (prop, NULL);

          if (object)
            {
              parentless = glade_widget_get_from_gobject (object);

              parentless = glade_widget_dup (parentless, exact);

              glade_widget_set_project (parentless, dest_widget->priv->project);

              glade_property_set (prop, parentless->priv->object);
            }
        }
      else
        prop = glade_property_dup (prop, NULL);


      properties = g_list_prepend (properties, prop);
    }
  return g_list_reverse (properties);
}

/**
 * glade_widget_remove_property:
 * @widget: A #GladeWidget
 * @id_property: the name of the property
 *
 * Removes the #GladeProperty indicated by @id_property
 * from @widget (this is intended for use in the plugin, to
 * remove properties from composite children that dont make
 * sence to allow the user to specify, notably - properties
 * that are proxied through the composite widget's properties or
 * style properties).
 */
void
glade_widget_remove_property (GladeWidget * widget, const gchar * id_property)
{
  GladeProperty *prop;

  g_return_if_fail (GLADE_IS_WIDGET (widget));
  g_return_if_fail (id_property);

  /* XXX FIXME: currently we arent calling this on packing properties,
   * but doing so could cause crashes because the hash table is not
   * managed properly
   */
  if ((prop = glade_widget_get_property (widget, id_property)) != NULL)
    {
      widget->priv->properties = g_list_remove (widget->priv->properties, prop);
      g_hash_table_remove (widget->priv->props_hash, id_property);
      g_object_unref (prop);
    }
  else
    g_critical ("Couldnt find property %s on widget %s\n",
                id_property, widget->priv->name);
}

static void
glade_widget_set_catalog_defaults (GList * list)
{
  GList *l;
  for (l = list; l && l->data; l = l->next)
    {
      GladeProperty      *prop = l->data;
      GladePropertyClass *klass = glade_property_get_class (prop);
      GParamSpec         *pspec = glade_property_class_get_pspec (klass);

      if (glade_property_equals_value (prop, glade_property_class_get_original_default (klass)) &&
          g_param_values_cmp (pspec, 
			      glade_property_class_get_original_default (klass), 
			      glade_property_class_get_default (klass)))
        glade_property_reset (prop);
    }
}

static void
glade_widget_sync_custom_props (GladeWidget * widget)
{
  GList *l;
  for (l = widget->priv->properties; l && l->data; l = l->next)
    {
      GladeProperty      *prop = GLADE_PROPERTY (l->data);
      GladePropertyClass *pclass = glade_property_get_class (prop);

      if (glade_property_class_get_virtual (pclass) || 
	  glade_property_class_needs_sync (pclass))
        glade_property_sync (prop);
    }
}

static void
glade_widget_sync_packing_props (GladeWidget * widget)
{
  GList *l;
  for (l = widget->priv->packing_properties; l && l->data; l = l->next)
    {
      GladeProperty *prop = GLADE_PROPERTY (l->data);
      glade_property_sync (prop);
    }
}


static GObject *
glade_widget_constructor (GType type,
                          guint n_construct_properties,
                          GObjectConstructParam * construct_properties)
{
  GladeWidget *gwidget;
  GObject *ret_obj;
  GList *properties = NULL, *list;

  ret_obj = G_OBJECT_CLASS (glade_widget_parent_class)->constructor
      (type, n_construct_properties, construct_properties);

  gwidget = GLADE_WIDGET (ret_obj);

  if (gwidget->priv->name == NULL)
    {
      if (gwidget->priv->internal)
        {
          gchar *name_base = g_strdup_printf ("%s-%s",
                                              gwidget->priv->construct_internal,
                                              gwidget->priv->internal);

          if (gwidget->priv->project)
            {
              gwidget->priv->name =
                  glade_project_new_widget_name (gwidget->priv->project,
                                                 gwidget, name_base);
              g_free (name_base);
            }
          else
            gwidget->priv->name = name_base;

        }
      else if (gwidget->priv->project)
        gwidget->priv->name = glade_project_new_widget_name
            (gwidget->priv->project, gwidget, 
	     glade_widget_adaptor_get_generic_name (gwidget->priv->adaptor));
      else
        gwidget->priv->name = 
	  g_strdup (glade_widget_adaptor_get_generic_name (gwidget->priv->adaptor));
    }

  if (gwidget->priv->construct_template)
    {
      properties = glade_widget_dup_properties
          (gwidget, gwidget->priv->construct_template->priv->properties, FALSE, TRUE,
           gwidget->priv->construct_exact);

      glade_widget_set_properties (gwidget, properties);
    }

  if (gwidget->priv->object == NULL)
    glade_widget_build_object (gwidget,
                               gwidget->priv->construct_template,
                               gwidget->priv->construct_reason);

  /* Copy sync parentless widget props here after a dup
   */
  if (gwidget->priv->construct_reason == GLADE_CREATE_COPY)
    {
      for (list = gwidget->priv->properties; list; list = list->next)
        {
          GladeProperty      *property = list->data;
	  GladePropertyClass *pclass = glade_property_get_class (property);

          if (glade_property_class_parentless_widget (pclass))
            glade_property_sync (property);
        }
    }

  /* Setup width/height */
  gwidget->priv->width = GWA_DEFAULT_WIDTH (gwidget->priv->adaptor);
  gwidget->priv->height = GWA_DEFAULT_HEIGHT (gwidget->priv->adaptor);

  /* Introspect object properties before passing it to post_create,
   * but only when its freshly created (depend on glade file at
   * load time and copying properties at dup time).
   */
  if (gwidget->priv->construct_reason == GLADE_CREATE_USER)
    for (list = gwidget->priv->properties; list; list = list->next)
      glade_property_load (GLADE_PROPERTY (list->data));

  /* We only use catalog defaults when the widget was created by the user!
   * and or is not an internal widget.
   */
  if (gwidget->priv->construct_reason == GLADE_CREATE_USER &&
      gwidget->priv->internal == NULL)
    glade_widget_set_catalog_defaults (gwidget->priv->properties);

  /* Only call this once the GladeWidget is completely built
   * (but before calling custom handlers...)
   */
  glade_widget_adaptor_post_create (gwidget->priv->adaptor,
                                    gwidget->priv->object, gwidget->priv->construct_reason);

  /* Virtual properties need to be explicitly synchronized.
   */
  if (gwidget->priv->construct_reason == GLADE_CREATE_USER)
    glade_widget_sync_custom_props (gwidget);

  if (gwidget->priv->parent && gwidget->priv->packing_properties == NULL)
    glade_widget_set_packing_properties (gwidget, gwidget->priv->parent);

  if (GTK_IS_WIDGET (gwidget->priv->object) &&
      !gtk_widget_is_toplevel (GTK_WIDGET (gwidget->priv->object)))
    {
      gwidget->priv->visible = TRUE;
      gtk_widget_show_all (GTK_WIDGET (gwidget->priv->object));
    }
  else if (GTK_IS_WIDGET (gwidget->priv->object) == FALSE)
    gwidget->priv->visible = TRUE;

  return ret_obj;
}

static void
glade_widget_finalize (GObject * object)
{
  GladeWidget *widget = GLADE_WIDGET (object);

  g_return_if_fail (GLADE_IS_WIDGET (object));

#if 0
  /* A good way to check if refcounts are balancing at project close time */
  g_print ("Finalizing widget %s\n", widget->priv->name);
#endif

  g_free (widget->priv->name);
  g_free (widget->priv->internal);
  g_free (widget->priv->construct_internal);
  g_free (widget->priv->support_warning);
  g_hash_table_destroy (widget->priv->signals);

  if (widget->priv->props_hash)
    g_hash_table_destroy (widget->priv->props_hash);
  if (widget->priv->pack_props_hash)
    g_hash_table_destroy (widget->priv->pack_props_hash);

  G_OBJECT_CLASS (glade_widget_parent_class)->finalize (object);
}

static void
reset_object_property (GladeProperty * property, GladeProject * project)
{
  GladePropertyClass *pclass = glade_property_get_class (property);

  if (glade_property_class_is_object (pclass))
    glade_property_reset (property);
}

static void
glade_widget_dispose (GObject * object)
{
  GladeWidget *widget = GLADE_WIDGET (object);

  glade_widget_push_superuser ();

  /* Release references by way of object properties... */
  while (widget->priv->prop_refs)
    {
      GladeProperty *property = GLADE_PROPERTY (widget->priv->prop_refs->data);
      glade_property_set (property, NULL);
    }

  if (widget->priv->properties)
    g_list_foreach (widget->priv->properties, (GFunc) reset_object_property,
                    widget->priv->project);

  /* We have to make sure properties release thier references on other widgets first 
   * hence the reset (for object properties) */
  if (widget->priv->properties)
    {
      g_list_foreach (widget->priv->properties, (GFunc) g_object_unref, NULL);
      g_list_free (widget->priv->properties);
      widget->priv->properties = NULL;
    }

  glade_widget_set_object (widget, NULL, TRUE);

  if (widget->priv->packing_properties)
    {
      g_list_foreach (widget->priv->packing_properties, (GFunc) g_object_unref, NULL);
      g_list_free (widget->priv->packing_properties);
      widget->priv->packing_properties = NULL;
    }

  if (widget->priv->actions)
    {
      g_list_foreach (widget->priv->actions, (GFunc) g_object_unref, NULL);
      g_list_free (widget->priv->actions);
      widget->priv->actions = NULL;
    }

  if (widget->priv->packing_actions)
    {
      g_list_foreach (widget->priv->packing_actions, (GFunc) g_object_unref, NULL);
      g_list_free (widget->priv->packing_actions);
      widget->priv->packing_actions = NULL;
    }

  if (widget->priv->signal_model)
    {
      g_object_unref (widget->priv->signal_model);
      widget->priv->signal_model = NULL;
    }

  glade_widget_pop_superuser ();

  G_OBJECT_CLASS (glade_widget_parent_class)->dispose (object);
}

static void
glade_widget_set_real_property (GObject * object,
                                guint prop_id,
                                const GValue * value, GParamSpec * pspec)
{
  GladeWidget *widget;

  widget = GLADE_WIDGET (object);

  switch (prop_id)
    {
      case PROP_NAME:
        glade_widget_set_name (widget, g_value_get_string (value));
        break;
      case PROP_INTERNAL:
        glade_widget_set_internal (widget, g_value_get_string (value));
        break;
      case PROP_ANARCHIST:
        widget->priv->anarchist = g_value_get_boolean (value);
        break;
      case PROP_OBJECT:
        if (g_value_get_object (value))
          glade_widget_set_object (widget, g_value_get_object (value), TRUE);
        break;
      case PROP_PROJECT:
        glade_widget_set_project (widget,
                                  GLADE_PROJECT (g_value_get_object (value)));
        break;
      case PROP_ADAPTOR:
        glade_widget_set_adaptor (widget, GLADE_WIDGET_ADAPTOR
                                  (g_value_get_object (value)));
        break;
      case PROP_PROPERTIES:
        glade_widget_set_properties (widget,
                                     (GList *) g_value_get_pointer (value));
        break;
      case PROP_PARENT:
        glade_widget_set_parent (widget,
                                 GLADE_WIDGET (g_value_get_object (value)));
        break;
      case PROP_INTERNAL_NAME:
        if (g_value_get_string (value))
          widget->priv->construct_internal = g_value_dup_string (value);
        break;
      case PROP_TEMPLATE:
        widget->priv->construct_template = g_value_get_object (value);
        break;
      case PROP_TEMPLATE_EXACT:
        widget->priv->construct_exact = g_value_get_boolean (value);
        break;
      case PROP_REASON:
        widget->priv->construct_reason = g_value_get_int (value);
        break;
      case PROP_TOPLEVEL_WIDTH:
        widget->priv->width = g_value_get_int (value);
        break;
      case PROP_TOPLEVEL_HEIGHT:
        widget->priv->height = g_value_get_int (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_widget_get_real_property (GObject * object,
                                guint prop_id,
                                GValue * value, GParamSpec * pspec)
{
  GladeWidget *widget;

  widget = GLADE_WIDGET (object);

  switch (prop_id)
    {
      case PROP_NAME:
        g_value_set_string (value, widget->priv->name);
        break;
      case PROP_INTERNAL:
        g_value_set_string (value, widget->priv->internal);
        break;
      case PROP_ANARCHIST:
        g_value_set_boolean (value, widget->priv->anarchist);
        break;
      case PROP_ADAPTOR:
        g_value_set_object (value, widget->priv->adaptor);
        break;
      case PROP_PROJECT:
        g_value_set_object (value, G_OBJECT (widget->priv->project));
        break;
      case PROP_OBJECT:
        g_value_set_object (value, widget->priv->object);
        break;
      case PROP_PROPERTIES:
        g_value_set_pointer (value, widget->priv->properties);
        break;
      case PROP_PARENT:
        g_value_set_object (value, widget->priv->parent);
        break;
      case PROP_TOPLEVEL_WIDTH:
        g_value_set_int (value, widget->priv->width);
        break;
      case PROP_TOPLEVEL_HEIGHT:
        g_value_set_int (value, widget->priv->height);
        break;
      case PROP_SUPPORT_WARNING:
        g_value_set_string (value, widget->priv->support_warning);
        break;
      case PROP_VISIBLE:
        g_value_set_boolean (value, widget->priv->visible);
        break;
      case PROP_REASON:
        g_value_set_int (value, widget->priv->construct_reason);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
free_signals (GPtrArray *signals)
{
  g_assert (signals);
  g_ptr_array_free (signals, TRUE);
}

static void
glade_widget_init (GladeWidget * widget)
{
  widget->priv = G_TYPE_INSTANCE_GET_PRIVATE (widget,
                                              GLADE_TYPE_WIDGET,
                                              GladeWidgetPrivate);

  widget->priv->adaptor = NULL;
  widget->priv->project = NULL;
  widget->priv->name = NULL;
  widget->priv->internal = NULL;
  widget->priv->object = NULL;
  widget->priv->properties = NULL;
  widget->priv->packing_properties = NULL;
  widget->priv->prop_refs = NULL;
  widget->priv->signals = g_hash_table_new_full
      (g_str_hash, g_str_equal,
       (GDestroyNotify) g_free, (GDestroyNotify) free_signals);

  /* Initial invalid values */
  widget->priv->width = -1;
  widget->priv->height = -1;
}

static void
glade_widget_class_init (GladeWidgetClass * klass)
{
  GObjectClass *object_class;

  if (glade_widget_name_quark == 0)
    glade_widget_name_quark = g_quark_from_static_string ("GladeWidgetDataTag");

  object_class = G_OBJECT_CLASS (klass);

  object_class->constructor = glade_widget_constructor;
  object_class->finalize = glade_widget_finalize;
  object_class->dispose = glade_widget_dispose;
  object_class->set_property = glade_widget_set_real_property;
  object_class->get_property = glade_widget_get_real_property;

  klass->add_child = glade_widget_add_child_impl;
  klass->remove_child = glade_widget_remove_child_impl;
  klass->replace_child = glade_widget_replace_child_impl;
  klass->event = glade_widget_event_impl;

  klass->button_press_event = glade_widget_button_press_event_impl;
  klass->button_release_event = NULL;
  klass->motion_notify_event = NULL;

  properties[PROP_NAME] =
       g_param_spec_string ("name", _("Name"),
                            _("The name of the widget"),
                            NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  properties[PROP_INTERNAL] =
       g_param_spec_string ("internal", _("Internal name"),
                            _("The internal name of the widget"),
                            NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  properties[PROP_ANARCHIST] =
       g_param_spec_boolean ("anarchist", _("Anarchist"),
                             _("Whether this composite child is "
                               "an ancestral child or an anarchist child"),
                             FALSE, G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_OBJECT] =
       g_param_spec_object ("object", _("Object"),
                            _("The object associated"),
                            G_TYPE_OBJECT,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  properties[PROP_ADAPTOR] =
       g_param_spec_object ("adaptor", _("Adaptor"),
                            _("The class adaptor for the associated widget"),
                            GLADE_TYPE_WIDGET_ADAPTOR,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_PROJECT] =
       g_param_spec_object ("project", _("Project"),
                            _("The glade project that "
                              "this widget belongs to"),
                            GLADE_TYPE_PROJECT,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  properties[PROP_PROPERTIES] =
       g_param_spec_pointer ("properties", _("Properties"),
                             _("A list of GladeProperties"),
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_PARENT] =
       g_param_spec_object ("parent", _("Parent"),
                            _("A pointer to the parenting GladeWidget"),
                            GLADE_TYPE_WIDGET,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  properties[PROP_INTERNAL_NAME] =
       g_param_spec_string ("internal-name", _("Internal Name"),
                            _("A generic name prefix for internal widgets"),
                            NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE);

  properties[PROP_TEMPLATE] =
       g_param_spec_object ("template", _("Template"),
                            _("A GladeWidget template to base a new widget on"),
                            GLADE_TYPE_WIDGET,
                            G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE);

  properties[PROP_TEMPLATE_EXACT] =
       g_param_spec_boolean ("template-exact", _("Exact Template"),
                             _
                             ("Whether we are creating an exact duplicate when using a template"),
                             FALSE, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_REASON] =
       g_param_spec_int ("reason", _("Reason"),
                         _("A GladeCreateReason for this creation"),
                         GLADE_CREATE_USER,
                         GLADE_CREATE_REASONS - 1,
                         GLADE_CREATE_USER,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  properties[PROP_TOPLEVEL_WIDTH] =
       g_param_spec_int ("toplevel-width", _("Toplevel Width"),
                         _("The width of the widget when toplevel in "
                           "the GladeDesignLayout"),
                         -1, G_MAXINT, -1, G_PARAM_READWRITE);

  properties[PROP_TOPLEVEL_HEIGHT] =
       g_param_spec_int ("toplevel-height", _("Toplevel Height"),
                         _("The height of the widget when toplevel in "
                           "the GladeDesignLayout"),
                         -1, G_MAXINT, -1, G_PARAM_READWRITE);

  properties[PROP_SUPPORT_WARNING] =
       g_param_spec_string ("support warning", _("Support Warning"),
                            _("A warning string about version mismatches"),
                            NULL, G_PARAM_READABLE);

  properties[PROP_VISIBLE] =
       g_param_spec_boolean ("visible", _("Visible"),
                            _("Wether the widget is visible or not"),
                             FALSE, G_PARAM_READABLE);

  /* Install all properties */
  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * GladeWidget::add-signal-handler:
   * @gladewidget: the #GladeWidget which received the signal.
   * @arg1: the #GladeSignal that was added to @gladewidget.
   */
  glade_widget_signals[ADD_SIGNAL_HANDLER] =
      g_signal_new ("add-signal-handler",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GladeWidgetClass, add_signal_handler),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__OBJECT,
                    G_TYPE_NONE, 1, GLADE_TYPE_SIGNAL);

  /**
   * GladeWidget::remove-signal-handler:
   * @gladewidget: the #GladeWidget which received the signal.
   * @arg1: the #GladeSignal that was removed from @gladewidget.
	 */
  glade_widget_signals[REMOVE_SIGNAL_HANDLER] =
      g_signal_new ("remove-signal-handler",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GladeWidgetClass, remove_signal_handler),
                     NULL, NULL,
                    g_cclosure_marshal_VOID__OBJECT,
                    G_TYPE_NONE, 1, GLADE_TYPE_SIGNAL);

  /**
   * GladeWidget::change-signal-handler:
   * @gladewidget: the #GladeWidget which received the signal.
   * @arg1: the old #GladeSignal
   * @arg2: the new #GladeSignal
   */
  glade_widget_signals[CHANGE_SIGNAL_HANDLER] =
      g_signal_new ("change-signal-handler",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GladeWidgetClass, change_signal_handler),
                     NULL, NULL,
                    _glade_marshal_VOID__OBJECT,
                    G_TYPE_NONE, 1, GLADE_TYPE_SIGNAL);

  /**
   * GladeWidget::button-press-event:
   * @gladewidget: the #GladeWidget which received the signal.
   * @arg1: the #GdkEvent
   */
  glade_widget_signals[BUTTON_PRESS_EVENT] =
      g_signal_new ("button-press-event",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GladeWidgetClass, button_press_event),
                    _glade_boolean_handled_accumulator, NULL,
                    _glade_marshal_BOOLEAN__BOXED,
                    G_TYPE_BOOLEAN, 1,
                    GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GladeWidget::button-relese-event:
   * @gladewidget: the #GladeWidget which received the signal.
   * @arg1: the #GdkEvent
   */
  glade_widget_signals[BUTTON_RELEASE_EVENT] =
      g_signal_new ("button-release-event",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GladeWidgetClass, button_release_event),
                    _glade_boolean_handled_accumulator, NULL,
                    _glade_marshal_BOOLEAN__BOXED,
                    G_TYPE_BOOLEAN, 1,
                    GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);


  /**
   * GladeWidget::motion-notify-event:
   * @gladewidget: the #GladeWidget which received the signal.
   * @arg1: the #GdkEvent
   */
  glade_widget_signals[MOTION_NOTIFY_EVENT] =
      g_signal_new ("motion-notify-event",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GladeWidgetClass, motion_notify_event),
                    _glade_boolean_handled_accumulator, NULL,
                    _glade_marshal_BOOLEAN__BOXED,
                    G_TYPE_BOOLEAN, 1,
                    GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);


  /**
   * GladeWidget::support-changed:
   * @gladewidget: the #GladeWidget which received the signal.
   *
   * Emitted when property and signal support metadatas and messages
   * have been updated.
   */
  glade_widget_signals[SUPPORT_CHANGED] =
      g_signal_new ("support-changed",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    0, NULL, NULL,
                    g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  g_type_class_add_private (klass, sizeof (GladeWidgetPrivate));
}

/*******************************************************************************
                                Static stuff....
 *******************************************************************************/
static void
glade_widget_copy_packing_props (GladeWidget * parent,
                                 GladeWidget * child,
                                 GladeWidget * template_widget)
{
  GladeProperty *dup_prop, *orig_prop;
  GList *l;

  g_return_if_fail (child->priv->parent == parent);

  glade_widget_set_packing_properties (child, parent);

  for (l = child->priv->packing_properties; l && l->data; l = l->next)
    {
      GladePropertyClass *pclass;

      dup_prop  = GLADE_PROPERTY (l->data);
      pclass    = glade_property_get_class (dup_prop);
      orig_prop = glade_widget_get_pack_property (template_widget, glade_property_class_id (pclass));

      glade_property_set_value (dup_prop, glade_property_inline_value (orig_prop));
    }
}

static void
glade_widget_set_default_packing_properties (GladeWidget * container,
                                             GladeWidget * child)
{
  GladePropertyClass *property_class;
  const GList *l;

  for (l = glade_widget_adaptor_get_packing_props (container->priv->adaptor); l; l = l->next)
    {
      const gchar *def;
      GValue *value;

      property_class = l->data;

      if ((def =
           glade_widget_adaptor_get_packing_default
           (child->priv->adaptor, container->priv->adaptor, 
	    glade_property_class_id (property_class))) == NULL)
        continue;

      value = glade_property_class_make_gvalue_from_string (property_class,
                                                            def, child->priv->project);

      glade_widget_child_set_property (container, child,
                                       glade_property_class_id (property_class), value);
      g_value_unset (value);
      g_free (value);
    }
}

/**
 * When looking for an internal child we have to walk up the hierarchy...
 */
static GObject *
glade_widget_get_internal_child (GladeWidget *main_target, 
				 GladeWidget *parent, 
				 const gchar *internal)
{
  while (parent)
    {
      if (glade_widget_adaptor_has_internal_children (parent->priv->adaptor))
        return glade_widget_adaptor_get_internal_child
            (parent->priv->adaptor, parent->priv->object, internal);

      /* Limit the itterations into where the copy routine stared */
      if (parent == main_target)
        break;

      parent = glade_widget_get_parent (parent);
    }
  return NULL;
}

static GladeWidget *
glade_widget_dup_internal (GladeWidget * main_target,
                           GladeWidget * parent,
                           GladeWidget * template_widget, gboolean exact)
{
  GladeWidget *gwidget = NULL;
  GList *children;
  GtkWidget *placeholder;
  gchar *child_type;
  GList *l;

  g_return_val_if_fail (GLADE_IS_WIDGET (template_widget), NULL);
  g_return_val_if_fail (parent == NULL || GLADE_IS_WIDGET (parent), NULL);

  /* Dont actually duplicate internal widgets, but recurse through them anyway. */
  if (parent && template_widget->priv->internal)
    {
      GObject *internal_object = NULL;

      if ((internal_object = 
	   glade_widget_get_internal_child (main_target,
					    parent,
					    template_widget->priv->internal)) != NULL)
	{
	  gwidget = glade_widget_get_from_gobject (internal_object);
	  g_assert (gwidget);
        }
    }

  /* If either it was not internal, or we failed to lookup the internal child
   * in the copied hierarchy (this can happen when copying an internal vbox from
   * a composite dialog for instance). */
  if (gwidget == NULL)
    {
      gchar *name = g_strdup (template_widget->priv->name);
      gwidget = glade_widget_adaptor_create_widget
          (template_widget->priv->adaptor, FALSE,
           "name", name,
           "parent", parent,
           "project", template_widget->priv->project,
           "template", template_widget,
           "template-exact", exact, "reason", GLADE_CREATE_COPY, NULL);
      g_free (name);
    }

  /* Copy signals over here regardless of internal or not... */
  if (exact)
    glade_widget_copy_signals (gwidget, template_widget);

  if ((children =
       glade_widget_adaptor_get_children (template_widget->priv->adaptor,
                                          template_widget->priv->object)) != NULL)
    {
      GList *list;

      for (list = children; list && list->data; list = list->next)
        {
          GObject *child = G_OBJECT (list->data);
          GladeWidget *child_gwidget, *child_dup;

          child_type = g_object_get_data (child, "special-child-type");

          if ((child_gwidget = glade_widget_get_from_gobject (child)) == NULL)
            {
              /* Bring the placeholders along ...
               * but not unmarked internal children */
              if (GLADE_IS_PLACEHOLDER (child))
                {
                  placeholder = glade_placeholder_new ();

                  g_object_set_data_full (G_OBJECT (placeholder),
                                          "special-child-type",
                                          g_strdup (child_type), g_free);

                  glade_widget_adaptor_add (gwidget->priv->adaptor,
                                            gwidget->priv->object,
                                            G_OBJECT (placeholder));
                }
            }
          else
            {
              /* Recurse through every GladeWidget (internal or not) */
              child_dup =
                  glade_widget_dup_internal (main_target, gwidget,
                                             child_gwidget, exact);

              if (child_dup->priv->internal == NULL)
                {
                  g_object_set_data_full (child_dup->priv->object,
                                          "special-child-type",
                                          g_strdup (child_type), g_free);

                  glade_widget_add_child (gwidget, child_dup, FALSE);
                }

              /* Internal children that are not heirarchic children
               * need to avoid copying these packing props (like popup windows
               * created on behalf of composite widgets).
               */
              if (glade_widget_adaptor_has_child (gwidget->priv->adaptor,
                                                  gwidget->priv->object,
                                                  child_dup->priv->object))
                glade_widget_copy_packing_props (gwidget, child_dup, child_gwidget);

            }
        }
      g_list_free (children);
    }

  if (gwidget->priv->internal)
    glade_widget_copy_properties (gwidget, template_widget, TRUE, exact);

  if (gwidget->priv->packing_properties == NULL)
    gwidget->priv->packing_properties =
        glade_widget_dup_properties (gwidget,
                                     template_widget->priv->packing_properties, FALSE,
                                     FALSE, FALSE);

  /* If custom properties are still at thier
   * default value, they need to be synced.
   */
  glade_widget_sync_custom_props (gwidget);

  /* Some properties may not be synced so we reload them */
  for (l = gwidget->priv->properties; l; l = l->next)
    glade_property_load (GLADE_PROPERTY (l->data));

  if (GWA_IS_TOPLEVEL (gwidget->priv->adaptor) && GTK_IS_WIDGET (gwidget->priv->object))
    g_object_set (gwidget,
                  "toplevel-width", template_widget->priv->width,
                  "toplevel-height", template_widget->priv->height, NULL);
  return gwidget;
}


typedef struct
{
  GladeWidget *widget;
  GtkWidget *placeholder;
  GList *properties;

  gchar *internal_name;
  GList *internal_list;
} GladeChildExtract;

static GList *
glade_widget_extract_children (GladeWidget * gwidget)
{
  GladeChildExtract *extract;
  GList *extract_list = NULL;
  GList *children, *list;

  children = glade_widget_adaptor_get_children
      (gwidget->priv->adaptor, gwidget->priv->object);

  for (list = children; list && list->data; list = list->next)
    {
      GObject *child = G_OBJECT (list->data);
      GladeWidget *gchild = glade_widget_get_from_gobject (child);
#if 0
      g_print ("Extracting %s from %s\n",
               gchild ? gchild->name :
               GLADE_IS_PLACEHOLDER (child) ? "placeholder" : "unknown widget",
               gwidget->priv->name);
#endif
      if (gchild && gchild->priv->internal)
        {
          /* Recurse and collect any deep child hierarchies
           * inside composite widgets.
           */
          extract = g_new0 (GladeChildExtract, 1);
          extract->internal_name = g_strdup (gchild->priv->internal);
          extract->internal_list = glade_widget_extract_children (gchild);
          extract->properties =
              glade_widget_dup_properties (gchild, gchild->priv->properties, TRUE,
                                           FALSE, FALSE);

          extract_list = g_list_prepend (extract_list, extract);

        }
      else if (gchild || GLADE_IS_PLACEHOLDER (child))
        {
          extract = g_new0 (GladeChildExtract, 1);

          if (gchild)
            {
              extract->widget = g_object_ref (gchild);

              /* Make copies of the packing properties
               */
              extract->properties =
                  glade_widget_dup_properties
                  (gchild, gchild->priv->packing_properties, TRUE, FALSE, FALSE);

              glade_widget_remove_child (gwidget, gchild);
            }
          else
            {
              /* need to handle placeholders by hand here */
              extract->placeholder = g_object_ref (child);
              glade_widget_adaptor_remove (gwidget->priv->adaptor,
                                           gwidget->priv->object, child);
            }
          extract_list = g_list_prepend (extract_list, extract);
        }
    }

  if (children)
    g_list_free (children);

  return g_list_reverse (extract_list);
}

static void
glade_widget_insert_children (GladeWidget * gwidget, GList * children)
{
  GladeChildExtract *extract;
  GladeWidget *gchild;
  GObject *internal_object;
  GList *list, *l;

  for (list = children; list; list = list->next)
    {
      extract = list->data;

      if (extract->internal_name)
        {

          /* Recurse and add deep widget hierarchies to internal
           * widgets.
           */
          internal_object = glade_widget_get_internal_child (NULL,
							     gwidget,
							     extract->internal_name);

          gchild = glade_widget_get_from_gobject (internal_object);

          /* This will free the list... */
          glade_widget_insert_children (gchild, extract->internal_list);

          /* Set the properties after inserting the children */
          for (l = extract->properties; l; l = l->next)
            {
              GValue              value = { 0, };
              GladeProperty      *saved_prop = l->data;
	      GladePropertyClass *pclass = glade_property_get_class (saved_prop);
              GladeProperty      *widget_prop = 
		glade_widget_get_property (gchild, glade_property_class_id (pclass));

              glade_property_get_value (saved_prop, &value);
              glade_property_set_value (widget_prop, &value);
              g_value_unset (&value);

              /* Free them as we go ... */
              g_object_unref (saved_prop);
            }

          if (extract->properties)
            g_list_free (extract->properties);

          g_free (extract->internal_name);
        }
      else if (extract->widget)
        {
          glade_widget_add_child (gwidget, extract->widget, FALSE);
          g_object_unref (extract->widget);

          for (l = extract->properties; l; l = l->next)
            {
              GValue              value = { 0, };
              GladeProperty      *saved_prop = l->data;
	      GladePropertyClass *pclass = glade_property_get_class (saved_prop);
              GladeProperty      *widget_prop =
		glade_widget_get_pack_property (extract->widget, glade_property_class_id (pclass));

              glade_property_get_value (saved_prop, &value);
              glade_property_set_value (widget_prop, &value);
              g_value_unset (&value);

              /* Free them as we go ... */
              g_object_unref (saved_prop);
            }
          if (extract->properties)
            g_list_free (extract->properties);
        }
      else
        {
          glade_widget_adaptor_add (gwidget->priv->adaptor,
                                    gwidget->priv->object,
                                    G_OBJECT (extract->placeholder));
          g_object_unref (extract->placeholder);
        }
      g_free (extract);
    }

  if (children)
    g_list_free (children);
}

static void
glade_widget_set_properties (GladeWidget * widget, GList * properties)
{
  GladeProperty *property;
  GList *list;

  if (properties)
    {
      if (widget->priv->properties)
        {
          g_list_foreach (widget->priv->properties, (GFunc) g_object_unref, NULL);
          g_list_free (widget->priv->properties);
        }
      if (widget->priv->props_hash)
        g_hash_table_destroy (widget->priv->props_hash);

      widget->priv->properties = properties;
      widget->priv->props_hash = g_hash_table_new (g_str_hash, g_str_equal);

      for (list = properties; list; list = list->next)
        {
	  GladePropertyClass *pclass;

          property = list->data;

	  pclass = glade_property_get_class (property);
	  glade_property_set_widget (property, widget);

          g_hash_table_insert (widget->priv->props_hash, 
			       (gchar *)glade_property_class_id (pclass), 
			       property);
        }
    }
}

static void
glade_widget_set_adaptor (GladeWidget * widget, GladeWidgetAdaptor * adaptor)
{
  GladePropertyClass *property_class;
  GladeProperty      *property;
  const GList        *list;
  GList              *properties = NULL;

  g_return_if_fail (GLADE_IS_WIDGET (widget));
  g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
  /* calling set_class out of the constructor? */
  g_return_if_fail (widget->priv->adaptor == NULL);

  widget->priv->adaptor = adaptor;

  /* If we have no properties; we are not in the process of loading
   */
  if (!widget->priv->properties)
    {
      for (list = glade_widget_adaptor_get_properties (adaptor); list; list = list->next)
        {
          property_class = GLADE_PROPERTY_CLASS (list->data);
          if ((property = glade_property_new (property_class,
                                              widget, NULL)) == NULL)
            {
              g_warning ("Failed to create [%s] property", 
			 glade_property_class_id (property_class));
              continue;
            }
          properties = g_list_prepend (properties, property);
        }
      glade_widget_set_properties (widget, g_list_reverse (properties));
    }

  /* Create actions from adaptor */
  widget->priv->actions = glade_widget_adaptor_actions_new (adaptor);
}

/*
 * Returns a list of GladeProperties from a list for the correct
 * child type for this widget of this container.
 */
static GList *
glade_widget_create_packing_properties (GladeWidget * container,
                                        GladeWidget * widget)
{
  GladePropertyClass *property_class;
  GladeProperty      *property;
  const GList        *list; 
  GList              *packing_props = NULL;

  /* XXX TODO: by checking with some GladePropertyClass metadata, decide
   * which packing properties go on which type of children.
   */
  for (list = glade_widget_adaptor_get_packing_props (container->priv->adaptor);
       list && list->data; list = list->next)
    {
      property_class = list->data;
      property = glade_property_new (property_class, widget, NULL);
      packing_props = g_list_prepend (packing_props, property);

    }
  return g_list_reverse (packing_props);
}

/*******************************************************************************
                                     API
 *******************************************************************************/
GladeWidget *
glade_widget_get_from_gobject (gpointer object)
{
  g_return_val_if_fail (G_IS_OBJECT (object), NULL);

  return g_object_get_qdata (G_OBJECT (object), glade_widget_name_quark);
}

/**
 * glade_widget_show:
 * @widget: A #GladeWidget
 *
 * Display @widget in it's project's GladeDesignView
 */
void
glade_widget_show (GladeWidget *widget)
{
  GladeProperty *property;
  GladeProject *project;

  g_return_if_fail (GLADE_IS_WIDGET (widget));

  /* Position window at saved coordinates or in the center */
  if (GTK_IS_WIDGET (widget->priv->object) && !widget->priv->parent)
    {
      /* Maybe a property references this widget internally, show that widget instead */
      if ((property = glade_widget_get_parentless_widget_ref (widget)) != NULL)
        {
          /* will never happen, paranoid check to avoid endless recursion. */
          if (glade_property_get_widget (property) != widget)
            glade_widget_show (glade_property_get_widget (property));
          return;
        }
    }
  else if (GTK_IS_WIDGET (widget->priv->object))
    {
      GladeWidget *toplevel = glade_widget_get_toplevel (widget);
      if (toplevel != widget)
        glade_widget_show (toplevel);
    }

  if (widget->priv->visible) return;
  
  widget->priv->visible = TRUE;
  if ((project = glade_widget_get_project (widget)))
    glade_project_widget_visibility_changed (project, widget, TRUE);
}

/**
 * glade_widget_hide:
 * @widget: A #GladeWidget
 *
 * Hide @widget
 */
void
glade_widget_hide (GladeWidget *widget)
{
  GladeProject *project;

  g_return_if_fail (GLADE_IS_WIDGET (widget));

  if (!widget->priv->visible) return;
  
  widget->priv->visible = FALSE;
  if ((project = glade_widget_get_project (widget)))
    glade_project_widget_visibility_changed (project, widget, FALSE);
}

/**
 * glade_widget_add_prop_ref:
 * @widget: A #GladeWidget
 * @property: the #GladeProperty
 *
 * Adds @property to @widget 's list of referenced properties.
 *
 * Note: this is used to track properties on other objects that
 *       reffer to this object.
 */
void
glade_widget_add_prop_ref (GladeWidget * widget, GladeProperty * property)
{
  GladePropertyClass *pclass;

  g_return_if_fail (GLADE_IS_WIDGET (widget));
  g_return_if_fail (GLADE_IS_PROPERTY (property));

  if (!g_list_find (widget->priv->prop_refs, property))
    widget->priv->prop_refs = g_list_prepend (widget->priv->prop_refs, property);

  /* parentless widget reffed widgets are added to thier reffering widgets. 
   * they cant be in the design view.
   */
  pclass = glade_property_get_class (property);
  if (glade_property_class_parentless_widget (pclass))
    {
      GladeProject *project = glade_widget_get_project (widget);

      if (project)
	glade_project_widget_changed (project, widget);

      glade_widget_hide (widget);
    }
}

/**
 * glade_widget_remove_prop_ref:
 * @widget: A #GladeWidget
 * @property: the #GladeProperty
 *
 * Removes @property from @widget 's list of referenced properties.
 *
 * Note: this is used to track properties on other objects that
 *       reffer to this object.
 */
void
glade_widget_remove_prop_ref (GladeWidget * widget, GladeProperty * property)
{
  GladePropertyClass *pclass;

  g_return_if_fail (GLADE_IS_WIDGET (widget));
  g_return_if_fail (GLADE_IS_PROPERTY (property));

  widget->priv->prop_refs = g_list_remove (widget->priv->prop_refs, property);

  pclass = glade_property_get_class (property);
  if (glade_property_class_parentless_widget (pclass))
    {
      GladeProject *project = glade_widget_get_project (widget);

      if (project)
	glade_project_widget_changed (project, widget);
    }
}

GList *
glade_widget_list_prop_refs (GladeWidget      *widget)
{
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

  return g_list_copy (widget->priv->prop_refs);
}


GladeProperty *
glade_widget_get_parentless_widget_ref (GladeWidget * widget)
{
  GladePropertyClass *pclass;
  GladeProperty      *property;
  GList              *l;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

  for (l = widget->priv->prop_refs; l && l->data; l = l->next)
    {
      property = l->data;
      pclass   = glade_property_get_class (property);

      if (glade_property_class_parentless_widget (pclass))
        /* only one external property can point to this widget */
        return property;
    }
  return NULL;
}


GList *
glade_widget_get_parentless_reffed_widgets (GladeWidget * widget)
{
  GladeProperty      *property = NULL;
  GladePropertyClass *pclass;
  GObject            *reffed = NULL;
  GList              *l, *widgets = NULL;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

  for (l = widget->priv->properties; l && l->data; l = l->next)
    {
      property = l->data;
      pclass   = glade_property_get_class (property);
      reffed = NULL;

      if (glade_property_class_parentless_widget (pclass))
        {
          glade_property_get (property, &reffed);
          if (reffed)
            widgets =
                g_list_prepend (widgets,
                                glade_widget_get_from_gobject (reffed));
        }
    }
  return g_list_reverse (widgets);
}

static void
glade_widget_accum_signal_foreach (const gchar * key,
                                   GPtrArray * signals, GList ** list)
{
  GladeSignal *signal;
  gint i;

  for (i = 0; i < signals->len; i++)
    {
      signal = (GladeSignal *) signals->pdata[i];
      *list = g_list_append (*list, signal);
    }
}

/**
 * glade_widget_get_signal_list:
 * @widget:   a 'dest' #GladeWidget
 *
 * Compiles a list of #GladeSignal elements
 *
 * Returns: a newly allocated #GList of #GladeSignals, the caller
 * must call g_list_free() to free the list.
 */
GList *
glade_widget_get_signal_list (GladeWidget * widget)
{
  GList *signals = NULL;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

  g_hash_table_foreach (widget->priv->signals,
                        (GHFunc) glade_widget_accum_signal_foreach, &signals);

  return signals;
}

static void
glade_widget_copy_signal_foreach (const gchar * key,
                                  GPtrArray * signals, GladeWidget * dest)
{
  GladeSignal *signal;
  gint i;

  for (i = 0; i < signals->len; i++)
    {
      signal = (GladeSignal *) signals->pdata[i];
      glade_widget_add_signal_handler (dest, signal);
    }
}

/**
 * glade_widget_copy_signals:
 * @widget:   a 'dest' #GladeWidget
 * @template_widget: a 'src' #GladeWidget
 *
 * Sets signals in @widget based on the values of
 * matching signals in @template_widget
 */
void
glade_widget_copy_signals (GladeWidget * widget, GladeWidget * template_widget)
{
  g_return_if_fail (GLADE_IS_WIDGET (widget));
  g_return_if_fail (GLADE_IS_WIDGET (template_widget));

  g_hash_table_foreach (template_widget->priv->signals,
                        (GHFunc) glade_widget_copy_signal_foreach, widget);
}

/**
 * glade_widget_copy_properties:
 * @widget:   a 'dest' #GladeWidget
 * @template_widget: a 'src' #GladeWidget
 * @copy_parentless: whether to copy reffed widgets at all
 * @exact: whether to copy reffed widgets exactly
 *
 * Sets properties in @widget based on the values of
 * matching properties in @template_widget
 */
void
glade_widget_copy_properties (GladeWidget * widget,
                              GladeWidget * template_widget,
                              gboolean copy_parentless, gboolean exact)
{
  GList *l;

  g_return_if_fail (GLADE_IS_WIDGET (widget));
  g_return_if_fail (GLADE_IS_WIDGET (template_widget));

  for (l = widget->priv->properties; l && l->data; l = l->next)
    {
      GladeProperty *widget_prop = GLADE_PROPERTY (l->data);
      GladeProperty *template_prop;
      GladePropertyClass *widget_pclass, *template_pclass = NULL;

      widget_pclass = glade_property_get_class (widget_prop);
      template_prop = glade_widget_get_property (template_widget, 
						 glade_property_class_id (widget_pclass));
      if (template_prop)
	template_pclass = glade_property_get_class (template_prop);

      /* Check if they share the same class definition, different
       * properties may have the same name (support for
       * copying properties across "not-quite" compatible widget
       * classes, like GtkImageMenuItem --> GtkCheckMenuItem).
       */
      if (template_pclass != NULL &&
          glade_property_class_match (template_pclass, widget_pclass))
        {
          if (glade_property_class_parentless_widget (template_pclass) && copy_parentless)
            {
              GObject *object = NULL;
              GladeWidget *parentless;

              glade_property_get (template_prop, &object);
              if (object)
                {
                  parentless = glade_widget_get_from_gobject (object);
                  parentless = glade_widget_dup (parentless, exact);

                  glade_widget_set_project (parentless, widget->priv->project);

                  glade_property_set (widget_prop, parentless->priv->object);
                }
              else
                glade_property_set (widget_prop, NULL);
            }
          else
            glade_property_set_value (widget_prop, glade_property_inline_value (template_prop));
        }
    }
}


/**
 * glade_widget_add_verify:
 * @widget: A #GladeWidget
 * @child: The child #GladeWidget to add
 * @user_feedback: whether a notification dialog should be
 * presented in the case that the child cannot not be added.
 *
 * Checks whether @child can be added to @parent.
 *
 * If @user_feedback is %TRUE and @child cannot be
 * added then this shows a notification dialog to the user 
 * explaining why.
 *
 * Returns: whether @child can be added to @widget.
 */
gboolean
glade_widget_add_verify (GladeWidget      *widget,
			 GladeWidget      *child,
			 gboolean          user_feedback)
{
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GLADE_IS_WIDGET (child), FALSE);

  return glade_widget_adaptor_add_verify (widget->priv->adaptor,
					  widget->priv->object,
					  child->priv->object,
					  user_feedback);
}

/**
 * glade_widget_add_child:
 * @parent: A #GladeWidget
 * @child: the #GladeWidget to add
 * @at_mouse: whether the added widget should be added
 *            at the current mouse position
 *
 * Adds @child to @parent in a generic way for this #GladeWidget parent.
 */
void
glade_widget_add_child (GladeWidget * parent,
                        GladeWidget * child, gboolean at_mouse)
{
  g_return_if_fail (GLADE_IS_WIDGET (parent));
  g_return_if_fail (GLADE_IS_WIDGET (child));

  GLADE_WIDGET_GET_CLASS (parent)->add_child (parent, child, at_mouse);
}

/**
 * glade_widget_remove_child:
 * @parent: A #GladeWidget
 * @child: the #GladeWidget to add
 *
 * Removes @child from @parent in a generic way for this #GladeWidget parent.
 */
void
glade_widget_remove_child (GladeWidget * parent, GladeWidget * child)
{
  g_return_if_fail (GLADE_IS_WIDGET (parent));
  g_return_if_fail (GLADE_IS_WIDGET (child));

  GLADE_WIDGET_GET_CLASS (parent)->remove_child (parent, child);
}

/**
 * glade_widget_dup:
 * @template_widget: a #GladeWidget
 * @exact: whether or not to creat an exact duplicate
 *
 * Creates a deep copy of #GladeWidget. if @exact is specified,
 * the widget name is preserved and signals are carried over
 * (this is used to maintain names & signals in Cut/Paste context
 * as opposed to Copy/Paste contexts).
 *
 * Returns: The newly created #GladeWidget
 */
GladeWidget *
glade_widget_dup (GladeWidget * template_widget, gboolean exact)
{
  GladeWidget *widget;

  g_return_val_if_fail (GLADE_IS_WIDGET (template_widget), NULL);

  glade_widget_push_superuser ();
  widget =
      glade_widget_dup_internal (template_widget, NULL, template_widget, exact);
  glade_widget_pop_superuser ();

  return widget;
}

typedef struct
{
  GladeProperty *property;
  GValue value;
} PropertyData;

/**
 * glade_widget_rebuild:
 * @gwidget: a #GladeWidget
 *
 * Replaces the current widget instance with
 * a new one while preserving all properties children and
 * takes care of reparenting.
 *
 */
void
glade_widget_rebuild (GladeWidget * gwidget)
{
  GObject *new_object, *old_object;
  GladeWidgetAdaptor *adaptor;
  GladeProject *project = NULL;
  GladeWidget  *parent = NULL;
  GList *children;
  gboolean reselect = FALSE;
  GList *restore_properties = NULL;
  GList *save_properties, *l;

  g_return_if_fail (GLADE_IS_WIDGET (gwidget));

  adaptor = gwidget->priv->adaptor;

  if (gwidget->priv->parent &&
      glade_widget_adaptor_has_child (gwidget->priv->parent->priv->adaptor,
				      gwidget->priv->parent->priv->object,
				      gwidget->priv->object))
    parent = gwidget->priv->parent;

  g_object_ref (gwidget);

  /* Extract and keep the child hierarchies aside... */
  children = glade_widget_extract_children (gwidget);

  /* Here we take care removing the widget from the project and
   * the selection before rebuilding the instance.
   */
  if (gwidget->priv->project && glade_project_has_object (gwidget->priv->project, gwidget->priv->object))
    project = gwidget->priv->project;

  if (project)
    {
      if (glade_project_is_selected (project, gwidget->priv->object))
        {
          reselect = TRUE;
          glade_project_selection_remove (project, gwidget->priv->object, FALSE);
        }
      glade_project_remove_object (project, gwidget->priv->object);
    }

  /* parentless_widget and object properties that reffer to this widget 
   * should be unset before transfering */
  l = g_list_copy (gwidget->priv->properties);
  save_properties = g_list_copy (gwidget->priv->prop_refs);
  save_properties = g_list_concat (l, save_properties);

  for (l = save_properties; l; l = l->next)
    {
      GladeProperty      *property = l->data;
      GladePropertyClass *pclass = glade_property_get_class (property);

      if (glade_property_get_widget (property) != gwidget || 
	  glade_property_class_parentless_widget (pclass))
        {
          PropertyData *prop_data;

          if (!G_IS_PARAM_SPEC_OBJECT (glade_property_class_get_pspec (pclass)))
            g_warning ("Parentless widget property should be of object type");

          prop_data = g_new0 (PropertyData, 1);
          prop_data->property = property;

          if (glade_property_get_widget (property) == gwidget)
	    glade_property_get_value (property, &prop_data->value);

          restore_properties = g_list_prepend (restore_properties, prop_data);
          glade_property_set (property, NULL);
        }
    }
  g_list_free (save_properties);

  /* Remove old object from parent
   */
  if (parent)
    glade_widget_remove_child (parent, gwidget);

  /* Hold a reference to the old widget while we transport properties
   * and children from it
   */
  old_object = g_object_ref (glade_widget_get_object (gwidget));
  new_object = glade_widget_build_object (gwidget, gwidget, GLADE_CREATE_REBUILD);

  /* Only call this once the object has a proper GladeWidget */
  glade_widget_adaptor_post_create (adaptor, new_object, GLADE_CREATE_REBUILD);

  /* Must call dispose for cases like dialogs and toplevels */
  if (GTK_IS_WINDOW (old_object))
    gtk_widget_destroy (GTK_WIDGET (old_object));
  else
    g_object_unref (old_object);

  /* Reparent any children of the old object to the new object
   * (this function will consume and free the child list).
   */
  glade_widget_push_superuser ();
  glade_widget_insert_children (gwidget, children);
  glade_widget_pop_superuser ();

  /* Add new object to parent
   */
  if (parent)
    glade_widget_add_child (parent, gwidget, FALSE);

  /* Custom properties aren't transfered in build_object, since build_object
   * is only concerned with object creation.
   */
  glade_widget_sync_custom_props (gwidget);

  /* Setting parentless_widget and prop_ref properties back */
  for (l = restore_properties; l; l = l->next)
    {
      PropertyData *prop_data = l->data;
      GladeProperty *property = prop_data->property;

      if (glade_property_get_widget (property) == gwidget)
        {
          glade_property_set_value (property, &prop_data->value);
          g_value_unset (&prop_data->value);
        }
      else
        {
          /* restore property references on rebuilt objects */
          glade_property_set (property, gwidget->priv->object);
        }
      g_free (prop_data);
    }
  g_list_free (restore_properties);

  /* Sync packing.
   */
  if (parent)
    glade_widget_sync_packing_props (gwidget);

  /* If the widget was in a project (and maybe the selection), then
   * restore that stuff.
   */
  if (project)
    {
      glade_project_add_object (project, gwidget->priv->object);
      if (reselect)
        glade_project_selection_add (project, gwidget->priv->object, TRUE);
    }

  /* Ensure rebuilt widget visibility */
  if (GTK_IS_WIDGET (gwidget->priv->object) && 
      !GTK_IS_WINDOW (gwidget->priv->object))
    gtk_widget_show_all (GTK_WIDGET (gwidget->priv->object));

  /* We shouldnt show if its not already visible */
  if (gwidget->priv->visible)
    glade_widget_show (gwidget);

  g_object_unref (gwidget);
}

/**
 * glade_widget_list_signal_handlers:
 * @widget: a #GladeWidget
 * @signal_name: the name of the signal
 *
 * Returns: A #GPtrArray of #GladeSignal for @signal_name
 */
GPtrArray *
glade_widget_list_signal_handlers (GladeWidget * widget, const gchar * signal_name)     /* array of GladeSignal* */
{
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
  return g_hash_table_lookup (widget->priv->signals, signal_name);
}

/**
 * glade_widget_set_name:
 * @widget: a #GladeWidget
 * @name: a string
 *
 * Sets @widget's name to @name.
 */
void
glade_widget_set_name (GladeWidget * widget, const gchar * name)
{
  g_return_if_fail (GLADE_IS_WIDGET (widget));
  if (widget->priv->name != name)
    {
      if (widget->priv->name)
        g_free (widget->priv->name);

      widget->priv->name = g_strdup (name);
      g_object_notify_by_pspec (G_OBJECT (widget), properties[PROP_NAME]);
    }
}

/**
 * glade_widget_get_name:
 * @widget: a #GladeWidget
 *
 * Returns: a pointer to @widget's name
 */
const gchar *
glade_widget_get_name (GladeWidget * widget)
{
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
  return widget->priv->name;
}

/**
 * glade_widget_set_internal:
 * @widget: A #GladeWidget
 * @internal: The internal name
 *
 * Sets the internal name of @widget to @internal
 */
void
glade_widget_set_internal (GladeWidget * widget, const gchar * internal)
{
  g_return_if_fail (GLADE_IS_WIDGET (widget));
  if (widget->priv->internal != internal)
    {
      g_free (widget->priv->internal);
      widget->priv->internal = g_strdup (internal);
      g_object_notify_by_pspec (G_OBJECT (widget), properties[PROP_INTERNAL]);
    }
}

/**
 * glade_widget_get_internal:
 * @widget: a #GladeWidget
 *
 * Returns: the internal name of @widget
 */
G_CONST_RETURN gchar *
glade_widget_get_internal (GladeWidget * widget)
{
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
  return widget->priv->internal;
}

/**
 * glade_widget_get_adaptor:
 * @widget: a #GladeWidget
 *
 * Returns: the #GladeWidgetAdaptor of @widget
 */
GladeWidgetAdaptor *
glade_widget_get_adaptor (GladeWidget * widget)
{
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
  return widget->priv->adaptor;
}

/**
 * glade_widget_set_project:
 * @widget: a #GladeWidget
 * @project: a #GladeProject
 *
 * Makes @widget belong to @project.
 */
void
glade_widget_set_project (GladeWidget * widget, GladeProject * project)
{
  if (widget->priv->project != project)
    {
      widget->priv->project = project;
      g_object_notify_by_pspec (G_OBJECT (widget), properties[PROP_PROJECT]);
    }
}

/**
 * glade_widget_get_project:
 * @widget: a #GladeWidget
 * 
 * Returns: the #GladeProject that @widget belongs to
 */
GladeProject *
glade_widget_get_project (GladeWidget * widget)
{
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
  return widget->priv->project;
}

void
glade_widget_set_in_project (GladeWidget      *widget,
			     gboolean          in_project)
{
  g_return_if_fail (GLADE_IS_WIDGET (widget));

  widget->priv->in_project = in_project;
}

gboolean
glade_widget_in_project (GladeWidget      *widget)
{
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

  return widget->priv->in_project;
}

/**
 * glade_widget_get_property:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 *
 * Returns: the #GladeProperty in @widget named @id_property
 */
GladeProperty *
glade_widget_get_property (GladeWidget * widget, const gchar * id_property)
{
  GladeProperty *property;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (id_property != NULL, NULL);

  if (widget->priv->props_hash &&
      (property = g_hash_table_lookup (widget->priv->props_hash, id_property)))
    return property;

  return glade_widget_get_pack_property (widget, id_property);
}

/**
 * glade_widget_get_pack_property:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 *
 * Returns: the #GladeProperty in @widget named @id_property
 */
GladeProperty *
glade_widget_get_pack_property (GladeWidget * widget, const gchar * id_property)
{
  GladeProperty *property;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (id_property != NULL, NULL);

  if (widget->priv->pack_props_hash &&
      (property = g_hash_table_lookup (widget->priv->pack_props_hash, id_property)))
    return property;

  return NULL;
}


/**
 * glade_widget_property_get:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @...: The return location for the value of the said #GladeProperty
 *
 * Gets the value of @id_property in @widget
 *
 * Returns: whether @id_property was found or not.
 *
 */
gboolean
glade_widget_property_get (GladeWidget * widget, const gchar * id_property, ...)
{
  GladeProperty *property;
  va_list vl;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (id_property != NULL, FALSE);

  if ((property = glade_widget_get_property (widget, id_property)) != NULL)
    {
      va_start (vl, id_property);
      glade_property_get_va_list (property, vl);
      va_end (vl);
      return TRUE;
    }
  return FALSE;
}

/**
 * glade_widget_property_set:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @...: A value of the correct type for the said #GladeProperty
 *
 * Sets the value of @id_property in @widget
 *
 * Returns: whether @id_property was found or not.
 */
gboolean
glade_widget_property_set (GladeWidget * widget, const gchar * id_property, ...)
{
  GladeProperty *property;
  va_list vl;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (id_property != NULL, FALSE);

  if ((property = glade_widget_get_property (widget, id_property)) != NULL)
    {
      va_start (vl, id_property);
      glade_property_set_va_list (property, vl);
      va_end (vl);
      return TRUE;
    }
  return FALSE;
}

/**
 * glade_widget_pack_property_get:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @...: The return location for the value of the said #GladeProperty
 *
 * Gets the value of @id_property in @widget packing properties
 *
 * Returns: whether @id_property was found or not.
 */
gboolean
glade_widget_pack_property_get (GladeWidget * widget,
                                const gchar * id_property, ...)
{
  GladeProperty *property;
  va_list vl;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (id_property != NULL, FALSE);

  if ((property = glade_widget_get_pack_property (widget, id_property)) != NULL)
    {
      va_start (vl, id_property);
      glade_property_get_va_list (property, vl);
      va_end (vl);
      return TRUE;
    }
  return FALSE;
}

/**
 * glade_widget_pack_property_set:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @...: The return location for the value of the said #GladeProperty
 *
 * Sets the value of @id_property in @widget packing properties
 *
 * Returns: whether @id_property was found or not.
 */
gboolean
glade_widget_pack_property_set (GladeWidget * widget,
                                const gchar * id_property, ...)
{
  GladeProperty *property;
  va_list vl;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (id_property != NULL, FALSE);

  if ((property = glade_widget_get_pack_property (widget, id_property)) != NULL)
    {
      va_start (vl, id_property);
      glade_property_set_va_list (property, vl);
      va_end (vl);
      return TRUE;
    }
  return FALSE;
}

/**
 * glade_widget_property_set_sensitive:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @sensitive: setting sensitive or insensitive
 * @reason: a description of why the user cant edit this property
 *          which will be used as a tooltip
 *
 * Sets the sensitivity of @id_property in @widget
 *
 * Returns: whether @id_property was found or not.
 */
gboolean
glade_widget_property_set_sensitive (GladeWidget * widget,
                                     const gchar * id_property,
                                     gboolean sensitive, const gchar * reason)
{
  GladeProperty *property;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (id_property != NULL, FALSE);

  if ((property = glade_widget_get_property (widget, id_property)) != NULL)
    {
      glade_property_set_sensitive (property, sensitive, reason);
      return TRUE;
    }
  return FALSE;
}

/**
 * glade_widget_pack_property_set_sensitive:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @sensitive: setting sensitive or insensitive
 * @reason: a description of why the user cant edit this property
 *          which will be used as a tooltip
 *
 * Sets the sensitivity of @id_property in @widget's packing properties.
 *
 * Returns: whether @id_property was found or not.
 */
gboolean
glade_widget_pack_property_set_sensitive (GladeWidget * widget,
                                          const gchar * id_property,
                                          gboolean sensitive,
                                          const gchar * reason)
{
  GladeProperty *property;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (id_property != NULL, FALSE);

  if ((property = glade_widget_get_pack_property (widget, id_property)) != NULL)
    {
      glade_property_set_sensitive (property, sensitive, reason);
      return TRUE;
    }
  return FALSE;
}


/**
 * glade_widget_property_set_enabled:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @enabled: setting enabled or disabled
 *
 * Sets the enabled state of @id_property in @widget; this is
 * used for optional properties.
 *
 * Returns: whether @id_property was found or not.
 */
gboolean
glade_widget_property_set_enabled (GladeWidget * widget,
                                   const gchar * id_property, gboolean enabled)
{
  GladeProperty *property;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (id_property != NULL, FALSE);

  if ((property = glade_widget_get_property (widget, id_property)) != NULL)
    {
      glade_property_set_enabled (property, enabled);
      return TRUE;
    }
  return FALSE;
}

/**
 * glade_widget_pack_property_set_enabled:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @enabled: setting enabled or disabled
 *
 * Sets the enabled state of @id_property in @widget's packing 
 * properties; this is used for optional properties.
 *
 * Returns: whether @id_property was found or not.
 */
gboolean
glade_widget_pack_property_set_enabled (GladeWidget * widget,
                                        const gchar * id_property,
                                        gboolean enabled)
{
  GladeProperty *property;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (id_property != NULL, FALSE);

  if ((property = glade_widget_get_pack_property (widget, id_property)) != NULL)
    {
      glade_property_set_enabled (property, enabled);
      return TRUE;
    }
  return FALSE;
}

/**
 * glade_widget_property_set_save_always:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @setting: the setting 
 *
 * Sets whether @id_property in @widget should be special cased
 * to always be saved regardless of its default value.
 * (used for some special cases like properties
 * that are assigned initial values in composite widgets
 * or derived widget code).
 *
 * Returns: whether @id_property was found or not.
 */
gboolean
glade_widget_property_set_save_always (GladeWidget * widget,
                                       const gchar * id_property,
                                       gboolean setting)
{
  GladeProperty *property;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (id_property != NULL, FALSE);

  if ((property = glade_widget_get_property (widget, id_property)) != NULL)
    {
      glade_property_set_save_always (property, setting);
      return TRUE;
    }
  return FALSE;
}

/**
 * glade_widget_pack_property_set_save_always:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @setting: the setting 
 *
 * Sets whether @id_property in @widget should be special cased
 * to always be saved regardless of its default value.
 * (used for some special cases like properties
 * that are assigned initial values in composite widgets
 * or derived widget code).
 *
 * Returns: whether @id_property was found or not.
 */
gboolean
glade_widget_pack_property_set_save_always (GladeWidget * widget,
                                            const gchar * id_property,
                                            gboolean setting)
{
  GladeProperty *property;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (id_property != NULL, FALSE);

  if ((property = glade_widget_get_pack_property (widget, id_property)) != NULL)
    {
      glade_property_set_save_always (property, setting);
      return TRUE;
    }
  return FALSE;
}

/**
 * glade_widget_property_string:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @value: the #GValue to print or %NULL
 *
 * Creates a printable string representing @id_property in
 * @widget, if @value is specified it will be used in place
 * of @id_property's real value (this is a convinience
 * function to print/debug properties usually from plugin
 * backends).
 *
 * Returns: A newly allocated string representing @id_property
 */
gchar *
glade_widget_property_string (GladeWidget  *widget,
                              const gchar  *id_property, 
			      const GValue *value)
{
  GladeProperty      *property;
  GladePropertyClass *pclass;
  gchar              *ret_string = NULL;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (id_property != NULL, NULL);

  if ((property = glade_widget_get_property (widget, id_property)) != NULL)
    {
      pclass     = glade_property_get_class (property);
      ret_string = glade_widget_adaptor_string_from_value
        (glade_property_class_get_adaptor (pclass), pclass, 
	 value ? value : glade_property_inline_value (property));
    }

  return ret_string;
}

/**
 * glade_widget_pack_property_string:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @value: the #GValue to print or %NULL
 *
 * Same as glade_widget_property_string() but for packing
 * properties.
 *
 * Returns: A newly allocated string representing @id_property
 */
gchar *
glade_widget_pack_property_string (GladeWidget * widget,
                                   const gchar * id_property,
                                   const GValue * value)
{
  GladeProperty      *property;
  GladePropertyClass *pclass;
  gchar              *ret_string = NULL;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (id_property != NULL, NULL);

  if ((property = glade_widget_get_pack_property (widget, id_property)) != NULL)
    {
      pclass     = glade_property_get_class (property);
      ret_string = glade_widget_adaptor_string_from_value
        (glade_property_class_get_adaptor (pclass), pclass, 
	 value ? value : glade_property_inline_value (property));
    }

  return ret_string;
}


/**
 * glade_widget_property_reset:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 *
 * Resets @id_property in @widget to it's default value
 *
 * Returns: whether @id_property was found or not.
 */
gboolean
glade_widget_property_reset (GladeWidget * widget, const gchar * id_property)
{
  GladeProperty *property;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

  if ((property = glade_widget_get_property (widget, id_property)) != NULL)
    {
      glade_property_reset (property);
      return TRUE;
    }
  return FALSE;
}

/**
 * glade_widget_pack_property_reset:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 *
 * Resets @id_property in @widget's packing properties to it's default value
 *
 * Returns: whether @id_property was found or not.
 */
gboolean
glade_widget_pack_property_reset (GladeWidget * widget,
                                  const gchar * id_property)
{
  GladeProperty *property;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

  if ((property = glade_widget_get_pack_property (widget, id_property)) != NULL)
    {
      glade_property_reset (property);
      return TRUE;
    }
  return FALSE;
}

static gboolean
glade_widget_property_default_common (GladeWidget * widget,
                                      const gchar * id_property,
                                      gboolean original)
{
  GladeProperty *property;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

  if ((property = glade_widget_get_property (widget, id_property)) != NULL)
    return (original) ? glade_property_original_default (property) :
        glade_property_default (property);

  return FALSE;
}

/**
 * glade_widget_property_default:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 *
 * Returns: whether whether @id_property was found and is 
 * currently set to it's default value.
 */
gboolean
glade_widget_property_default (GladeWidget * widget, const gchar * id_property)
{
  return glade_widget_property_default_common (widget, id_property, FALSE);
}

/**
 * glade_widget_property_original_default:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 *
 * Returns: whether whether @id_property was found and is 
 * currently set to it's original default value.
 */
gboolean
glade_widget_property_original_default (GladeWidget * widget,
                                        const gchar * id_property)
{
  return glade_widget_property_default_common (widget, id_property, TRUE);
}

/**
 * glade_widget_pack_property_default:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 *
 * Returns: whether whether @id_property was found and is 
 * currently set to it's default value.
 */
gboolean
glade_widget_pack_property_default (GladeWidget * widget,
                                    const gchar * id_property)
{
  GladeProperty *property;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

  if ((property = glade_widget_get_pack_property (widget, id_property)) != NULL)
    return glade_property_default (property);

  return FALSE;
}


/**
 * glade_widget_object_set_property:
 * @widget:        A #GladeWidget
 * @property_name: The property identifier
 * @value:         The #GValue
 *
 * This function applies @value to the property @property_name on
 * the runtime object of @widget.
 */
void
glade_widget_object_set_property (GladeWidget * widget,
                                  const gchar * property_name,
                                  const GValue * value)
{
  g_return_if_fail (GLADE_IS_WIDGET (widget));
  g_return_if_fail (property_name != NULL && value != NULL);

  glade_widget_adaptor_set_property (widget->priv->adaptor,
                                     widget->priv->object, property_name, value);
}


/**
 * glade_widget_object_get_property:
 * @widget:        A #GladeWidget
 * @property_name: The property identifier
 * @value:         The #GValue
 *
 * This function retrieves the value of the property @property_name on
 * the runtime object of @widget and sets it in @value.
 */
void
glade_widget_object_get_property (GladeWidget * widget,
                                  const gchar * property_name, GValue * value)
{
  g_return_if_fail (GLADE_IS_WIDGET (widget));
  g_return_if_fail (property_name != NULL && value != NULL);

  glade_widget_adaptor_get_property (widget->priv->adaptor,
                                     widget->priv->object, property_name, value);
}

/**
 * glade_widget_child_set_property:
 * @widget:        A #GladeWidget
 * @child:         The #GladeWidget child
 * @property_name: The id of the property
 * @value:         The @GValue
 *
 * Sets @child's packing property identified by @property_name to @value.
 */
void
glade_widget_child_set_property (GladeWidget  *widget,
                                 GladeWidget  *child,
                                 const gchar  *property_name,
                                 const GValue *value)
{
  GladeWidgetPrivate *priv, *cpriv;
  GList *old_order;
  gboolean check;

  g_return_if_fail (GLADE_IS_WIDGET (widget));
  g_return_if_fail (GLADE_IS_WIDGET (child));
  g_return_if_fail (property_name != NULL && value != NULL);

  priv = widget->priv;
  cpriv = child->priv;

  check = priv->project && priv->in_project && cpriv->project && cpriv->in_project;
  old_order = (check) ? glade_widget_get_children (widget) : NULL;

  glade_widget_adaptor_child_set_property (priv->adaptor, priv->object,
                                           cpriv->object, property_name, value);

  /* After setting a child property... it's possible the order of children
   * in the parent has been effected.
   *
   * If this is the case then we need to signal the GladeProject that
   * it's rows have been reordered so that any connected views update
   * themselves properly.
   */
  if (check)
    glade_project_check_reordered (priv->project, widget, old_order);

  g_list_free (old_order);
}

/**
 * glade_widget_child_get_property:
 * @widget:        A #GladeWidget
 * @child:         The #GladeWidget child
 * @property_name: The id of the property
 * @value:         The @GValue
 *
 * Gets @child's packing property identified by @property_name.
 */
void
glade_widget_child_get_property (GladeWidget * widget,
                                 GladeWidget * child,
                                 const gchar * property_name, GValue * value)
{
  g_return_if_fail (GLADE_IS_WIDGET (widget));
  g_return_if_fail (GLADE_IS_WIDGET (child));
  g_return_if_fail (property_name != NULL && value != NULL);

  glade_widget_adaptor_child_get_property (widget->priv->adaptor,
                                           widget->priv->object,
                                           child->priv->object, property_name, value);

}

static void
glade_widget_add_events (GtkWidget * widget)
{
  GList *children, *list;

  gtk_widget_add_events (widget,
                         GDK_POINTER_MOTION_MASK |      /* Handle pointer events */
                         GDK_POINTER_MOTION_HINT_MASK | /* for drag/resize and   */
                         GDK_BUTTON_PRESS_MASK |        /* managing selection.   */
                         GDK_BUTTON_RELEASE_MASK);

  /* We also need to get events for any children. */
  if (GTK_IS_CONTAINER (widget))
    {
      if ((children =
           glade_util_container_get_all_children (GTK_CONTAINER
                                                  (widget))) != NULL)
        {
          for (list = children; list; list = list->next)
            glade_widget_add_events (GTK_WIDGET (list->data));
          g_list_free (children);
        }
    }
}

static void
glade_widget_set_object (GladeWidget * gwidget, GObject * new_object,
                         gboolean destroy)
{
  GObject *old_object;

  g_return_if_fail (GLADE_IS_WIDGET (gwidget));
  g_return_if_fail (new_object == NULL ||
                    g_type_is_a (G_OBJECT_TYPE (new_object),
                                 glade_widget_adaptor_get_object_type (gwidget->priv->adaptor)));

  if (gwidget->priv->object == new_object)
    return;

  old_object = gwidget->priv->object;
  gwidget->priv->object = new_object;

  if (new_object)
    {
      /* Add internal reference to new widget if its not internal */
      if (gwidget->priv->internal == NULL)
        {
          /* Assume initial ref count of all objects */
          if (G_IS_INITIALLY_UNOWNED (new_object))
            g_object_ref_sink (new_object);
        }

      g_object_set_qdata (G_OBJECT (new_object), glade_widget_name_quark,
                          gwidget);

      if (g_type_is_a (glade_widget_adaptor_get_object_type (gwidget->priv->adaptor), GTK_TYPE_WIDGET))
        {
          /* Disable any built-in DnD */
          gtk_drag_dest_unset (GTK_WIDGET (new_object));
          gtk_drag_source_unset (GTK_WIDGET (new_object));
          /* We nee to make sure all widgets set the event glade core needs */
          glade_widget_add_events (GTK_WIDGET (new_object));
        }
    }

  /* Remove internal reference to old widget */
  if (old_object)
    {
      /* From this point on, the GladeWidget is no longer retrievable with
       * glade_widget_get_from_gobject() */
      g_object_set_qdata (G_OBJECT (old_object), glade_widget_name_quark, NULL);

      if (gwidget->priv->internal == NULL)
        {
#if _YOU_WANT_TO_LOOK_AT_PROJECT_REFCOUNT_BALANCING_
          g_print ("Killing '%s::%s' widget's object with reference count %d\n",
                   glade_widget_adaptor_get_name (gwidget->priv->adaptor),
                   gwidget->priv->name ? gwidget->priv->name : "(unknown)",
                   old_object->ref_count);
#endif
          if (GTK_IS_WINDOW (old_object) && destroy)
            gtk_widget_destroy (GTK_WIDGET (old_object));
          else
            g_object_unref (old_object);

        }
    }
  g_object_notify_by_pspec (G_OBJECT (gwidget), properties[PROP_OBJECT]);
}

/**
 * glade_widget_get_object:
 * @widget: a #GladeWidget
 *
 * Returns: the #GObject associated with @widget
 */
GObject *
glade_widget_get_object (GladeWidget * widget)
{
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
  return widget->priv->object;
}

/**
 * glade_widget_get_parent:
 * @widget: A #GladeWidget
 *
 * Returns: The parenting #GladeWidget
 */
GladeWidget *
glade_widget_get_parent (GladeWidget * widget)
{
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
  return widget->priv->parent;
}

/**
 * glade_widget_set_parent:
 * @widget: A #GladeWidget
 * @parent: the parenting #GladeWidget (or %NULL)
 *
 * sets the parenting #GladeWidget
 */
void
glade_widget_set_parent (GladeWidget * widget, GladeWidget * parent)
{
  GladeWidget *old_parent;

  g_return_if_fail (GLADE_IS_WIDGET (widget));

  old_parent = widget->priv->parent;
  widget->priv->parent = parent;

  /* Set packing props only if the object is actually parented by 'parent'
   * (a subsequent call should come from glade_command after parenting).
   */
  if (widget->priv->object && parent != NULL &&
      glade_widget_adaptor_has_child
      (parent->priv->adaptor, parent->priv->object, widget->priv->object))
    {
      if (old_parent == NULL || widget->priv->packing_properties == NULL ||
          old_parent->priv->adaptor != parent->priv->adaptor)
        glade_widget_set_packing_properties (widget, parent);
      else
        glade_widget_sync_packing_props (widget);
    }

  if (parent)
    glade_widget_set_packing_actions (widget, parent);

  g_object_notify_by_pspec (G_OBJECT (widget), properties[PROP_PARENT]);
}

/**
 * glade_widget_find_child:
 * @widget: A #GladeWidget
 * @name: child name
 *
 * Finds a child widget named @name.
 *
 * Returns: The child of widget or NULL if it was not found.
 */
GladeWidget *
glade_widget_find_child (GladeWidget *widget, 
			 const gchar *name)
{
  GList *adapter_children;
  GladeWidget *real_child = NULL;
  GList *node;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

  adapter_children =
      glade_widget_adaptor_get_children (glade_widget_get_adaptor (widget),
                                         widget->priv->object);

  for (node = adapter_children; node && !real_child; node = g_list_next (node))
    {
      GladeWidget *child = glade_widget_get_from_gobject (node->data);

      if (child)
        {
          if (strcmp (child->priv->name, name) == 0)
            real_child = child;
          else
            real_child = glade_widget_find_child (child, name);
        }
    }
  g_list_free (adapter_children);

  return real_child;
}

/**
 * glade_widget_get_children:
 * @widget: A #GladeWidget
 *
 * Fetches any wrapped children of @widget.
 *
 * Returns: The children of widget
 *
 * <note><para>This differs from a direct call to glade_widget_adaptor_get_children() as
 * it only returns children which have an associated GladeWidget. This function will
 * not return any placeholders or internal composite children that have not been 
 * exposed for Glade configuration</para></note>
 */
GList *
glade_widget_get_children (GladeWidget * widget)
{
  GList *adapter_children;
  GList *real_children = NULL;
  GList *node;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

  adapter_children =
      glade_widget_adaptor_get_children (glade_widget_get_adaptor (widget),
                                         widget->priv->object);

  for (node = adapter_children; node != NULL; node = g_list_next (node))
    {
      if (glade_widget_get_from_gobject (node->data))
        {
          real_children = g_list_prepend (real_children, node->data);
        }
    }
  g_list_free (adapter_children);

  return g_list_reverse (real_children);
}


/**
 * glade_widget_get_toplevel:
 * @widget: A #GladeWidget
 *
 * Returns: The toplevel #GladeWidget in the hierarchy (or @widget)
 */
GladeWidget *
glade_widget_get_toplevel (GladeWidget * widget)
{
  GladeWidget *toplevel = widget;
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

  while (toplevel->priv->parent)
    toplevel = toplevel->priv->parent;

  return toplevel;
}

/**
 * glade_widget_set_packing_properties:
 * @widget:     A #GladeWidget
 * @container:  The parent #GladeWidget
 *
 * Generates the packing_properties list of the widget, given
 * the class of the container we are adding the widget to.
 * If the widget already has packing_properties, but the container
 * has changed, the current list is freed and replaced.
 */
void
glade_widget_set_packing_properties (GladeWidget * widget,
                                     GladeWidget * container)
{
  GList *list;

  g_return_if_fail (GLADE_IS_WIDGET (widget));
  g_return_if_fail (GLADE_IS_WIDGET (container));

  g_list_foreach (widget->priv->packing_properties, (GFunc) g_object_unref, NULL);
  g_list_free (widget->priv->packing_properties);
  widget->priv->packing_properties = NULL;

  if (widget->priv->pack_props_hash)
    g_hash_table_destroy (widget->priv->pack_props_hash);
  widget->priv->pack_props_hash = NULL;

  /* We have to detect whether this is an anarchist child of a composite
   * widget or not, in otherwords; whether its really a direct child or
   * a child of a popup window created on the composite widget's behalf.
   */
  if (widget->priv->anarchist)
    return;

  widget->priv->packing_properties =
      glade_widget_create_packing_properties (container, widget);
  widget->priv->pack_props_hash = g_hash_table_new (g_str_hash, g_str_equal);

  /* update the quick reference hash table */
  for (list = widget->priv->packing_properties; list && list->data; list = list->next)
    {
      GladeProperty      *property = list->data;
      GladePropertyClass *pclass = glade_property_get_class (property);

      g_hash_table_insert (widget->priv->pack_props_hash, 
			   (gchar *)glade_property_class_id (pclass), 
			   property);
    }

  /* Dont introspect on properties that are not parented yet.
   */
  if (glade_widget_adaptor_has_child (container->priv->adaptor,
                                      container->priv->object, 
				      widget->priv->object))
    {
      glade_widget_set_default_packing_properties (container, widget);

      /* update the values of the properties to the ones we get from gtk */
      for (list = widget->priv->packing_properties;
           list && list->data; list = list->next)
        {
	  /* XXX Ugly dangerous code, plays with the property value inline */
          GladeProperty      *property = list->data;
	  GladePropertyClass *pclass   = glade_property_get_class (property);
	  GValue             *value    = glade_property_inline_value (property);

          g_value_reset (value);
          glade_widget_child_get_property (container, 
					   widget, 
					   glade_property_class_id (pclass), 
					   value);
        }
    }
}

/**
 * glade_widget_has_decendant:
 * @widget: a #GladeWidget
 * @type: a  #GType
 * 
 * Returns: whether this GladeWidget has any decendants of type @type
 *          or any decendants that implement the @type interface
 */
gboolean
glade_widget_has_decendant (GladeWidget * widget, GType type)
{
  GladeWidget *child;
  GList *children, *l;
  gboolean found = FALSE;

  if (glade_widget_adaptor_get_object_type (widget->priv->adaptor) == type ||
      g_type_is_a (glade_widget_adaptor_get_object_type (widget->priv->adaptor), type))
    return TRUE;

  if ((children = glade_widget_adaptor_get_children
       (widget->priv->adaptor, widget->priv->object)) != NULL)
    {
      for (l = children; l; l = l->next)
        if ((child = glade_widget_get_from_gobject (l->data)) != NULL &&
            (found = glade_widget_has_decendant (child, type)))
          break;
      g_list_free (children);
    }
  return found;
}

/**
 * glade_widget_replace:
 * @old_object: a #GObject
 * @new_object: a #GObject
 * 
 * Replaces a GObject with another GObject inside a GObject which
 * behaves as a container.
 *
 * Note that both GObjects must be owned by a GladeWidget.
 */
void
glade_widget_replace (GladeWidget * parent, GObject * old_object,
                      GObject * new_object)
{
  g_return_if_fail (G_IS_OBJECT (old_object));
  g_return_if_fail (G_IS_OBJECT (new_object));

  GLADE_WIDGET_GET_CLASS (parent)->replace_child (parent, old_object,
                                                  new_object);
}

/*******************************************************************************
 *                           Xml Parsing code                                  *
 *******************************************************************************/
/* XXX Doc me !*/
void
glade_widget_write_special_child_prop (GladeWidget * parent,
                                       GObject * object,
                                       GladeXmlContext * context,
                                       GladeXmlNode * node)
{
  gchar *buff, *special_child_type;

  buff = g_object_get_data (object, "special-child-type");
  g_object_get (parent->priv->adaptor, "special-child-type", &special_child_type,
                NULL);

  if (special_child_type && buff)
    {
      glade_xml_node_set_property_string (node, GLADE_XML_TAG_TYPE, buff);
    }
  g_free (special_child_type);
}

/* XXX Doc me ! */
void
glade_widget_set_child_type_from_node (GladeWidget * parent,
                                       GObject * child, GladeXmlNode * node)
{
  gchar *special_child_type, *value;

  if (!glade_xml_node_verify (node, GLADE_XML_TAG_CHILD))
    return;

  g_object_get (parent->priv->adaptor, 
		"special-child-type", &special_child_type, 
		NULL);
  if (!special_child_type)
    return;

  /* all child types here are depicted by the "type" property */
  if ((value = glade_xml_get_property_string (node, GLADE_XML_TAG_TYPE)))
    {
      g_object_set_data_full (child, "special-child-type", value, g_free);
    }
  g_free (special_child_type);
}


/**
 * glade_widget_read_child:
 * @widget: A #GladeWidget
 * @node: a #GladeXmlNode
 *
 * Reads in a child widget from the xml (handles 'child' tag)
 */
void
glade_widget_read_child (GladeWidget * widget, GladeXmlNode * node)
{
  if (glade_project_load_cancelled (widget->priv->project))
    return;

  glade_widget_adaptor_read_child (widget->priv->adaptor, widget, node);
}

/**
 * glade_widget_read:
 * @project: a #GladeProject
 * @parent: The parent #GladeWidget or %NULL
 * @node: a #GladeXmlNode
 *
 * Returns: a new #GladeWidget for @project, based on @node
 */
GladeWidget *
glade_widget_read (GladeProject * project,
                   GladeWidget * parent,
                   GladeXmlNode * node, const gchar * internal)
{
  GladeWidgetAdaptor *adaptor;
  GladeWidget *widget = NULL;
  gchar *klass, *id;

  if (glade_project_load_cancelled (project))
    return NULL;

  if (!glade_xml_node_verify (node, GLADE_XML_TAG_WIDGET))
    return NULL;

  glade_widget_push_superuser ();

  if ((klass =
       glade_xml_get_property_string_required
       (node, GLADE_XML_TAG_CLASS, NULL)) != NULL)
    {
      if ((id =
           glade_xml_get_property_string_required
           (node, GLADE_XML_TAG_ID, NULL)) != NULL)
        {
          GType type;
          /* 
           * Create GladeWidget instance based on type. 
           */
          if ((adaptor = glade_widget_adaptor_get_by_name (klass)) &&
              (type = glade_widget_adaptor_get_object_type (adaptor)) &&
              G_TYPE_IS_INSTANTIATABLE (type) &&
              G_TYPE_IS_ABSTRACT (type) == FALSE)
            {
              /* Internal children !!! */
              if (internal)
                {
                  GObject *child_object =
		    glade_widget_get_internal_child (NULL, parent, internal);

                  if (!child_object)
                    {
                      g_warning ("Failed to locate "
                                 "internal child %s of %s",
                                 internal, glade_widget_get_name (parent));
		      goto out;
                    }

                  if (!(widget = glade_widget_get_from_gobject (child_object)))
                    g_error ("Unable to get GladeWidget "
                             "for internal child %s\n", internal);

                  /* Apply internal widget name from here */
                  glade_widget_set_name (widget, id);
                }
              else
                {
                  widget = glade_widget_adaptor_create_widget
                      (adaptor, FALSE,
                       "name", id,
                       "parent", parent,
                       "project", project, "reason", GLADE_CREATE_LOAD, NULL);
                }

              glade_widget_adaptor_read_widget (adaptor, widget, node);
            }
          else
            {
              GObject *stub = g_object_new (GLADE_TYPE_OBJECT_STUB,
                                            "object-type", klass,
                                            "xml-node", node,
                                            NULL);
              
              widget = glade_widget_adaptor_create_widget (glade_widget_adaptor_get_by_type (GTK_TYPE_WIDGET),
                                                           FALSE,
                                                           "parent", parent,
                                                           "project", project,
                                                           "reason", GLADE_CREATE_LOAD,
                                                           "object", stub,
                                                           "name", id,
                                                           NULL);
            }
          g_free (id);
        }
      g_free (klass);
    }

 out:
  glade_widget_pop_superuser ();

  glade_project_push_progress (project);

  return widget;
}


/**
 * glade_widget_write_child:
 * @widget: A #GladeWidget
 * @child: The child #GladeWidget to write
 * @context: A #GladeXmlContext
 * @node: A #GladeXmlNode
 *
 * Writes out a widget to the xml, takes care
 * of packing properties and special child types.
 */
void
glade_widget_write_child (GladeWidget * widget,
                          GladeWidget * child,
                          GladeXmlContext * context, GladeXmlNode * node)
{
  g_return_if_fail (GLADE_IS_WIDGET (widget));
  g_return_if_fail (GLADE_IS_WIDGET (child));
  g_return_if_fail (child->priv->parent == widget);

  glade_widget_adaptor_write_child (widget->priv->adaptor, child, context, node);
}


/**
 * glade_widget_write_placeholder:
 * @parent: The parent #GladeWidget
 * @object: A #GladePlaceHolder
 * @context: A #GladeXmlContext
 * @node: A #GladeXmlNode
 *
 * Writes out a placeholder to the xml
 */
void
glade_widget_write_placeholder (GladeWidget * parent,
                                GObject * object,
                                GladeXmlContext * context, GladeXmlNode * node)
{
  GladeXmlNode *child_node, *packing_node, *placeholder_node;

  child_node = glade_xml_node_new (context, GLADE_XML_TAG_CHILD);
  glade_xml_node_append_child (node, child_node);

  placeholder_node = glade_xml_node_new (context, GLADE_XML_TAG_PLACEHOLDER);
  glade_xml_node_append_child (child_node, placeholder_node);

  /* maybe write out special-child-type here */
  packing_node = glade_xml_node_new (context, GLADE_XML_TAG_PACKING);
  glade_xml_node_append_child (child_node, packing_node);

  glade_widget_write_special_child_prop (parent, object, context, child_node);

  if (!glade_xml_node_get_children (packing_node))
    {
      glade_xml_node_remove (packing_node);
      glade_xml_node_delete (packing_node);
    }
}

typedef struct
{
  GladeXmlContext *context;
  GladeXmlNode *node;
} WriteSignalsInfo;

static void
glade_widget_adaptor_write_signals (gpointer key,
                                    gpointer value, gpointer user_data)
{
  WriteSignalsInfo *info;
  GPtrArray *signals;
  guint i;

  info = (WriteSignalsInfo *) user_data;
  signals = (GPtrArray *) value;
  for (i = 0; i < signals->len; i++)
    {
      GladeSignal *signal = g_ptr_array_index (signals, i);
      glade_signal_write (signal, info->context, info->node);
    }
}

void
glade_widget_write_signals (GladeWidget * widget,
                            GladeXmlContext * context, GladeXmlNode * node)
{
  WriteSignalsInfo info;

  info.context = context;
  info.node = node;

  g_hash_table_foreach (widget->priv->signals,
                        glade_widget_adaptor_write_signals, &info);

}

/**
 * glade_widget_write:
 * @widget: The #GladeWidget
 * @context: A #GladeXmlContext
 * @node: A #GladeXmlNode
 *
 * Recursively writes out @widget and its children
 * and appends the created #GladeXmlNode to @node.
 */
void
glade_widget_write (GladeWidget * widget,
                    GladeXmlContext * context, GladeXmlNode * node)
{
  GObject *object = glade_widget_get_object (widget);
  GladeXmlNode *widget_node;
  GList *l, *list;

  /* Check if its an unknown object, and use saved xml if so */
  if (GLADE_IS_OBJECT_STUB (object))
    {
      g_object_get (object, "xml-node", &widget_node, NULL);
      glade_xml_node_append_child (node, widget_node);
      return;
    }

  widget_node = glade_xml_node_new (context, GLADE_XML_TAG_WIDGET);
  glade_xml_node_append_child (node, widget_node);

  /* Set class and id */
  glade_xml_node_set_property_string (widget_node,
                                      GLADE_XML_TAG_CLASS,
                                      glade_widget_adaptor_get_name (widget->priv->adaptor));
  glade_xml_node_set_property_string (widget_node,
                                      GLADE_XML_TAG_ID, widget->priv->name);

  /* Write out widget content (properties and signals) */
  glade_widget_adaptor_write_widget (widget->priv->adaptor, widget, context,
                                     widget_node);

  /* Write the signals strictly after all properties and before children
   */
  glade_widget_write_signals (widget, context, widget_node);

  /* Write the children */
  if ((list =
       glade_widget_adaptor_get_children (widget->priv->adaptor,
                                          widget->priv->object)) != NULL)
    {
      for (l = list; l; l = l->next)
        {
          GladeWidget *child = glade_widget_get_from_gobject (l->data);

          if (child)
            glade_widget_write_child (widget, child, context, widget_node);
          else if (GLADE_IS_PLACEHOLDER (l->data))
            glade_widget_write_placeholder (widget,
                                            G_OBJECT (l->data),
                                            context, widget_node);
        }
      g_list_free (list);
    }
}


/**
 * glade_widget_is_ancestor:
 * @widget: a #GladeWidget
 * @ancestor: another #GladeWidget
 *
 * Determines whether @widget is somewhere inside @ancestor, possibly with
 * intermediate containers.
 *
 * Return value: %TRUE if @ancestor contains @widget as a child,
 *    grandchild, great grandchild, etc.
 **/
gboolean
glade_widget_is_ancestor (GladeWidget * widget, GladeWidget * ancestor)
{
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GLADE_IS_WIDGET (ancestor), FALSE);

  while (widget)
    {
      if (widget->priv->parent == ancestor)
        return TRUE;
      widget = widget->priv->parent;
    }

  return FALSE;
}

/**
 * glade_widget_get_device_from_event:
 * @event: a GdkEvent
 * 
 * Currently only motion and button events are handled (see IS_GLADE_WIDGET_EVENT)
 * 
 * Returns: the asociated GdkDevice for this glade widget event.
 */
GdkDevice *
glade_widget_get_device_from_event (GdkEvent *event)
{
  g_return_val_if_fail (event, NULL);

  switch (event->type)
    {
      case GDK_BUTTON_PRESS:
      case GDK_BUTTON_RELEASE:
        return event->button.device;
      case GDK_MOTION_NOTIFY:
        return event->motion.device;
      default:
        return NULL;
    }
}

static gint glade_widget_su_stack = 0;

/**
 * glade_widget_superuser:
 *
 * Checks if we are in superuser mode.
 *
 * Superuser mode is when we are
 *   - Loading a project
 *   - Dupping a widget recursively
 *   - Rebuilding an instance for a construct-only property
 *
 * In these cases, we must act like a load, this should be checked
 * from the plugin when implementing containers, when undo/redo comes
 * around, the plugin is responsable for maintaining the same container
 * size when widgets are added/removed.
 */
gboolean
glade_widget_superuser (void)
{
  return glade_widget_su_stack > 0;
}

/**
 * glade_widget_push_superuser:
 *
 * Sets superuser mode
 */
void
glade_widget_push_superuser (void)
{
  glade_property_push_superuser ();
  glade_widget_su_stack++;
}


/**
 * glade_widget_pop_superuser:
 *
 * Unsets superuser mode
 */
void
glade_widget_pop_superuser (void)
{
  if (--glade_widget_su_stack < 0)
    {
      g_critical ("Bug: widget super user stack is corrupt.\n");
    }
  glade_property_pop_superuser ();
}


/**
 * glade_widget_placeholder_relation:
 * @parent: A #GladeWidget
 * @widget: The child #GladeWidget
 *
 * Returns whether placeholders should be used
 * in operations concerning this parent & child.
 *
 * Currently that criteria is whether @parent is a
 * GtkContainer, @widget is a GtkWidget and the parent
 * adaptor has been marked to use placeholders.
 *
 * Returns: whether to use placeholders for this relationship.
 */
gboolean
glade_widget_placeholder_relation (GladeWidget * parent, GladeWidget * widget)
{
  g_return_val_if_fail (GLADE_IS_WIDGET (parent), FALSE);
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

  return (GTK_IS_CONTAINER (parent->priv->object) &&
          GTK_IS_WIDGET (widget->priv->object) &&
          GWA_USE_PLACEHOLDERS (parent->priv->adaptor));
}

static GladeWidgetAction *
glade_widget_action_lookup (GList *actions, const gchar * path)
{
  GList *l;

  for (l = actions; l; l = g_list_next (l))
    {
      GladeWidgetAction *action   = l->data;
      GWActionClass     *aclass   = glade_widget_action_get_class (action);
      GList             *children = glade_widget_action_get_children (action);

      if (strcmp (aclass->path, path) == 0)
	return action;

      if (children &&
          g_str_has_prefix (path, aclass->path) &&
          (action = glade_widget_action_lookup (children, path)))
        return action;
    }

  return NULL;
}

/**
 * glade_widget_get_action:
 * @widget: a #GladeWidget
 * @action_path: a full action path including groups
 *
 * Returns a #GladeWidgetAction object indentified by @action_path.
 *
 * Returns: the action or NULL if not found.
 */
GladeWidgetAction *
glade_widget_get_action (GladeWidget * widget, const gchar * action_path)
{
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (action_path != NULL, NULL);

  return glade_widget_action_lookup (widget->priv->actions, action_path);
}

/**
 * glade_widget_get_pack_action:
 * @widget: a #GladeWidget
 * @action_path: a full action path including groups
 *
 * Returns a #GladeWidgetAction object indentified by @action_path.
 *
 * Returns: the action or NULL if not found.
 */
GladeWidgetAction *
glade_widget_get_pack_action (GladeWidget * widget, const gchar * action_path)
{
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (action_path != NULL, NULL);

  return glade_widget_action_lookup (widget->priv->packing_actions, action_path);
}


GList *
glade_widget_get_actions (GladeWidget *widget)
{
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

  return widget->priv->actions;
}

GList *
glade_widget_get_pack_actions (GladeWidget *widget)
{
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

  return widget->priv->packing_actions;
}


/**
 * glade_widget_set_action_sensitive:
 * @widget: a #GladeWidget
 * @action_path: a full action path including groups
 * @sensitive: setting sensitive or insensitive
 *
 * Sets the sensitivity of @action_path in @widget
 *
 * Returns: whether @action_path was found or not.
 */
gboolean
glade_widget_set_action_sensitive (GladeWidget * widget,
                                   const gchar * action_path,
                                   gboolean sensitive)
{
  GladeWidgetAction *action;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

  if ((action = glade_widget_get_action (widget, action_path)) != NULL)
    {
      glade_widget_action_set_sensitive (action, sensitive);
      return TRUE;
    }
  return FALSE;
}

/**
 * glade_widget_set_pack_action_sensitive:
 * @widget: a #GladeWidget
 * @action_path: a full action path including groups
 * @sensitive: setting sensitive or insensitive
 *
 * Sets the sensitivity of @action_path in @widget
 *
 * Returns: whether @action_path was found or not.
 */
gboolean
glade_widget_set_pack_action_sensitive (GladeWidget * widget,
                                        const gchar * action_path,
                                        gboolean sensitive)
{
  GladeWidgetAction *action;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

  if ((action = glade_widget_get_pack_action (widget, action_path)) != NULL)
    {
      glade_widget_action_set_sensitive (action, sensitive);
      return TRUE;
    }
  return FALSE;
}


/**
 * glade_widget_set_action_visible:
 * @widget: a #GladeWidget
 * @action_path: a full action path including groups
 * @visible: setting visible or invisible
 *
 * Sets the visibility of @action_path in @widget
 *
 * Returns: whether @action_path was found or not.
 */
gboolean
glade_widget_set_action_visible (GladeWidget *widget,
				 const gchar *action_path,
				 gboolean     visible)
{
  GladeWidgetAction *action;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

  if ((action = glade_widget_get_action (widget, action_path)) != NULL)
    {
      glade_widget_action_set_visible (action, visible);
      return TRUE;
    }
  return FALSE;
}

/**
 * glade_widget_set_pack_action_visible:
 * @widget: a #GladeWidget
 * @action_path: a full action path including groups
 * @visible: setting visible or invisible
 *
 * Sets the visibility of @action_path in @widget
 *
 * Returns: whether @action_path was found or not.
 */
gboolean
glade_widget_set_pack_action_visible (GladeWidget *widget,
				      const gchar *action_path,
				      gboolean     visible)
{
  GladeWidgetAction *action;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

  if ((action = glade_widget_get_pack_action (widget, action_path)) != NULL)
    {
      glade_widget_action_set_visible (action, visible);
      return TRUE;
    }
  return FALSE;
}


/**
 * glade_widget_create_editor_property:
 * @widget: A #GladeWidget
 * @property: The widget's property
 * @packing: whether @property indicates a packing property or not.
 * @use_command: Whether the undo/redo stack applies here.
 *
 * This is a convenience function to create a GladeEditorProperty corresponding
 * to @property
 *
 * Returns: A newly created and connected GladeEditorProperty
 */
GladeEditorProperty *
glade_widget_create_editor_property (GladeWidget * widget,
                                     const gchar * property,
                                     gboolean packing, gboolean use_command)
{
  GladeEditorProperty *eprop;
  GladeProperty       *prop;
  GladePropertyClass  *pclass;

  if (packing)
    prop = glade_widget_get_pack_property (widget, property);
  else
    prop = glade_widget_get_property (widget, property);

  g_return_val_if_fail (GLADE_IS_PROPERTY (prop), NULL);
  pclass = glade_property_get_class (prop);

  eprop = glade_widget_adaptor_create_eprop (widget->priv->adaptor,
                                             pclass, use_command);
  glade_editor_property_load (eprop, prop);

  return eprop;
}

/**
 * glade_widget_generate_path_name:
 * @widget: A #GladeWidget
 *
 * Creates a user friendly name to describe project widgets
 *
 * Returns: A newly allocated string
 */
gchar *
glade_widget_generate_path_name (GladeWidget * widget)
{
  GString *string;
  GladeWidget *iter;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

  string = g_string_new (widget->priv->name);

  for (iter = widget->priv->parent; iter; iter = iter->priv->parent)
    {
      gchar *str = g_strdup_printf ("%s:", iter->priv->name);
      g_string_prepend (string, str);
      g_free (str);
    }

  return g_string_free (string, FALSE);
}

void
glade_widget_set_support_warning (GladeWidget * widget, const gchar * warning)
{
  g_return_if_fail (GLADE_IS_WIDGET (widget));

  if (widget->priv->support_warning)
    g_free (widget->priv->support_warning);
  widget->priv->support_warning = g_strdup (warning);

  g_object_notify_by_pspec (G_OBJECT (widget), properties[PROP_SUPPORT_WARNING]);
}

G_CONST_RETURN gchar *
glade_widget_support_warning (GladeWidget *widget)
{
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

  return widget->priv->support_warning;
}

/**
 * glade_widget_lock:
 * @widget: A #GladeWidget
 * @locked: The #GladeWidget to lock
 *
 * Sets @locked to be in a locked up state
 * spoken for by @widget, locked widgets cannot
 * be removed from the project until unlocked.
 *
 */
void
glade_widget_lock (GladeWidget * widget, GladeWidget * locked)
{
  g_return_if_fail (GLADE_IS_WIDGET (widget));
  g_return_if_fail (GLADE_IS_WIDGET (locked));
  g_return_if_fail (locked->priv->lock == NULL);

  locked->priv->lock = widget;
  widget->priv->locked_widgets = g_list_prepend (widget->priv->locked_widgets, locked);
}

/**
 * glade_widget_unlock:
 * @widget: A #GladeWidget
 *
 * Unlocks @widget so that it can be removed
 * from the project again
 *
 */
void
glade_widget_unlock (GladeWidget * widget)
{
  GladeWidget *lock;

  g_return_if_fail (GLADE_IS_WIDGET (widget));
  g_return_if_fail (GLADE_IS_WIDGET (widget->priv->lock));

  lock = widget->priv->lock;

  lock->priv->locked_widgets =
    g_list_remove (lock->priv->locked_widgets, widget);

  widget->priv->lock = NULL;
}


GladeWidget *
glade_widget_get_locker (GladeWidget *widget)
{
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

  return widget->priv->lock;
}

GList *
glade_widget_list_locked_widgets (GladeWidget *widget)
{
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

  return g_list_copy (widget->priv->locked_widgets);
}


/**
 * glade_widget_support_changed:
 * @widget: A #GladeWidget
 *
 * Notifies that support metadata has changed on the widget.
 *
 */
void
glade_widget_support_changed (GladeWidget * widget)
{
  g_return_if_fail (GLADE_IS_WIDGET (widget));

  g_signal_emit (widget, glade_widget_signals[SUPPORT_CHANGED], 0);
}

/**
 * glade_widget_get_signal_model:
 * @widget: A #GladeWidget
 * 
 * Returns: a GtkTreeModel that can be used to view the widget's signals.
 *          The signal model is owned by the #GladeWidget.
 */
GtkTreeModel *
glade_widget_get_signal_model (GladeWidget *widget)
{
	if (!widget->priv->signal_model)
	{
		widget->priv->signal_model = glade_signal_model_new (widget, 
                                                         widget->priv->signals);
	}
	return widget->priv->signal_model;
}

GList *
glade_widget_get_properties (GladeWidget *widget)
{
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

  return widget->priv->properties;
}

GList *
glade_widget_get_packing_properties (GladeWidget *widget)
{
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

  return widget->priv->packing_properties;
}
