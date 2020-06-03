#ifndef __GLADE_PROJECT_H__
#define __GLADE_PROJECT_H__

#include <gladeui/glade-widget.h>
#include <gladeui/glade-command.h>
#include <gladeui/glade-xml-utils.h>

G_BEGIN_DECLS

#define GLADE_TYPE_PROJECT            (glade_project_get_type ())
#define GLADE_PROJECT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PROJECT, GladeProject))
#define GLADE_PROJECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_PROJECT, GladeProjectClass))
#define GLADE_IS_PROJECT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PROJECT))
#define GLADE_IS_PROJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_PROJECT))
#define GLADE_PROJECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_PROJECT, GladeProjectClass))

typedef struct _GladeProjectPrivate  GladeProjectPrivate;
typedef struct _GladeProjectClass    GladeProjectClass;

/**
 * GladePointerMode:
 * @GLADE_POINTER_SELECT:      Mouse pointer used for selecting widgets
 * @GLADE_POINTER_ADD_WIDGET:  Mouse pointer used for adding widgets
 * @GLADE_POINTER_DRAG_RESIZE: Mouse pointer used for dragging and 
 *                             resizing widgets in containers
 * @GLADE_POINTER_MARGIN_EDIT: Mouse pointer used to edit widget margins
 * @GLADE_POINTER_ALIGN_EDIT:  Mouse pointer used to edit widget alignment
 *
 * Indicates what the pointer is used for in the workspace.
 */
typedef enum
{
  GLADE_POINTER_SELECT = 0,
  GLADE_POINTER_ADD_WIDGET,
  GLADE_POINTER_DRAG_RESIZE,
  GLADE_POINTER_MARGIN_EDIT,
  GLADE_POINTER_ALIGN_EDIT
} GladePointerMode;

typedef enum
{
  GLADE_SUPPORT_OK                   = 0,
  GLADE_SUPPORT_DEPRECATED           = (0x01 << 0),
  GLADE_SUPPORT_MISMATCH             = (0x01 << 1)
} GladeSupportMask;

/**
 * GladeProjectModelColumns:
 * @GLADE_PROJECT_MODEL_COLUMN_ICON_NAME: name of the icon for the widget
 * @GLADE_PROJECT_MODEL_COLUMN_NAME: Name of the widget
 * @GLADE_PROJECT_MODEL_COLUMN_TYPE_NAME: The type name of the widget
 * @GLADE_PROJECT_MODEL_COLUMN_OBJECT: the GObject of the widget
 * @GLADE_PROJECT_MODEL_COLUMN_MISC: the auxilary text describing a widget's role
 * @GLADE_PROJECT_MODEL_COLUMN_WARNING: the support warning text for this widget
 * @GLADE_PROJECT_MODEL_N_COLUMNS: Number of columns
 *
 * The tree view columns provided by the GtkTreeModel implemented
 * by GladeProject
 *
 **/
typedef enum
{
  GLADE_PROJECT_MODEL_COLUMN_ICON_NAME,
  GLADE_PROJECT_MODEL_COLUMN_NAME,
  GLADE_PROJECT_MODEL_COLUMN_TYPE_NAME,
  GLADE_PROJECT_MODEL_COLUMN_OBJECT,
  GLADE_PROJECT_MODEL_COLUMN_MISC,
  GLADE_PROJECT_MODEL_COLUMN_WARNING,
  GLADE_PROJECT_MODEL_N_COLUMNS
} GladeProjectModelColumns;

/**
 * GladeVerifyFlags:
 * @GLADE_VERIFY_NONE: No verification
 * @GLADE_VERIFY_VERSIONS: Verify version mismatches
 * @GLADE_VERIFY_DEPRECATIONS: Verify deprecations
 * @GLADE_VERIFY_UNRECOGNIZED: Verify unrecognized types
 *
 */
typedef enum {
  GLADE_VERIFY_NONE          = 0,
  GLADE_VERIFY_VERSIONS      = (1 << 0),
  GLADE_VERIFY_DEPRECATIONS  = (1 << 1),
  GLADE_VERIFY_UNRECOGNIZED  = (1 << 2)
} GladeVerifyFlags;

struct _GladeProject
{
  GObject parent_instance;

  GladeProjectPrivate *priv;
};

struct _GladeProjectClass
{
  GObjectClass parent_class;

  void          (*add_object)          (GladeProject *project,
                                        GladeWidget  *object);
  void          (*remove_object)       (GladeProject *project,
                                        GladeWidget  *object);

  void          (*undo)                (GladeProject *project);
  void          (*redo)                (GladeProject *project);
  GladeCommand *(*next_undo_item)      (GladeProject *project);
  GladeCommand *(*next_redo_item)      (GladeProject *project);
  void          (*push_undo)           (GladeProject *project,
                                        GladeCommand *cmd);

  void          (*changed)             (GladeProject *project,
                                        GladeCommand *command,
                                        gboolean      forward);

  void          (*widget_name_changed) (GladeProject *project,
                                        GladeWidget  *widget);
  void          (*selection_changed)   (GladeProject *project); 
  void          (*close)               (GladeProject *project);

