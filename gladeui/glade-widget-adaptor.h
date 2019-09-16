#ifndef _GLADE_WIDGET_ADAPTOR_H_
#define _GLADE_WIDGET_ADAPTOR_H_

#include <gladeui/glade-xml-utils.h>
#include <gladeui/glade-property-def.h>
#include <gladeui/glade-editor-property.h>
#include <gladeui/glade-signal-def.h>
#include <gladeui/glade-catalog.h>
#include <gladeui/glade-editable.h>
#include <glib-object.h>
#include <gmodule.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GLADE_TYPE_WIDGET_ADAPTOR glade_widget_adaptor_get_type ()
G_DECLARE_DERIVABLE_TYPE (GladeWidgetAdaptor, glade_widget_adaptor, GLADE, WIDGET_ADAPTOR, GObject)

/**
 * GWA_DEPRECATED:
 * @obj: A #GladeWidgetAdaptor
 *
 * Checks whether this widget class is marked as deprecated
 */
#define GWA_DEPRECATED(obj) \
  ((obj) ? GLADE_WIDGET_ADAPTOR_GET_CLASS(obj)->deprecated : FALSE)

/**
 * GWA_VERSION_SINCE_MAJOR:
 * @obj: A #GladeWidgetAdaptor
 *
 * Checks major version in which this widget was introduced
 */
#define GWA_VERSION_SINCE_MAJOR(obj) \
  ((obj) ? GLADE_WIDGET_ADAPTOR_GET_CLASS(obj)->version_since_major : 0)

/**
 * GWA_VERSION_SINCE_MINOR:
 * @obj: A #GladeWidgetAdaptor
 *
 * Checks minor version in which this widget was introduced
 */
#define GWA_VERSION_SINCE_MINOR(obj) \
  ((obj) ? GLADE_WIDGET_ADAPTOR_GET_CLASS(obj)->version_since_minor : 0)

/**
 * GWA_VERSION_CHECK:
 * @adaptor: A #GladeWidgetAdaptor
 * @major_version: The major version to check
 * @minor_version: The minor version to check
 *
 * Evaluates to %TRUE if @adaptor is available in its owning library version-@major_verion.@minor_version.
 *
 */
#define GWA_VERSION_CHECK(adaptor, major_version, minor_version) \
  ((GWA_VERSION_SINCE_MAJOR (adaptor) == major_version) ?        \
   (GWA_VERSION_SINCE_MINOR (adaptor) <= minor_version) :        \
   (GWA_VERSION_SINCE_MAJOR (adaptor) <= major_version))

/**
 * GWA_DEPRECATED_SINCE_MAJOR:
 * @obj: A #GladeWidgetAdaptor
 *
 * Checks major version in which this widget was deprecated
 */
#define GWA_DEPRECATED_SINCE_MAJOR(obj) \
  ((obj) ? GLADE_WIDGET_ADAPTOR_GET_CLASS(obj)->deprecated_since_major : 0)

/**
 * GWA_DEPRECATED_SINCE_MINOR:
 * @obj: A #GladeWidgetAdaptor
 *
 * Checks minor version in which this widget was deprecated
 */
#define GWA_DEPRECATED_SINCE_MINOR(obj) \
  ((obj) ? GLADE_WIDGET_ADAPTOR_GET_CLASS(obj)->deprecated_since_minor : 0)

/**
 * GWA_DEPRECATED_SINCE_CHECK:
 * @adaptor: A #GladeWidgetAdaptor
 * @major_version: The major version to check
 * @minor_version: The minor version to check
 *
 * Evaluates to %TRUE if @adaptor is deprecated in its owning library version-@major_verion.@minor_version.
 *
 */
#define GWA_DEPRECATED_SINCE_CHECK(adaptor, major_version, minor_version)           \
  ((GWA_DEPRECATED_SINCE_MAJOR (adaptor) || GWA_DEPRECATED_SINCE_MINOR (adaptor)) ? \
    ((GWA_DEPRECATED_SINCE_MAJOR (adaptor) == major_version)  ?                     \
      (GWA_DEPRECATED_SINCE_MINOR (adaptor) <= minor_version)  :                    \
      (GWA_DEPRECATED_SINCE_MAJOR (adaptor) <= major_version)) :                    \
    FALSE)

/**
 * GWA_IS_TOPLEVEL:
 * @obj: A #GladeWidgetAdaptor
 *
 * Checks whether this widget class has been marked as
 * a toplevel one.
 */
