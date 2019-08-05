#ifndef __GLADE_PROPERTY_DEF_H__
#define __GLADE_PROPERTY_DEF_H__

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include <gladeui/glade-xml-utils.h>

G_BEGIN_DECLS

#define GLADE_TYPE_PROPERTY_DEF glade_property_def_get_type ()

/* The GladePropertyDef structure parameters of a GladeProperty.
 * All entries in the GladeEditor are GladeProperties (except signals)
 * All GladeProperties are associated with a GParamSpec.
 */
#define GLADE_PROPERTY_DEF(gpc)     ((GladePropertyDef *) gpc)
#define GLADE_IS_PROPERTY_DEF(gpc)  (gpc != NULL)

/**
 * GLADE_PROPERTY_DEF_IS_TYPE:
 * @gpc: A #GladePropertyDef
 * @type: The #GladeEditorPageType to query
 *
 * Checks if @gpc is good to be loaded as @type
 */
#define GLADE_PROPERTY_DEF_IS_TYPE(gpc, type)                                   \
  (((type) == GLADE_PAGE_GENERAL &&                                               \
    !glade_property_def_common (gpc) &&                                           \
    !glade_property_def_get_is_packing (gpc) &&                                   \
    !glade_property_def_atk (gpc)) ||                                             \
   ((type) == GLADE_PAGE_COMMON  && glade_property_def_common (gpc))    ||        \
   ((type) == GLADE_PAGE_PACKING && glade_property_def_get_is_packing (gpc))   || \
   ((type) == GLADE_PAGE_ATK     && glade_property_def_atk (gpc))       ||        \
   ((type) == GLADE_PAGE_QUERY   && glade_property_def_query (gpc)))

/**
 * GLADE_PROPERTY_DEF_VERSION_CHECK:
 * @def: A #GladePropertyDef
 * @major_version: The major version to check
 * @minor_version: The minor version to check
 *
 * Evaluates to %TRUE if @def is available in its owning library version-@major_verion.@minor_version.
 *
 */
#define GLADE_PROPERTY_DEF_VERSION_CHECK(def, major_version, minor_version)       \
  ((glade_property_def_since_major (GLADE_PROPERTY_DEF (def)) == major_version) ? \
   (glade_property_def_since_minor (GLADE_PROPERTY_DEF (def)) <= minor_version) : \
   (glade_property_def_since_major (GLADE_PROPERTY_DEF (def)) <= major_version))


#define GLADE_PROPERTY_DEF_OBJECT_DELIMITER ", "

typedef struct _GladePropertyDef GladePropertyDef;

GType                  glade_property_def_get_type                (void) G_GNUC_CONST;
GladePropertyDef      *glade_property_def_new                     (GladeWidgetAdaptor *adaptor,
                                                                   const gchar        *id);
GladePropertyDef      *glade_property_def_new_from_spec           (GladeWidgetAdaptor *adaptor,
                                                                   GParamSpec         *spec);
GladePropertyDef      *glade_property_def_new_from_spec_full      (GladeWidgetAdaptor *adaptor,
                                                                   GParamSpec         *spec,
                                                                   gboolean            need_handle);
GladePropertyDef      *glade_property_def_clone                   (GladePropertyDef   *property_def);
void                   glade_property_def_free                    (GladePropertyDef   *property_def);

void                   glade_property_def_set_adaptor             (GladePropertyDef   *property_def,
                                                                   GladeWidgetAdaptor *adaptor);
GladeWidgetAdaptor    *glade_property_def_get_adaptor             (GladePropertyDef   *property_def);
void                   glade_property_def_set_pspec               (GladePropertyDef   *property_def,
                                                                   GParamSpec         *pspec);
GParamSpec            *glade_property_def_get_pspec               (GladePropertyDef   *property_def);
void                   glade_property_def_set_is_packing          (GladePropertyDef   *property_def,
                                                                   gboolean            is_packing);
gboolean               glade_property_def_get_is_packing          (GladePropertyDef   *property_def);
gboolean               glade_property_def_save                    (GladePropertyDef   *property_def);
gboolean               glade_property_def_save_always             (GladePropertyDef   *property_def);
gboolean               glade_property_def_is_visible              (GladePropertyDef   *property_def);
gboolean               glade_property_def_is_object               (GladePropertyDef   *property_def);
void                   glade_property_def_set_virtual             (GladePropertyDef   *property_def,
                                                                   gboolean            value);
gboolean               glade_property_def_get_virtual             (GladePropertyDef   *property_def);
void                   glade_property_def_set_ignore              (GladePropertyDef   *property_def,
                                                                   gboolean            ignore);
