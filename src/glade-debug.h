#ifndef __GLADE_DEBUG_H__
#define __GLADE_DEBUG_H__

#ifdef DEBUG
#define g_debug(st) g_print st
#else
#define g_debug(st) 
#endif

#endif /* __GLADE-DEBUG_H__ */
