/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* TODO : s/glade_xml_get_/glade_xml_node_get/g */
#ifndef __GLADE_XML_UTILS_H__
#define __GLADE_XML_UTILS_H__


#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xmlmemory.h>
#include "glade-node.h"

G_BEGIN_DECLS

#define GLADE_XML_CONTEXT(c)    ((GladeXmlContext *)c)
#define GLADE_XML_IS_CONTEXT(c) (c != NULL)

typedef struct _GladeXmlContext GladeXmlContext;
typedef struct _GladeXmlNode    GladeXmlNode;
typedef struct _GladeXmlDoc     GladeXmlDoc;

struct _GladeXmlNode
{
	xmlNode node;
};
struct _GladeXmlDoc
{
	xmlDoc doc;
};

struct _GladeXmlContext {
	GladeXmlDoc *doc;
	xmlNsPtr  ns;
};

/* This is used inside for loops so that we skip xml comments <!-- i am a comment ->
 * also to skip whitespace bettween nodes
 */
#define skip_text(node) if ((strcmp ( ((xmlNodePtr)node)->name, "text") == 0) ||\
			    (strcmp ( ((xmlNodePtr)node)->name, "comment") == 0)) { \
			         node = (GladeXmlNode *)((xmlNodePtr)node)->next; continue ; };
#define skip_text_libxml(node) if ((strcmp ( ((xmlNodePtr)node)->name, "text") == 0) ||\
			           (strcmp ( ((xmlNodePtr)node)->name, "comment") == 0)) { \
                                        node = ((xmlNodePtr)node)->next; continue ; };


gchar *      glade_xml_get_content (GladeXmlNode * node); /* Get the content of the node */
void         glade_xml_set_content (GladeXmlNode *node_in, const gchar *content);

gboolean     glade_xml_get_value_int          (GladeXmlNode * node, const char *name, int *val);
gboolean     glade_xml_get_value_int_required (GladeXmlNode * node, const char *name, int *val);

gchar *      glade_xml_get_value_string          (GladeXmlNode * node, const char *name);
gchar *      glade_xml_get_value_string_required (GladeXmlNode * node,
					    const char *name,
					    const char *xtra_info);

gboolean glade_xml_get_boolean (GladeXmlNode * node, const char *name);

GladeXmlNode *   glade_xml_search_child          (GladeXmlNode * node, const char *name);
GladeXmlNode *   glade_xml_search_child_required (GladeXmlNode * tree, const gchar* name);

gboolean     glade_xml_node_verify (GladeXmlNode * node, const gchar *name);

void         glade_xml_set_value (GladeXmlNode * node, const char *name, const char *val);

/* Properties */ 
gchar * glade_xml_get_property_string_required (GladeXmlNode *node_in, const char *name, const char *xtra);
gchar * glade_xml_get_property_string (GladeXmlNode *node_in, const gchar *name);
gboolean glade_xml_property_get_boolean (GladeXmlNode *node_in, const char *name);
void glade_xml_node_set_property_string (GladeXmlNode *node_in, const gchar *name, const gchar *string);

/* Parse Context */
GladeXmlContext * glade_xml_context_new     (GladeXmlDoc *doc, xmlNsPtr name_space);
void              glade_xml_context_destroy (GladeXmlContext *context);
void              glade_xml_context_free    (GladeXmlContext *context);
GladeXmlContext * glade_xml_context_new_from_path (const gchar *full_path,
						   const gchar *nspace,
						   const gchar *root_name);
GladeXmlNode * glade_xml_context_get_root (GladeXmlContext *context);


void glade_xml_append_child (GladeXmlNode * node, GladeXmlNode * child);

/* Hash */
GHashTable * glade_xml_utils_new_hash_from_node (GladeXmlNode * tree, const gchar *hash_type);

GladeXmlNode * glade_xml_utils_hash_write (GladeXmlContext *context,
				       GHashTable *hash,
				       const gchar *name);

/* Node operations */
GladeXmlNode * glade_xml_node_new (GladeXmlContext *context, const gchar *name);
GladeXmlNode * glade_xml_node_get_children (GladeXmlNode *node);
GladeXmlNode * glade_xml_node_next (GladeXmlNode *node_in);

const gchar * glade_xml_node_get_name (GladeXmlNode *node_in);

/* Document Operatons */
GladeXmlNode * glade_xml_doc_get_root (GladeXmlDoc *doc);
GladeXmlDoc * glade_xml_doc_new (void);
void glade_xml_doc_set_root (GladeXmlDoc *doc, GladeXmlNode *node);
gint glade_xml_doc_save (GladeXmlDoc *doc_in, const gchar *full_path);
void glade_xml_doc_free (GladeXmlDoc *doc_in);

G_END_DECLS

#endif /* __GLADE_XML_UTILS_H__ */