#define GWA_IS_TOPLEVEL(obj) \
  ((obj) ? GLADE_WIDGET_ADAPTOR_GET_CLASS(obj)->toplevel : FALSE)

/**
 * GWA_USE_PLACEHOLDERS:
 * @obj: A #GladeWidgetAdaptor
 *
 * Checks whether this widget class has been marked to
 * use placeholders in child widget operations
 */
#define GWA_USE_PLACEHOLDERS(obj) \
  ((obj) ? GLADE_WIDGET_ADAPTOR_GET_CLASS(obj)->use_placeholders : FALSE)


/**
 * GWA_DEFAULT_WIDTH:
 * @obj: A #GladeWidgetAdaptor
 *
 * Returns: the default width to be used when this widget
 * is toplevel in the GladeDesignLayout
 */
#define GWA_DEFAULT_WIDTH(obj) \
  ((obj) ? GLADE_WIDGET_ADAPTOR_GET_CLASS(obj)->default_width : -1)


/**
 * GWA_DEFAULT_HEIGHT:
 * @obj: A #GladeWidgetAdaptor
 *
 * Returns: the default width to be used when this widget
 * is toplevel in the GladeDesignLayout
 */
#define GWA_DEFAULT_HEIGHT(obj) \
  ((obj) ? GLADE_WIDGET_ADAPTOR_GET_CLASS(obj)->default_height : -1)


/**
 * GWA_SCROLLABLE_WIDGET:
 * @obj: A #GladeWidgetAdaptor
 *
 * Checks whether this is a GtkWidgetClass with scrolling capabilities.
 */
#define GWA_SCROLLABLE_WIDGET(obj)                   \
  ((obj) ?                                           \
   g_type_is_a (glade_widget_adaptor_get_object_type \
                (GLADE_WIDGET_ADAPTOR (obj)),        \
                GTK_TYPE_SCROLLABLE) : FALSE)

/**
 * GWA_GET_CLASS:
 * @type: A #GType
 *
 * Shorthand for referencing glade adaptor classes from
 * the plugin eg. GWA_GET_CLASS (GTK_TYPE_CONTAINER)->post_create (adaptor...
 */
#define GWA_GET_CLASS(type)                                                   \
  (((type) == G_TYPE_OBJECT) ?                                                \
   (GladeWidgetAdaptorClass *)g_type_class_peek (GLADE_TYPE_WIDGET_ADAPTOR) : \
   GLADE_WIDGET_ADAPTOR_GET_CLASS (glade_widget_adaptor_get_by_type(type)))

/**
 * GWA_GET_OCLASS:
 * @type: A #GType.
 *
 * Same as GWA_GET_CLASS but casted to GObjectClass
 */
#define GWA_GET_OCLASS(type) ((GObjectClass*)GWA_GET_CLASS(type))


#define GLADE_VALID_CREATE_REASON(reason) (reason >= 0 && reason < GLADE_CREATE_REASONS)

/**
 * GLADE_WIDGET_ADAPTOR_INSTANTIABLE_PREFIX:
 *
 * Class prefix used for abstract classes (ie GtkBin -> GladeInstantiableGtkBin)
 */
#define GLADE_WIDGET_ADAPTOR_INSTANTIABLE_PREFIX "GladeInstantiable"

/**
 * GladeCreateReason:
 * @GLADE_CREATE_USER: Was created at the user's request
 *                     (this is a good time to set any properties
 *                     or add children to the project; like GtkFrame's 
 *                     label for example).
 * @GLADE_CREATE_COPY: Was created as a result of the copy/paste 
 *                     mechanism, at this point you can count on glade
 *                     to follow up with properties and children on 
 *                     its own.
 * @GLADE_CREATE_LOAD: Was created during the load process.
 * @GLADE_CREATE_REBUILD: Was created as a replacement for another project 
 *                        object; this only happens when the user is 
 *                        changing a property that is marked by the type 
 *                        system as G_PARAM_SPEC_CONSTRUCT_ONLY.
 * @GLADE_CREATE_REASONS: Never used.
 *
 * These are the reasons your #GladePostCreateFunc can be called.
 */
typedef enum
{
  GLADE_CREATE_USER = 0,
  GLADE_CREATE_COPY,
  GLADE_CREATE_LOAD,
  GLADE_CREATE_REBUILD,
  GLADE_CREATE_REASONS
} GladeCreateReason;

