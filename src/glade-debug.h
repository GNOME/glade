/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_DEBUG_H__
#define __GLADE_DEBUG_H__

#ifdef DEBUG
#define g_debug(st) g_print st
#else
#define g_debug(st) 
#endif


void glade_setup_log_handlers (void);

#endif /* __GLADE-DEBUG_H__ */