  void          (*parse_finished)      (GladeProject *project);
  
  void   (* glade_reserved1)   (void);
  void   (* glade_reserved2)   (void);
  void   (* glade_reserved3)   (void);
  void   (* glade_reserved4)   (void);
  void   (* glade_reserved5)   (void);
  void   (* glade_reserved6)   (void);
  void   (* glade_reserved7)   (void);
  void   (* glade_reserved8)   (void);
};


GLADEUI_EXPORTS
GType               glade_project_get_type            (void) G_GNUC_CONST;

GLADEUI_EXPORTS
GladeProject       *glade_project_new                 (void);
GLADEUI_EXPORTS
GladeProject       *glade_project_load                (const gchar         *path);
GLADEUI_EXPORTS
gboolean            glade_project_load_from_file      (GladeProject        *project, 
                                                       const gchar         *path);
GLADEUI_EXPORTS
gboolean            glade_project_save                (GladeProject        *project,
                                                       const gchar         *path, 
                                                       GError             **error);
GLADEUI_EXPORTS
gboolean            glade_project_save_verify         (GladeProject        *project,
                                                       const gchar         *path,
                                                       GladeVerifyFlags     flags,
                                                       GError             **error);
GLADEUI_EXPORTS
gboolean            glade_project_autosave            (GladeProject        *project,
                                                       GError             **error);
GLADEUI_EXPORTS
gboolean            glade_project_backup              (GladeProject        *project,
                                                       const gchar         *path, 
                                                       GError             **error);
GLADEUI_EXPORTS
void                glade_project_push_progress        (GladeProject       *project);
GLADEUI_EXPORTS
gboolean            glade_project_load_cancelled       (GladeProject       *project);
GLADEUI_EXPORTS
void                glade_project_cancel_load          (GladeProject       *project);

GLADEUI_EXPORTS
void                glade_project_preview              (GladeProject       *project, 
                                                        GladeWidget        *gwidget);
GLADEUI_EXPORTS
void                glade_project_properties           (GladeProject       *project);
GLADEUI_EXPORTS
gchar              *glade_project_resource_fullpath    (GladeProject       *project,
                                                        const gchar        *resource);
GLADEUI_EXPORTS
void                glade_project_set_resource_path    (GladeProject       *project,
                                                        const gchar        *path);
GLADEUI_EXPORTS
const gchar        *glade_project_get_resource_path    (GladeProject       *project);

GLADEUI_EXPORTS
void                glade_project_widget_visibility_changed (GladeProject  *project,
                                                             GladeWidget   *widget,
                                                             gboolean       visible);
GLADEUI_EXPORTS
void                glade_project_check_reordered      (GladeProject       *project,
                                                        GladeWidget        *parent,
                                                        GList              *old_order);

GLADEUI_EXPORTS
void                glade_project_set_template         (GladeProject       *project,
                                                        GladeWidget        *widget);
GLADEUI_EXPORTS
GladeWidget        *glade_project_get_template         (GladeProject       *project);

GLADEUI_EXPORTS
void                glade_project_set_license          (GladeProject       *project,
                                                        const gchar        *license);

GLADEUI_EXPORTS
const gchar        *glade_project_get_license          (GladeProject       *project);

/* Commands */
GLADEUI_EXPORTS
void                glade_project_undo                 (GladeProject       *project);
GLADEUI_EXPORTS
void                glade_project_redo                 (GladeProject       *project);
GLADEUI_EXPORTS
GladeCommand       *glade_project_next_undo_item       (GladeProject       *project);
GLADEUI_EXPORTS
GladeCommand       *glade_project_next_redo_item       (GladeProject       *project);
GLADEUI_EXPORTS
void                glade_project_push_undo            (GladeProject       *project,
                                                        GladeCommand       *cmd);
GLADEUI_EXPORTS
GtkWidget          *glade_project_undo_items           (GladeProject       *project);
GLADEUI_EXPORTS
GtkWidget          *glade_project_redo_items           (GladeProject       *project);

/* Add/Remove Objects */
GLADEUI_EXPORTS
const GList        *glade_project_get_objects          (GladeProject       *project);
GLADEUI_EXPORTS
void                glade_project_add_object           (GladeProject       *project,
                                                        GObject            *object);
GLADEUI_EXPORTS
void                glade_project_remove_object        (GladeProject       *project,
                                                        GObject            *object);
GLADEUI_EXPORTS
gboolean            glade_project_has_object           (GladeProject       *project,
                                                        GObject            *object);
GLADEUI_EXPORTS
void                glade_project_widget_changed       (GladeProject       *project,
                                                        GladeWidget        *gwidget);

/* Widget names */
GLADEUI_EXPORTS
GladeWidget        *glade_project_get_widget_by_name   (GladeProject       *project,
                                                        const gchar        *name);
GLADEUI_EXPORTS
void                glade_project_set_widget_name      (GladeProject       *project,
                                                        GladeWidget        *widget, 
                                                        const gchar        *name);
GLADEUI_EXPORTS
gchar              *glade_project_new_widget_name      (GladeProject       *project,
                                                        GladeWidget        *widget,
                                                        const gchar        *base_name);