/**
 * GladeCreateWidgetFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @first_property_name: the name of the first property
 * @var_args: the value of the first property, followed optionally by more
 *  name/value pairs, followed by %NULL
 *
 * This entry point allows the backend to create a specialized GladeWidget
 * derived object for handling instances in the core.
 *
 * Returns: A newly created #GladeWidget for the said adaptor.
 */
typedef GladeWidget * (* GladeCreateWidgetFunc) (GladeWidgetAdaptor *adaptor,
                                                 const gchar        *first_property_name,
                                                 va_list             var_args);

/**
 * GladeSetPropertyFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @object: The #GObject
 * @property_name: The property identifier
 * @value: The #GValue
 *
 * This delagate function is used to apply the property value on
 * the runtime object.
 *
 * Sets @value on @object for a given #GladePropertyDef
 */
typedef void     (* GladeSetPropertyFunc)    (GladeWidgetAdaptor *adaptor,
                                              GObject            *object,
                                              const gchar        *property_name,
                                              const GValue       *value);

/**
 * GladeGetPropertyFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @object: The #GObject
 * @property_name: The property identifier
 * @value: The #GValue
 *
 * Gets @value on @object for a given #GladePropertyDef
 */
typedef void     (* GladeGetPropertyFunc)    (GladeWidgetAdaptor *adaptor,
                                              GObject            *object,
                                              const gchar        *property_name,
                                              GValue             *value);

/**
 * GladeVerifyPropertyFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @object: The #GObject
 * @property_name: The property identifier
 * @value: The #GValue
 *
 * This delagate function is always called whenever setting any
 * properties with the exception of load time, and copy/paste time
 * (basicly the two places where we recreate a hierarchy that we
 * already know "works") its basicly an optional backend provided
 * boundry checker for properties.
 *
 * Returns: whether or not its OK to set @value on @object
 */
typedef gboolean (* GladeVerifyPropertyFunc)      (GladeWidgetAdaptor *adaptor,
                                                   GObject            *object,
                                                   const gchar        *property_name,
                                                   const GValue       *value);

/**
 * GladeChildSetPropertyFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @container: The #GObject container
 * @child: The #GObject child
 * @property_name: The property name
 * @value: The #GValue
 *
 * Called to set the packing property @property_name to @value
 * on the @child object of @container.
 */
typedef void   (* GladeChildSetPropertyFunc)      (GladeWidgetAdaptor *adaptor,
                                                   GObject            *container,
                                                   GObject            *child,
                                                   const gchar        *property_name,
                                                   const GValue       *value);

/**
 * GladeChildGetPropertyFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @container: The #GObject container
 * @child: The #GObject child
 * @property_name: The property name
 * @value: The #GValue
 *
 * Called to get the packing property @property_name
 * on the @child object of @container into @value.
 */
typedef void   (* GladeChildGetPropertyFunc)      (GladeWidgetAdaptor *adaptor,
                                                   GObject            *container,
                                                   GObject            *child,
                                                   const gchar        *property_name,
                                                   GValue             *value);

/**
 * GladeChildVerifyPropertyFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @container: The #GObject container
 * @child: The #GObject child
 * @property_name: The property name
 * @value: The #GValue
 *
 * This delagate function is always called whenever setting any
 * properties with the exception of load time, and copy/paste time
 * (basicly the two places where we recreate a hierarchy that we
 * already know "works") its basicly an optional backend provided
 * boundry checker for properties.
 *
 * Returns: whether or not its OK to set @value on @object
 */
typedef gboolean (* GladeChildVerifyPropertyFunc) (GladeWidgetAdaptor *adaptor,
                                                   GObject            *container,
                                                   GObject            *child,
                                                   const gchar        *property_name,
                                                   const GValue       *value);

/**
 * GladeAddChildVerifyFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @container: A #GObject container
 * @child: A #GObject child
 * @user_feedback: whether a notification dialog should be
 * presented in the case that the child cannot not be added.
 *
 * Checks whether @child can be added to @container.
 *
 * If @user_feedback is %TRUE and @child cannot be
 * added then this shows a notification dialog to the user 
 * explaining why.
 *
 * Returns: whether @child can be added to @container.
 */
typedef gboolean (* GladeAddChildVerifyFunc)      (GladeWidgetAdaptor *adaptor,
                                                   GObject            *container,
                                                   GObject            *child,
                                                   gboolean            user_feedback);

/**
 * GladeGetChildrenFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @container: A #GObject container
 *
 * A function called to get @containers children.
 *
 * Returns: A #GList of #GObject children.
 */
