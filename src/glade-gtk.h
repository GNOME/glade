/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_GTK_H__
#define __GLADE_GTK_H__

#include "glade.h"

#ifdef G_OS_WIN32
#define GLADEGTK_API __declspec(dllexport)
#else
#define GLADEGTK_API
#endif

/* Types */

typedef enum {
	GLADEGTK_IMAGE_FILENAME = 0,
	GLADEGTK_IMAGE_STOCK,
	GLADEGTK_IMAGE_ICONTHEME
} GladeGtkImageType;

typedef enum {
	GLADEGTK_BUTTON_LABEL = 0,
	GLADEGTK_BUTTON_STOCK,
	GLADEGTK_BUTTON_CONTAINER
} GladeGtkButtonType;

GType GLADEGTK_API		glade_gtk_image_type_get_type (void);
GType GLADEGTK_API		glade_gtk_button_type_get_type (void);

#endif /* __GLADE_GTK_H__ */