gboolean               glade_property_def_get_ignore              (GladePropertyDef   *property_def);
void                   glade_property_def_set_name                (GladePropertyDef   *property_def,
                                                                   const gchar        *name);
const gchar  *glade_property_def_get_name                (GladePropertyDef   *property_def);
void                   glade_property_def_set_tooltip             (GladePropertyDef   *property_def,
                                                                   const gchar        *tooltip);
const gchar  *glade_property_def_get_tooltip             (GladePropertyDef   *property_def);
const gchar  *glade_property_def_id                      (GladePropertyDef   *property_def);
gboolean               glade_property_def_themed_icon             (GladePropertyDef   *property_def);
void                   glade_property_def_set_construct_only      (GladePropertyDef   *property_def,
                                                                   gboolean            construct_only);
gboolean               glade_property_def_get_construct_only      (GladePropertyDef   *property_def);
const GValue *glade_property_def_get_default             (GladePropertyDef   *property_def);
const GValue *glade_property_def_get_original_default    (GladePropertyDef   *property_def);
gboolean               glade_property_def_translatable            (GladePropertyDef   *property_def);
gboolean               glade_property_def_needs_sync              (GladePropertyDef   *property_def);

gboolean               glade_property_def_query                   (GladePropertyDef   *property_def);
gboolean               glade_property_def_atk                     (GladePropertyDef   *property_def);
gboolean               glade_property_def_common                  (GladePropertyDef   *property_def);
gboolean               glade_property_def_parentless_widget       (GladePropertyDef   *property_def);
gboolean               glade_property_def_optional                (GladePropertyDef   *property_def);
gboolean               glade_property_def_optional_default        (GladePropertyDef   *property_def);
gboolean               glade_property_def_multiline               (GladePropertyDef   *property_def);
gboolean               glade_property_def_stock                   (GladePropertyDef   *property_def);
gboolean               glade_property_def_stock_icon              (GladePropertyDef   *property_def);
gboolean               glade_property_def_transfer_on_paste       (GladePropertyDef   *property_def);
gboolean               glade_property_def_custom_layout           (GladePropertyDef   *property_def);
gdouble                glade_property_def_weight                  (GladePropertyDef   *property_def);

const gchar  *glade_property_def_create_type             (GladePropertyDef   *property_def);

guint16                glade_property_def_since_major             (GladePropertyDef   *property_def);
guint16                glade_property_def_since_minor             (GladePropertyDef   *property_def);
gboolean               glade_property_def_deprecated              (GladePropertyDef   *property_def);

GValue                *glade_property_def_make_gvalue_from_string (GladePropertyDef   *property_def,
                                                                   const gchar        *string,
                                                                   GladeProject       *project);

gchar                 *glade_property_def_make_string_from_gvalue (GladePropertyDef   *property_def,
                                                                   const GValue       *value);

GValue                *glade_property_def_make_gvalue_from_vl     (GladePropertyDef   *property_def,
                                                                   va_list             vl);

void                   glade_property_def_set_vl_from_gvalue      (GladePropertyDef   *property_def,
                                                                   GValue             *value,
                                                                   va_list             vl);

GValue                *glade_property_def_make_gvalue             (GladePropertyDef   *property_def,
                                                                   ...);

void                   glade_property_def_get_from_gvalue         (GladePropertyDef   *property_def,
                                                                   GValue             *value,
                                                                   ...);

gboolean               glade_property_def_update_from_node        (GladeXmlNode       *node,
                                                                   GType               object_type,
                                                                   GladePropertyDef  **property_def_ref,
                                                                   const gchar        *domain);

GtkAdjustment         *glade_property_def_make_adjustment         (GladePropertyDef   *property_def);

gboolean               glade_property_def_match                   (GladePropertyDef   *property_def,
                                                                   GladePropertyDef   *comp);

gboolean               glade_property_def_void_value              (GladePropertyDef   *property_def,
                                                                   GValue             *value);

gint                   glade_property_def_compare                 (GladePropertyDef   *property_def,
                                                                   const GValue       *value1,
                                                                   const GValue       *value2);

GValue                *glade_property_def_get_default_from_spec   (GParamSpec         *spec);

void                   glade_property_def_set_weights             (GList             **properties,
                                                                   GType               parent);

void                   glade_property_def_load_defaults_from_spec (GladePropertyDef   *property_def);

guint                  glade_property_def_make_flags_from_string  (GType               type,
                                                                   const char         *string);
G_END_DECLS

#endif /* __GLADE_PROPERTY_DEF_H__ */