typedef GList   *(* GladeGetChildrenFunc)         (GladeWidgetAdaptor *adaptor,
                                                   GObject            *container);

/**
 * GladeAddChildFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @container: A #GObject parent container
 * @child: A #GObject child
 *
 * Called to add @child to @container.
 */
typedef void     (* GladeAddChildFunc)            (GladeWidgetAdaptor *adaptor,
                                                   GObject            *container,
                                                   GObject            *child);

/**
 * GladeRemoveChildFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @container: A #GObject parent container
 * @child: A #GObject child
 *
 * Called to remove @child from @container.
 */
typedef void     (* GladeRemoveChildFunc)         (GladeWidgetAdaptor *adaptor,
                                                   GObject            *container,
                                                   GObject            *child);

/**
 * GladeReplaceChildFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @container: A #GObject container
 * @old_obj: The old #GObject child
 * @new_obj: The new #GObject child to take its place
 * 
 * Called to swap placeholders with project objects
 * in containers.
 */
typedef void     (* GladeReplaceChildFunc)        (GladeWidgetAdaptor *adaptor,
                                                   GObject            *container,  
                                                   GObject            *old_obj,
                                                   GObject            *new_obj);


G_GNUC_BEGIN_IGNORE_DEPRECATIONS
/**
 * GladeConstructObjectFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @n_parameters: amount of construct parameters
 * @parameters: array of construct #GParameter args to create 
 *              the new object with.
 *
 * This function is called to construct a GObject instance.
 * (for language bindings that may need to construct a wrapper
 * object).
 *
 * Returns: A newly created #GObject
 */
typedef GObject *(* GladeConstructObjectFunc)     (GladeWidgetAdaptor *adaptor,
                                                   guint               n_parameters,
                                                   GParameter         *parameters);
G_GNUC_END_IGNORE_DEPRECATIONS

/**
 * GladeDestroyObjectFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @object: The #GObject to destroy
 *
 * This function is called to break any additional references to
 * a GObject instance. Note that this function is not responsible
 * for calling g_object_unref() on @object, the reference count
 * of @object belongs to it's #GladeWidget wrapper.
 *
 * The #GtkWidget adaptor will call gtk_widget_destroy() before
 * chaining up in this function.
 *
 * If your adaptor adds any references in any way at
 * #GladePostCreateFunc time or #GladeConstructObjectFunc
 * time, then this function must be implemented to also
 * remove that reference.
 *
 */
typedef void (* GladeDestroyObjectFunc) (GladeWidgetAdaptor *adaptor,
                                         GObject            *object);


/**
 * GladePostCreateFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @object: a #GObject
 * @reason: a #GladeCreateReason
 *
 * This function is called exactly once for any project object
 * instance and can be for any #GladeCreateReason.
 */
typedef void     (* GladePostCreateFunc)          (GladeWidgetAdaptor *adaptor,
                                                   GObject            *object,
                                                   GladeCreateReason   reason);

/**
 * GladeGetInternalFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @object: A #GObject composite object
 * @internal_name: A string identifier
 *
 * Called to lookup child in composite @object parent by @internal_name.
 *
 * Returns: The specified internal widget.
 */
typedef GObject *(* GladeGetInternalFunc)         (GladeWidgetAdaptor *adaptor,
                                                   GObject            *object,
                                                   const gchar        *internal_name);

/**
 * GladeActionActivateFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @object: The #GObject
 * @action_path: The action path
 *
 * This delagate function is used to catch actions from the core.
 *
 */
typedef void     (* GladeActionActivateFunc)  (GladeWidgetAdaptor *adaptor,
                                               GObject            *object,
                                               const gchar        *action_path);

/**
 * GladeChildActionActivateFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @container: The #GtkContainer
 * @object: The #GObject
 * @action_path: The action path
 *
 * This delagate function is used to catch packing actions from the core.
 *
 */
typedef void     (* GladeChildActionActivateFunc) (GladeWidgetAdaptor *adaptor,
                                                   GObject            *container,
                                                   GObject            *object,
                                                   const gchar        *action_path);


/**
 * GladeActionSubmenuFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @object: The #GObject
 * @action_path: The action path
 *
 * This delagate function is used to create dynamically customized
 * submenus. Called only for actions that dont have children.
 *
 */
typedef GtkWidget  *(* GladeActionSubmenuFunc)  (GladeWidgetAdaptor *adaptor,
                                                 GObject            *object,
                                                 const gchar        *action_path);


