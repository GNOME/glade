#ifndef __GLADE_PROPERTY_CLASS_H__
#define __GLADE_PROPERTY_CLASS_H__

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include <gladeui/glade-xml-utils.h>

G_BEGIN_DECLS

/* The GladePropertyClass structure parameters of a GladeProperty.
 * All entries in the GladeEditor are GladeProperties (except signals)
 * All GladeProperties are associated with a GParamSpec.
 */
#define GLADE_PROPERTY_CLASS(gpc)     ((GladePropertyClass *) gpc)
#define GLADE_IS_PROPERTY_CLASS(gpc)  (gpc != NULL)


/**
 * GLADE_PROPERTY_CLASS_IS_TYPE:
 * gpc: A #GladePropertyClass
 * type: The #GladeEditorPageType to query
 *
 * Checks if @gpc is good to be loaded as @type
 */
#define GLADE_PROPERTY_CLASS_IS_TYPE(gpc, type)                                     \
  (((type) == GLADE_PAGE_GENERAL &&                                                 \
    !glade_property_class_common (gpc) &&                                           \
    !glade_property_class_get_is_packing (gpc) &&                                   \
    !glade_property_class_atk (gpc)) ||                                             \
   ((type) == GLADE_PAGE_COMMON  && glade_property_class_common (gpc))    ||        \
   ((type) == GLADE_PAGE_PACKING && glade_property_class_get_is_packing (gpc))   || \
   ((type) == GLADE_PAGE_ATK     && glade_property_class_atk (gpc))       ||        \
   ((type) == GLADE_PAGE_QUERY   && glade_property_class_query (gpc)))

/**
 * GPC_VERSION_CHECK:
 * @klass: A #GladePropertyClass
 * @major_version: The major version to check
 * @minor_version: The minor version to check
 *
 * Evaluates to %TRUE if @klass is available in its owning library version-@major_verion.@minor_version.
 *
 */
#define GPC_VERSION_CHECK(klass, major_version, minor_version)                          \
  ((glade_property_class_since_major (GLADE_PROPERTY_CLASS (klass)) == major_version) ? \
   (glade_property_class_since_minor (GLADE_PROPERTY_CLASS (klass)) <= minor_version) : \
   (glade_property_class_since_major (GLADE_PROPERTY_CLASS (klass)) <= major_version))


#define GPC_OBJECT_DELIMITER ", "
#define GPC_PROPERTY_NAMELEN 512  /* Enough space for a property name I think */

typedef struct _GladePropertyClass GladePropertyClass;


GladePropertyClass   *glade_property_class_new                     (GladeWidgetAdaptor  *adaptor,
                                                                    const gchar         *id);
GladePropertyClass   *glade_property_class_new_from_spec           (GladeWidgetAdaptor  *adaptor,
                                                                    GParamSpec          *spec);
GladePropertyClass   *glade_property_class_new_from_spec_full      (GladeWidgetAdaptor  *adaptor,
                                                                    GParamSpec          *spec,
                                                                    gboolean             need_handle);
GladePropertyClass   *glade_property_class_clone                   (GladePropertyClass  *property_class,
                                                                    gboolean             reset_version);
void                  glade_property_class_free                    (GladePropertyClass  *property_class);

void                  glade_property_class_set_adaptor             (GladePropertyClass  *property_class,
                                                                    GladeWidgetAdaptor  *adaptor);
GladeWidgetAdaptor   *glade_property_class_get_adaptor             (GladePropertyClass  *property_class);
void                  glade_property_class_set_pspec               (GladePropertyClass  *property_class,
                                                                    GParamSpec          *pspec);
GParamSpec           *glade_property_class_get_pspec               (GladePropertyClass  *property_class);
void                  glade_property_class_set_is_packing          (GladePropertyClass  *property_class,
                                                                    gboolean             is_packing);
gboolean              glade_property_class_get_is_packing          (GladePropertyClass  *property_class);
gboolean              glade_property_class_save                    (GladePropertyClass  *property_class);
gboolean              glade_property_class_save_always             (GladePropertyClass  *property_class);
gboolean              glade_property_class_is_visible              (GladePropertyClass  *property_class);
gboolean              glade_property_class_is_object               (GladePropertyClass  *property_class);
void                  glade_property_class_set_virtual             (GladePropertyClass  *property_class,
                                                                    gboolean             value);
gboolean              glade_property_class_get_virtual             (GladePropertyClass  *property_class);
void                  glade_property_class_set_ignore              (GladePropertyClass  *property_class,
                                                                    gboolean             ignore);