GLADEUI_EXPORTS
gboolean            glade_project_available_widget_name(GladeProject       *project,
                                                        GladeWidget        *widget,
                                                        const gchar        *name);

/* Selection */
GLADEUI_EXPORTS
gboolean            glade_project_is_selected          (GladeProject       *project,
                                                        GObject            *object);
GLADEUI_EXPORTS
void                glade_project_selection_set        (GladeProject       *project,
                                                        GObject            *object,
                                                        gboolean            emit_signal);
GLADEUI_EXPORTS
void                glade_project_selection_add        (GladeProject       *project,
                                                        GObject            *object,
                                                        gboolean            emit_signal);
GLADEUI_EXPORTS
void                glade_project_selection_remove     (GladeProject       *project,
                                                        GObject            *object,
                                                        gboolean            emit_signal);
GLADEUI_EXPORTS
void                glade_project_selection_clear      (GladeProject       *project,
                                                        gboolean            emit_signal);
GLADEUI_EXPORTS
void                glade_project_selection_changed    (GladeProject       *project);
GLADEUI_EXPORTS
void                glade_project_queue_selection_changed (GladeProject    *project);
GLADEUI_EXPORTS
GList              *glade_project_selection_get        (GladeProject       *project);
GLADEUI_EXPORTS
gboolean            glade_project_get_has_selection    (GladeProject       *project);

/* Accessors */ 
GLADEUI_EXPORTS
const gchar        *glade_project_get_path             (GladeProject       *project);
GLADEUI_EXPORTS
gchar              *glade_project_get_name             (GladeProject       *project);
GLADEUI_EXPORTS
void                glade_project_reset_path           (GladeProject       *project);
GLADEUI_EXPORTS
gboolean            glade_project_is_loading           (GladeProject       *project);
GLADEUI_EXPORTS
time_t              glade_project_get_file_mtime       (GladeProject       *project);
GLADEUI_EXPORTS
gboolean            glade_project_get_readonly         (GladeProject       *project);
GLADEUI_EXPORTS
gboolean            glade_project_get_modified         (GladeProject       *project);
GLADEUI_EXPORTS
void                glade_project_set_pointer_mode     (GladeProject       *project,
                                                        GladePointerMode    mode);
GLADEUI_EXPORTS
GladePointerMode    glade_project_get_pointer_mode     (GladeProject       *project);
GLADEUI_EXPORTS
void                glade_project_set_add_item         (GladeProject       *project,
                                                        GladeWidgetAdaptor *adaptor);
GLADEUI_EXPORTS
GladeWidgetAdaptor *glade_project_get_add_item         (GladeProject       *project);
GLADEUI_EXPORTS
void                glade_project_set_target_version   (GladeProject       *project,
                                                        const gchar        *catalog,
                                                        gint                major,
                                                        gint                minor);
GLADEUI_EXPORTS
void                glade_project_get_target_version   (GladeProject       *project,
                                                        const gchar        *catalog,
                                                        gint               *major,
                                                        gint               *minor);
GLADEUI_EXPORTS
GList              *glade_project_required_libs        (GladeProject       *project);
GLADEUI_EXPORTS
gchar              *glade_project_display_dependencies (GladeProject       *project);

GLADEUI_EXPORTS
GList              *glade_project_toplevels            (GladeProject       *project);

GLADEUI_EXPORTS
void                glade_project_set_translation_domain (GladeProject *project,
                                                          const gchar *domain);
GLADEUI_EXPORTS
const gchar        *glade_project_get_translation_domain (GladeProject *project);

GLADEUI_EXPORTS
void                glade_project_set_css_provider_path  (GladeProject *project,
                                                          const gchar  *path);

GLADEUI_EXPORTS
const gchar        *glade_project_get_css_provider_path  (GladeProject *project);

/* Verifications */
GLADEUI_EXPORTS
gboolean            glade_project_verify               (GladeProject       *project,
                                                        gboolean            saving,
                                                        GladeVerifyFlags    flags);
GLADEUI_EXPORTS
gchar              *glade_project_verify_widget_adaptor(GladeProject       *project,
                                                        GladeWidgetAdaptor *adaptor,
                                                        GladeSupportMask   *mask);
GLADEUI_EXPORTS
void                glade_project_verify_property      (GladeProperty      *property);
GLADEUI_EXPORTS
void                glade_project_verify_signal        (GladeWidget        *widget,
                                                        GladeSignal        *signal);
GLADEUI_EXPORTS
gboolean            glade_project_writing_preview      (GladeProject       *project);

/* General selection driven commands */
GLADEUI_EXPORTS
void                glade_project_copy_selection       (GladeProject       *project);
GLADEUI_EXPORTS
void                glade_project_command_cut          (GladeProject       *project); 
GLADEUI_EXPORTS
void                glade_project_command_paste        (GladeProject       *project,
                                                        GladePlaceholder   *placeholder);
GLADEUI_EXPORTS
void                glade_project_command_delete       (GladeProject       *project);

G_END_DECLS

#endif /* __GLADE_PROJECT_H__ */