/**
 * GladeDependsFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @widget: A #GladeWidget of the adaptor
 * @another: another #GladeWidget
 *
 * Checks whether @widget depends on @another to be placed earlier in
 * the glade file.
 *
 * Returns: whether @widget depends on @another being parsed first in
 * the resulting glade file.
 */
typedef gboolean (* GladeDependsFunc) (GladeWidgetAdaptor *adaptor,
                                       GladeWidget        *widget,
                                       GladeWidget        *another);



/**
 * GladeReadWidgetFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @widget: The #GladeWidget
 * @node: The #GladeXmlNode
 *
 * This function is called to update @widget from @node.
 *
 */
typedef void     (* GladeReadWidgetFunc) (GladeWidgetAdaptor *adaptor,
                                          GladeWidget        *widget,
                                          GladeXmlNode       *node);

/**
 * GladeWriteWidgetFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @widget: The #GladeWidget
 * @context: The #GladeXmlContext
 * @node: The #GladeXmlNode
 *
 * This function is called to fill in @node from @widget.
 *
 */
typedef void     (* GladeWriteWidgetFunc) (GladeWidgetAdaptor *adaptor,
                                           GladeWidget        *widget,
                                           GladeXmlContext    *context,
                                           GladeXmlNode       *node);


/**
 * GladeCreateEPropFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @def: The #GladePropertyDef to be edited
 * @use_command: whether to use the GladeCommand interface
 * to commit property changes
 * 
 * Creates a GladeEditorProperty to edit @klass
 *
 * Returns: A newly created #GladeEditorProperty
 */
typedef GladeEditorProperty *(* GladeCreateEPropFunc) (GladeWidgetAdaptor *adaptor,
                                                       GladePropertyDef   *def,
                                                       gboolean            use_command);

/**
 * GladeStringFromValueFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @def: The #GladePropertyDef 
 * @value: The #GValue to convert to a string
 * 
 * For normal properties this is used to serialize
 * property values, for custom properties (only when new pspecs are 
 * introduced) its needed for value comparisons in boxed pspecs 
 * and also to update the UI for undo/redo items etc.
 *
 * Returns: A newly allocated string representation of @value
 */
typedef gchar   *(* GladeStringFromValueFunc) (GladeWidgetAdaptor *adaptor,
                                               GladePropertyDef   *def,
                                               const GValue       *value);



/**
 * GladeCreateEditableFunc:
 * @adaptor: A #GladeWidgetAdaptor
 * @type: The #GladeEditorPageType
 * 
 * This is used to allow the backend to override the way an
 * editor page is layed out (note that editor widgets are created
 * on demand and not at startup).
 *
 * Returns: A new #GladeEditable widget
 */
typedef GladeEditable *(* GladeCreateEditableFunc) (GladeWidgetAdaptor   *adaptor,
                                                    GladeEditorPageType   type);

struct _GladeWidgetAdaptorClass
{
  GObjectClass               parent_class;

  guint16                    version_since_major; /* Version in which this widget was */
  guint16                    version_since_minor; /* introduced.                      */

  gint16                     default_width;       /* Default width in GladeDesignLayout */
  gint16                     default_height;      /* Default height in GladeDesignLayout */

  guint                      deprecated : 1;          /* If this widget is currently
                                                       * deprecated
                                                       */
  guint                      toplevel : 1;             /* If this class is toplevel */

  guint                      use_placeholders : 1;     /* Whether or not to use placeholders
                                                        * to interface with child widgets.
                                                        */

  GladeCreateWidgetFunc      create_widget;  /* Creates a GladeWidget for this adaptor */

  GladeConstructObjectFunc   construct_object;  /* Object constructor
                                                 */

  GladePostCreateFunc        deep_post_create;   /* Executed after widget creation: 
                                                  * plugins use this to setup various
                                                  * support codes (adaptors must always
                                                  * chain up in this stage of instantiation).
                                                  */

  GladePostCreateFunc        post_create;   /* Executed after widget creation: 
                                             * plugins use this to setup various
                                             * support codes (adaptors are free here
                                             * to chain up or not in this stage of
                                             * instantiation).
                                             */

  GladeGetInternalFunc       get_internal_child; /* Retrieves the the internal child
                                                  * of the given name.
                                                  */

  /* Delagate to verify if this is a valid value for this property,
   * if this function exists and returns FALSE, then glade_property_set
   * will abort before making any changes
   */
  GladeVerifyPropertyFunc verify_property;
        
