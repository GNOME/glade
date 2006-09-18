/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* TODO : s/glade_xml_get_/glade_xml_node_get/g */
#ifndef __GLADE_XML_UTILS_H__
#define __GLADE_XML_UTILS_H__

#include "glade-parser.h"

G_BEGIN_DECLS

#define GLADE_XML_CONTEXT(c)    ((GladeXmlContext *)c)
#define GLADE_XML_IS_CONTEXT(c) (c != NULL)

typedef struct _GladeXmlContext GladeXmlContext;
typedef struct _GladeXmlNode    GladeXmlNode;
typedef struct _GladeXmlDoc     GladeXmlDoc;


/* search child */
GladeXmlNode *   glade_xml_search_child          (GladeXmlNode * node, const gchar *name);
GladeXmlNode *   glade_xml_search_child_required (GladeXmlNode * tree, const gchar* name);

/* content */

gchar *      glade_xml_get_content (GladeXmlNode * node_in); /* Get the content of the node */
void         glade_xml_set_content (GladeXmlNode *node_in, const gchar *content);

gboolean     glade_xml_get_value_int          (GladeXmlNode * node_in, const gchar *name, int *val);
gboolean     glade_xml_get_value_int_required (GladeXmlNode * node, const gchar *name, int *val);

gchar *      glade_xml_get_value_string          (GladeXmlNode * node, const gchar *name);
gchar *      glade_xml_get_value_string_required (GladeXmlNode * node,
					    const gchar *name,
					    const gchar *xtra_info);

gboolean glade_xml_get_boolean (GladeXmlNode * node, const gchar *name, gboolean _default);

void         glade_xml_set_value (GladeXmlNode * node_in, const gchar *name, const gchar *val);

/* Properties */ 
gchar *  glade_xml_get_property_string_required (GladeXmlNode *node_in, const gchar *name, const gchar *xtra);
gchar *  glade_xml_get_property_string (GladeXmlNode *node_in, const gchar *name);
gboolean glade_xml_get_property_boolean (GladeXmlNode *node_in, const gchar *name, gboolean _default);
gdouble  glade_xml_get_property_double (GladeXmlNode *node_in, const gchar *name, gdouble _default);

void glade_xml_node_set_property_string (GladeXmlNode *node_in, const gchar *name, const gchar *string);
void glade_xml_node_set_property_boolean (GladeXmlNode *node_in, const gchar *name, gboolean value);

/* Node operations */
GladeXmlNode * glade_xml_node_new (GladeXmlContext *context, const gchar *name);
void           glade_xml_node_delete (GladeXmlNode *node);
GladeXmlNode * glade_xml_node_get_children (GladeXmlNode *node);
GladeXmlNode * glade_xml_node_next (GladeXmlNode *node_in);
gboolean       glade_xml_node_verify (GladeXmlNode * node_in, const gchar *name);
gboolean       glade_xml_node_verify_silent (GladeXmlNode *node_in, const gchar *name);
const gchar *  glade_xml_node_get_name (GladeXmlNode *node_in);
void           glade_xml_node_append_child (GladeXmlNode * node, GladeXmlNode * child);

/* Document Operatons */
GladeXmlNode * glade_xml_doc_get_root (GladeXmlDoc *doc);
GladeXmlDoc *  glade_xml_doc_new (void);
void           glade_xml_doc_set_root (GladeXmlDoc *doc, GladeXmlNode *node);
void           glade_xml_doc_free (GladeXmlDoc *doc_in);
gint           glade_xml_doc_save (GladeXmlDoc *doc_in, const gchar *full_path);

/* Parse Context */
GladeXmlContext * glade_xml_context_new     (GladeXmlDoc *doc, const gchar *name_space);
void              glade_xml_context_destroy (GladeXmlContext *context);
void              glade_xml_context_free    (GladeXmlContext *context);
GladeXmlContext * glade_xml_context_new_from_path (const gchar *full_path,
						   const gchar *nspace,
						   const gchar *root_name);
GladeXmlDoc *     glade_xml_context_get_doc (GladeXmlContext *context);

gchar *		glade_xml_alloc_string   (GladeInterface *interface, const gchar *string);
gchar *		glade_xml_alloc_propname (GladeInterface *interface, const gchar *string);

void            glade_xml_load_sym_from_node (GladeXmlNode     *node_in,
					      GModule          *module,
					      gchar            *tagname,
					      gpointer         *sym_location);

G_END_DECLS

#endif /* __GLADE_XML_UTILS_H__ */