gboolean              glade_property_class_get_ignore              (GladePropertyClass  *property_class);
void                  glade_property_class_set_name                (GladePropertyClass  *property_class,
                                                                    const gchar         *name);
G_CONST_RETURN gchar *glade_property_class_get_name                (GladePropertyClass  *property_class);
void                  glade_property_class_set_tooltip             (GladePropertyClass  *property_class,
                                                                    const gchar         *tooltip);
G_CONST_RETURN gchar *glade_property_class_get_tooltip             (GladePropertyClass  *property_class);
G_CONST_RETURN gchar *glade_property_class_id                      (GladePropertyClass  *property_class);
gboolean              glade_property_class_themed_icon             (GladePropertyClass  *property_class);
void                  glade_property_class_set_construct_only      (GladePropertyClass  *property_class,
                                                                    gboolean             construct_only);
gboolean              glade_property_class_get_construct_only      (GladePropertyClass  *property_class);
G_CONST_RETURN GValue *glade_property_class_get_default            (GladePropertyClass  *property_class);
G_CONST_RETURN GValue *glade_property_class_get_original_default   (GladePropertyClass  *property_class);
gboolean              glade_property_class_translatable            (GladePropertyClass  *property_class);
gboolean              glade_property_class_needs_sync              (GladePropertyClass  *property_class);

gboolean              glade_property_class_query                   (GladePropertyClass  *property_class);
gboolean              glade_property_class_atk                     (GladePropertyClass  *property_class);
gboolean              glade_property_class_common                  (GladePropertyClass  *property_class);
gboolean              glade_property_class_parentless_widget       (GladePropertyClass  *property_class);
gboolean              glade_property_class_optional                (GladePropertyClass  *property_class);
gboolean              glade_property_class_optional_default        (GladePropertyClass  *property_class);
gboolean              glade_property_class_multiline               (GladePropertyClass  *property_class);
gboolean              glade_property_class_stock                   (GladePropertyClass  *property_class);
gboolean              glade_property_class_stock_icon              (GladePropertyClass  *property_class);
gboolean              glade_property_class_transfer_on_paste       (GladePropertyClass  *property_class);
gboolean              glade_property_class_custom_layout           (GladePropertyClass  *property_class);
gdouble               glade_property_class_weight                  (GladePropertyClass  *property_class);

G_CONST_RETURN gchar *glade_property_class_create_type             (GladePropertyClass  *property_class);

guint16               glade_property_class_since_major             (GladePropertyClass  *property_class);
guint16               glade_property_class_since_minor             (GladePropertyClass  *property_class);
gboolean              glade_property_class_deprecated              (GladePropertyClass  *property_class);

GValue             *glade_property_class_make_gvalue_from_string (GladePropertyClass  *property_class,
                                                                  const gchar         *string,
                                                                  GladeProject        *project);

gchar              *glade_property_class_make_string_from_gvalue (GladePropertyClass  *property_class,
                                                                  const GValue        *value);

GValue             *glade_property_class_make_gvalue_from_vl     (GladePropertyClass  *property_class,
                                                                  va_list              vl);

void                glade_property_class_set_vl_from_gvalue      (GladePropertyClass  *klass,
                                                                  GValue              *value,
                                                                  va_list              vl);

GValue             *glade_property_class_make_gvalue             (GladePropertyClass  *klass,
                                                                  ...);

void                glade_property_class_get_from_gvalue         (GladePropertyClass  *klass,
                                                                  GValue              *value,
                                                                  ...);

gboolean            glade_property_class_update_from_node        (GladeXmlNode        *node,
                                                                  GType                object_type,
                                                                  GladePropertyClass **property_class,
                                                                  const gchar         *domain);

GtkAdjustment      *glade_property_class_make_adjustment         (GladePropertyClass *property_class);

gboolean            glade_property_class_match                   (GladePropertyClass *klass,
                                                                  GladePropertyClass *comp);

gboolean            glade_property_class_void_value              (GladePropertyClass *klass,
                                                                  GValue             *value);

gint                glade_property_class_compare                 (GladePropertyClass *klass,
                                                                  const GValue       *value1,
                                                                  const GValue       *value2);

GValue             *glade_property_class_get_default_from_spec   (GParamSpec *spec);

void                glade_property_class_set_weights             (GList **properties, GType parent);

void                glade_property_class_load_defaults_from_spec (GladePropertyClass *property_class);

G_END_DECLS

#endif /* __GLADE_PROPERTY_CLASS_H__ */