  /* An optional backend function used instead of g_object_set()
   * virtual properties must be handled with this function.
   */
  GladeSetPropertyFunc set_property;

  /* An optional backend function used instead of g_object_get()
   * virtual properties must be handled with this function.
   *
   * Note that since glade knows what the property values are 
   * at all times regardless of the objects copy, this is currently
   * only used to obtain the values of packing properties that are
   * set by the said object's parent at "container_add" time.
   */
  GladeGetPropertyFunc get_property;

  GladeAddChildVerifyFunc    add_verify;       /* Checks if a child can be added */
  GladeAddChildFunc          add;              /* Adds a new child of this type */
  GladeRemoveChildFunc       remove;           /* Removes a child from the container */
  GladeGetChildrenFunc       get_children;     /* Returns a list of direct children for
                                                * this support type.
                                                */

  GladeChildVerifyPropertyFunc child_verify_property; /* A boundry checker for 
                                                       * packing properties 
                                                       */
  GladeChildSetPropertyFunc    child_set_property; /* Sets/Gets a packing property */
  GladeChildGetPropertyFunc    child_get_property; /* for this child */
  GladeReplaceChildFunc        replace_child;  /* This method replaces a 
                                                * child widget with
                                                * another one: it's used to
                                                * replace a placeholder with
                                                * a widget and viceversa.
                                                */
        
  GladeActionActivateFunc      action_activate;       /* This method is used to catch actions */
  GladeChildActionActivateFunc child_action_activate; /* This method is used to catch packing actions */
  GladeActionSubmenuFunc       action_submenu;        /* Delagate function to create dynamic submenus
                                                       * in action menus. */
  GladeDependsFunc             depends;           /* Periodically sort widgets in the project */
  GladeReadWidgetFunc          read_widget;       /* Reads widget attributes from xml */
  GladeWriteWidgetFunc         write_widget;      /* Writes widget attributes to the xml */
  GladeReadWidgetFunc          read_child;        /* Reads widget attributes from xml */
  GladeWriteWidgetFunc         write_child;       /* Writes widget attributes to the xml */
  GladeCreateEPropFunc         create_eprop;      /* Creates a GladeEditorProperty */
  GladeStringFromValueFunc     string_from_value; /* Creates a string for a value */
  GladeCreateEditableFunc      create_editable;   /* Creates a page for the editor */

  GladeDestroyObjectFunc       destroy_object;    /* Object destructor */
  GladeWriteWidgetFunc         write_widget_after;/* Writes widget attributes to the xml (after children) */

  guint16                      deprecated_since_major;
  guint16                      deprecated_since_minor;

  void   (* glade_reserved1)   (void);
  void   (* glade_reserved2)   (void);
  void   (* glade_reserved3)   (void);
  void   (* glade_reserved4)   (void);
  void   (* glade_reserved5)   (void);
};

#define glade_widget_adaptor_create_widget(adaptor, query, ...) \
    (glade_widget_adaptor_create_widget_real (query, "adaptor", adaptor, __VA_ARGS__));

GType                 glade_widget_adaptor_get_object_type  (GladeWidgetAdaptor   *adaptor);
const gchar *glade_widget_adaptor_get_name         (GladeWidgetAdaptor   *adaptor);
const gchar *glade_widget_adaptor_get_generic_name (GladeWidgetAdaptor   *adaptor);
const gchar *glade_widget_adaptor_get_display_name (GladeWidgetAdaptor   *adaptor);
const gchar *glade_widget_adaptor_get_title        (GladeWidgetAdaptor   *adaptor);
const gchar *glade_widget_adaptor_get_icon_name    (GladeWidgetAdaptor   *adaptor);
const gchar *glade_widget_adaptor_get_missing_icon (GladeWidgetAdaptor   *adaptor);
const gchar *glade_widget_adaptor_get_catalog      (GladeWidgetAdaptor   *adaptor);
const gchar *glade_widget_adaptor_get_book         (GladeWidgetAdaptor   *adaptor);
const GList *glade_widget_adaptor_get_properties   (GladeWidgetAdaptor   *adaptor);
const GList *glade_widget_adaptor_get_packing_props(GladeWidgetAdaptor   *adaptor);
const GList *glade_widget_adaptor_get_signals      (GladeWidgetAdaptor   *adaptor);

GList                *glade_widget_adaptor_list_adaptors    (void);

GladeWidgetAdaptor   *glade_widget_adaptor_from_catalog     (GladeCatalog         *catalog,
                                                             GladeXmlNode         *class_node,
                                                             GModule              *module);

