/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_XML_UTILS_H__
#define __GLADE_XML_UTILS_H__

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xmlmemory.h>

G_BEGIN_DECLS

typedef struct _XmlParseContext XmlParseContext;

struct _XmlParseContext {
	xmlDocPtr doc;
	xmlNsPtr  ns;
};

#define skip_text(node) if ((strcmp ( ((xmlNodePtr)node)->name, "text") == 0) ||\
			    (strcmp ( ((xmlNodePtr)node)->name, "comment") == 0)) { \
			node = ((xmlNodePtr)node)->next; continue ; };


gchar *      glade_xml_get_content (xmlNodePtr node); /* Get the content of the node */
gboolean     glade_xml_get_value_int          (xmlNodePtr node, const char *name, int *val);
gboolean     glade_xml_get_value_int_required (xmlNodePtr node, const char *name, int *val);

gchar *      glade_xml_get_value_string          (xmlNodePtr node, const char *name);
gchar *      glade_xml_get_value_string_required (xmlNodePtr node,
					    const char *name,
					    const char *xtra_info);

gboolean glade_xml_get_boolean (xmlNodePtr node, const char *name);

xmlNodePtr   glade_xml_search_child          (xmlNodePtr node, const char *name);
xmlNodePtr   glade_xml_search_child_required (xmlNodePtr tree, const gchar* name);

gboolean     glade_xml_node_verify (xmlNodePtr node, const gchar *name);

void         glade_xml_set_value (xmlNodePtr node, const char *name, const char *val);

/* Parse Context */
XmlParseContext * glade_xml_parse_context_new     (xmlDocPtr doc, xmlNsPtr name_space);
void              glade_xml_parse_context_destroy (XmlParseContext *context);

gboolean          glade_xml_parse_context_free (XmlParseContext *context);
XmlParseContext * glade_xml_parse_context_new_from_path (const gchar *full_path,
										 const gchar *nspace,
										 const gchar *root_name);


/* Hash */
GHashTable * glade_xml_utils_new_hash_from_node (xmlNodePtr tree, const gchar *hash_type);

xmlNodePtr glade_xml_utils_hash_write (XmlParseContext *context,
						   GHashTable *hash,
						   const gchar *name);

G_END_DECLS

#endif /* __GLADE_XML_UTILS_H__ */
