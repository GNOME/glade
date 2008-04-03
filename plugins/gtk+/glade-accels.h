#include <gladeui/glade.h>
#include <gtk/gtk.h>


#define	GLADE_TYPE_PARAM_ACCEL         (glade_param_accel_get_type())
#define	GLADE_TYPE_ACCEL_GLIST         (glade_accel_glist_get_type())
#define GLADE_TYPE_EPROP_ACCEL         (glade_eprop_accel_get_type())

#define GLADE_IS_PARAM_SPEC_ACCEL(pspec)       \
        (G_TYPE_CHECK_INSTANCE_TYPE ((pspec),  \
         GLADE_TYPE_PARAM_ACCEL))
#define GLADE_PARAM_SPEC_ACCEL(pspec)          \
        (G_TYPE_CHECK_INSTANCE_CAST ((pspec),  \
         GLADE_TYPE_PARAM_ACCEL, GladeParamSpecAccel))

typedef struct _GladeKey                GladeKey;
typedef struct _GladeParamSpecAccel     GladeParamSpecAccel;
typedef struct _GladeAccelInfo          GladeAccelInfo;

struct _GladeAccelInfo {
    guint key;
    GdkModifierType modifiers;
    gchar *signal;
};

struct _GladeKey {
	guint  value;
	gchar *name;
};

extern const GladeKey GladeKeys[];

#define  GLADE_KEYS_LAST_ALPHANUM    "9"
#define  GLADE_KEYS_LAST_EXTRA       "questiondown"
#define  GLADE_KEYS_LAST_KP          "KP_9"
#define  GLADE_KEYS_LAST_FKEY        "F35"

GType        glade_param_accel_get_type          (void) G_GNUC_CONST;
GType        glade_accel_glist_get_type          (void) G_GNUC_CONST;
GType        glade_eprop_accel_get_type          (void) G_GNUC_CONST;

GList       *glade_accel_list_copy         (GList         *accels);
void         glade_accel_list_free         (GList         *accels);


GParamSpec  *glade_param_spec_accel        (const gchar   *name,
					    const gchar   *nick,
					    const gchar   *blurb,
					    GType          widget_type,
					    GParamFlags    flags);

GParamSpec  *glade_standard_accel_spec     (void);

gboolean     glade_keyval_valid            (guint val);

gchar       *glade_accels_make_string      (GList *accels);
