/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_BUILTINS_H__
#define __GLADE_BUILTINS_H__

#include <glib-object.h>
#include "glade.h"

G_BEGIN_DECLS

#define GLADE_TYPE_STOCK  (glade_standard_stock_get_type())

LIBGLADEUI_API GType       glade_standard_stock_get_type (void);

#define GLADE_IS_PARAM_SPEC_STOCK(pspec)   (G_TYPE_CHECK_INSTANCE_TYPE ((pspec), GLADE_TYPE_STOCK))
LIBGLADEUI_API GParamSpec *glade_standard_stock_spec     (void);
LIBGLADEUI_API GParamSpec *glade_standard_int_spec       (void);
LIBGLADEUI_API GParamSpec *glade_standard_string_spec    (void);
LIBGLADEUI_API GParamSpec *glade_standard_strv_spec      (void);
LIBGLADEUI_API GParamSpec *glade_standard_float_spec     (void);
LIBGLADEUI_API GParamSpec *glade_standard_boolean_spec   (void);

G_END_DECLS

#endif   /* __GLADE_BUILTINS_H__ */
