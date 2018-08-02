#ifndef __GLADE_DEBUG_H__
#define __GLADE_DEBUG_H__

G_BEGIN_DECLS

typedef enum {
  GLADE_DEBUG_REF_COUNTS    = (1 << 0),
  GLADE_DEBUG_WIDGET_EVENTS = (1 << 1),
  GLADE_DEBUG_COMMANDS      = (1 << 2),
  GLADE_DEBUG_PROPERTIES    = (1 << 3),
  GLADE_DEBUG_VERIFY        = (1 << 4)
} GladeDebugFlag;

#ifdef GLADE_ENABLE_DEBUG

#define GLADE_NOTE(type,action)                        \
  G_STMT_START {                                       \
    if (glade_get_debug_flags () & GLADE_DEBUG_##type) \
      { action; };                                     \
  } G_STMT_END

#else /* !GLADE_ENABLE_DEBUG */

#define GLADE_NOTE(type, action)

#endif /* GLADE_ENABLE_DEBUG */

void   glade_init_debug_flags (void);
guint  glade_get_debug_flags  (void);

void   glade_setup_log_handlers (void);

G_END_DECLS

#endif /* __GLADE_DEBUG_H__ */
