/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_BUILTINS_H__
#define __GLADE_BUILTINS_H__

#include <glib-object.h>
#include "glade.h"

G_BEGIN_DECLS

#define GLADE_TYPE_STOCK  (glade_standard_stock_get_type())

LIBGLADEUI_API GType       glade_standard_stock_get_type (void);

#define GLADE_IS_PARAM_SPEC_STOCK(pspec) \
        (G_TYPE_CHECK_INSTANCE_TYPE ((pspec), GLADE_TYPE_STOCK))



typedef struct _GladeParamSpecObjects   GladeParamSpecObjects;


#define	GLADE_TYPE_GLIST               (glade_glist_get_type())

#define	GLADE_TYPE_PARAM_OBJECTS       (glade_param_objects_get_type())
#define GLADE_IS_PARAM_SPEC_OBJECTS(pspec)     \
        (G_TYPE_CHECK_INSTANCE_TYPE ((pspec),  \
         GLADE_TYPE_PARAM_OBJECTS))
#define GLADE_PARAM_SPEC_OBJECTS(pspec)        \
        (G_TYPE_CHECK_INSTANCE_CAST ((pspec),  \
         GLADE_TYPE_PARAM_OBJECTS, GladeParamSpecObjects))


LIBGLADEUI_API GType        glade_glist_get_type          (void) G_GNUC_CONST;

LIBGLADEUI_API GType        glade_param_objects_get_type  (void) G_GNUC_CONST;
LIBGLADEUI_API GParamSpec  *glade_param_spec_objects      (const gchar   *name,
							   const gchar   *nick,
							   const gchar   *blurb,
							   GType          accepted_type,
							   GParamFlags    flags);

LIBGLADEUI_API void         glade_param_spec_objects_set_type (GladeParamSpecObjects *pspec,
							       GType                  type);
LIBGLADEUI_API GType        glade_param_spec_objects_get_type (GladeParamSpecObjects *pspec);

LIBGLADEUI_API GParamSpec **glade_list_atk_relations      (GType  owner_type, 
							   guint *n_specs);
LIBGLADEUI_API GParamSpec  *glade_standard_pixbuf_spec    (void);
LIBGLADEUI_API GParamSpec  *glade_standard_gdkcolor_spec  (void);
LIBGLADEUI_API GParamSpec  *glade_standard_objects_spec   (void);
LIBGLADEUI_API GParamSpec  *glade_standard_stock_spec     (void);
LIBGLADEUI_API GParamSpec  *glade_standard_int_spec       (void);
LIBGLADEUI_API GParamSpec  *glade_standard_uint_spec       (void);
LIBGLADEUI_API GParamSpec  *glade_standard_string_spec    (void);
LIBGLADEUI_API GParamSpec  *glade_standard_strv_spec      (void);
LIBGLADEUI_API GParamSpec  *glade_standard_float_spec     (void);
LIBGLADEUI_API GParamSpec  *glade_standard_boolean_spec   (void);

G_END_DECLS

#endif   /* __GLADE_BUILTINS_H__ */