void                  glade_widget_adaptor_register         (GladeWidgetAdaptor   *adaptor);
 
GladeWidget          *glade_widget_adaptor_create_internal  (GladeWidget          *parent,
                                                             GObject              *internal_object,
                                                             const gchar          *internal_name,
                                                             const gchar          *parent_name,
                                                             gboolean              anarchist,
                                                             GladeCreateReason     reason);

GladeWidget          *glade_widget_adaptor_create_widget_real (gboolean            query, 
                                                               const gchar        *first_property,
                                                               ...);


GladeWidgetAdaptor   *glade_widget_adaptor_get_by_name        (const gchar        *name);
GladeWidgetAdaptor   *glade_widget_adaptor_get_by_type        (GType               type);
GladeWidgetAdaptor   *glade_widget_adaptor_from_pspec         (GladeWidgetAdaptor *adaptor,
                                                               GParamSpec         *pspec);

GladePropertyDef     *glade_widget_adaptor_get_property_def   (GladeWidgetAdaptor *adaptor,
                                                               const gchar        *name);
GladePropertyDef     *glade_widget_adaptor_get_pack_property_def (GladeWidgetAdaptor *adaptor,
                                                                  const gchar        *name);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
GParameter           *glade_widget_adaptor_default_params     (GladeWidgetAdaptor *adaptor,
                                                               gboolean            construct,
                                                               guint              *n_params);
GObject              *glade_widget_adaptor_construct_object   (GladeWidgetAdaptor *adaptor,
                                                               guint               n_parameters,
                                                               GParameter         *parameters);
G_GNUC_END_IGNORE_DEPRECATIONS
void                  glade_widget_adaptor_destroy_object     (GladeWidgetAdaptor *adaptor,
                                                               GObject            *object);
void                  glade_widget_adaptor_post_create        (GladeWidgetAdaptor *adaptor,
                                                               GObject            *object,
                                                               GladeCreateReason   reason);
GObject              *glade_widget_adaptor_get_internal_child (GladeWidgetAdaptor *adaptor,
                                                               GObject            *object,
                                                               const gchar        *internal_name);
void                  glade_widget_adaptor_set_property       (GladeWidgetAdaptor *adaptor,
                                                               GObject            *object,
                                                               const gchar        *property_name,
                                                               const GValue       *value);
void                  glade_widget_adaptor_get_property       (GladeWidgetAdaptor *adaptor,
                                                               GObject            *object,
                                                               const gchar        *property_name,
                                                               GValue             *value);
gboolean              glade_widget_adaptor_verify_property    (GladeWidgetAdaptor *adaptor,
                                                               GObject            *object,
                                                               const gchar        *property_name,
                                                               const GValue       *value);
gboolean              glade_widget_adaptor_add_verify         (GladeWidgetAdaptor *adaptor,
                                                               GObject            *container,
                                                               GObject            *child,
                                                               gboolean            user_feedback);
void                  glade_widget_adaptor_add                (GladeWidgetAdaptor *adaptor,
                                                               GObject            *container,
                                                               GObject            *child);
void                  glade_widget_adaptor_remove             (GladeWidgetAdaptor *adaptor,
                                                               GObject            *container,
                                                               GObject            *child);
GList                *glade_widget_adaptor_get_children       (GladeWidgetAdaptor *adaptor,
                                                               GObject            *container);
gboolean              glade_widget_adaptor_has_child          (GladeWidgetAdaptor *adaptor,
                                                               GObject            *container,
                                                               GObject            *child);
void                  glade_widget_adaptor_child_set_property (GladeWidgetAdaptor *adaptor,
                                                               GObject            *container,
                                                               GObject            *child,
                                                               const gchar        *property_name,
                                                               const GValue       *value);
void                  glade_widget_adaptor_child_get_property (GladeWidgetAdaptor *adaptor,
                                                               GObject            *container,
                                                               GObject            *child,
                                                               const gchar        *property_name,
                                                               GValue             *value);
gboolean              glade_widget_adaptor_child_verify_property (GladeWidgetAdaptor *adaptor,
                                                                  GObject            *container,
                                                                  GObject            *child,
                                                                  const gchar        *property_name,
                                                                  const GValue       *value);
void                  glade_widget_adaptor_replace_child      (GladeWidgetAdaptor *adaptor,
                                                               GObject            *container,
                                                               GObject            *old_obj,
                                                               GObject            *new_obj);
gboolean              glade_widget_adaptor_query              (GladeWidgetAdaptor *adaptor);

const gchar *glade_widget_adaptor_get_packing_default(GladeWidgetAdaptor *child_adaptor,
                                                               GladeWidgetAdaptor *container_adaptor,
                                                               const gchar        *id);
gboolean              glade_widget_adaptor_is_container       (GladeWidgetAdaptor *adaptor);
gboolean              glade_widget_adaptor_action_add         (GladeWidgetAdaptor *adaptor,
                                                               const gchar *action_path,
                                                               const gchar *label,
                                                               const gchar *stock,
                                                               gboolean important);
gboolean              glade_widget_adaptor_pack_action_add    (GladeWidgetAdaptor *adaptor,
                                                               const gchar *action_path,
                                                               const gchar *label,
                                                               const gchar *stock,
                                                               gboolean important);
gboolean              glade_widget_adaptor_action_remove      (GladeWidgetAdaptor *adaptor,
                                                               const gchar *action_path);
gboolean              glade_widget_adaptor_pack_action_remove (GladeWidgetAdaptor *adaptor,
                                                               const gchar *action_path);
GList                *glade_widget_adaptor_actions_new        (GladeWidgetAdaptor *adaptor);
GList                *glade_widget_adaptor_pack_actions_new   (GladeWidgetAdaptor *adaptor);
void                  glade_widget_adaptor_action_activate    (GladeWidgetAdaptor *adaptor,
                                                               GObject            *object,
                                                               const gchar        *action_path);
void                  glade_widget_adaptor_child_action_activate (GladeWidgetAdaptor *adaptor,
                                                                  GObject            *container,
                                                                  GObject            *object,
                                                                  const gchar        *action_path);
GtkWidget            *glade_widget_adaptor_action_submenu        (GladeWidgetAdaptor *adaptor,
                                                                  GObject            *object,
                                                                  const gchar        *action_path);

G_DEPRECATED
gboolean              glade_widget_adaptor_depends            (GladeWidgetAdaptor *adaptor,
                                                               GladeWidget        *widget,
                                                               GladeWidget        *another);

void                  glade_widget_adaptor_read_widget        (GladeWidgetAdaptor *adaptor,
                                                               GladeWidget        *widget,
                                                               GladeXmlNode       *node);
void                  glade_widget_adaptor_write_widget       (GladeWidgetAdaptor *adaptor,
                                                               GladeWidget        *widget,
                                                               GladeXmlContext    *context,
                                                               GladeXmlNode       *node);
void                  glade_widget_adaptor_write_widget_after (GladeWidgetAdaptor *adaptor,
                                                               GladeWidget        *widget,
                                                               GladeXmlContext    *context,
                                                               GladeXmlNode       *node);
void                  glade_widget_adaptor_read_child         (GladeWidgetAdaptor *adaptor,
                                                               GladeWidget        *widget,
                                                               GladeXmlNode       *node);
void                  glade_widget_adaptor_write_child        (GladeWidgetAdaptor *adaptor,
                                                               GladeWidget        *widget,
                                                               GladeXmlContext    *context,
                                                               GladeXmlNode       *node);

GladeEditorProperty  *glade_widget_adaptor_create_eprop       (GladeWidgetAdaptor *adaptor,
                                                               GladePropertyDef   *def,
                                                               gboolean            use_command);
GladeEditorProperty  *glade_widget_adaptor_create_eprop_by_name (GladeWidgetAdaptor *adaptor,
                                                                 const gchar        *property_id,
                                                                 gboolean            packing,
                                                                 gboolean            use_command);

gchar                *glade_widget_adaptor_string_from_value  (GladeWidgetAdaptor *adaptor,
                                                               GladePropertyDef   *def,
                                                               const GValue       *value);
GladeEditable        *glade_widget_adaptor_create_editable    (GladeWidgetAdaptor *adaptor,
                                                               GladeEditorPageType type);
GladeSignalDef       *glade_widget_adaptor_get_signal_def     (GladeWidgetAdaptor *adaptor,
                                                               const gchar        *name);
GladeWidgetAdaptor   *glade_widget_adaptor_get_parent_adaptor (GladeWidgetAdaptor *adaptor);

gboolean              glade_widget_adaptor_has_internal_children (GladeWidgetAdaptor *adaptor);
const gchar          *glade_widget_adaptor_get_type_func      (GladeWidgetAdaptor *adaptor);
G_END_DECLS

#endif /* _GLADE_WIDGET_ADAPTOR_H_ */
