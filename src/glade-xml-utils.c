/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* This functions are based on gnome-print/libgpa/gpa-xml.c which where in turn based on
 * gnumeric/xml-io.c
 */
/* Authors:
 *   Daniel Veillard <Daniel.Veillard@w3.org>
 *   Miguel de Icaza <miguel@gnu.org>
 *   Chema Celorio <chema@gnome.org>
 */

#include <string.h>
#include <glib.h>
#include <string.h>

#include "glade-xml-utils.h"

/*
 * Set a string value for a node either carried as an attibute or as
 * the content of a child.
 */
void
glade_xml_set_value (GladeXmlNode *node_in, const char *name, const char *val)
{
	xmlNodePtr node = (xmlNodePtr) node_in;
	char *ret;

	ret = xmlGetProp (node, name);
	if (ret != NULL){
		xmlFree (ret);
		xmlSetProp (node, name, val);
		return;
	}
}


gchar *
glade_xml_get_content (GladeXmlNode *node_in)
{
	xmlNodePtr node = (xmlNodePtr) node_in;
	gchar *ret;
	char *val;

	val = xmlNodeGetContent(node);
	ret = g_strdup (val);
	xmlFree (val);
	
	return ret;
}

void
glade_xml_set_content (GladeXmlNode *node_in, const gchar *content)
{
	xmlNodePtr node = (xmlNodePtr) node_in;

	xmlNodeSetContent(node, content);
}

/*
 * Get a value for a node either carried as an attibute or as
 * the content of a child.
 *
 * Returns a g_malloc'ed string.  Caller must free.
 * (taken from gnumeric )
 *
 */
static char *
glade_xml_get_value (xmlNodePtr node, const char *name)
{
	char *ret, *val;
	xmlNodePtr child;

	child = node->children;

	while (child != NULL) {
		if (!strcmp (child->name, name)) {
		        /*
			 * !!! Inefficient, but ...
			 */
			val = xmlNodeGetContent(child);
			if (val != NULL) {
				ret = g_strdup (val);
				xmlFree (val);
				return ret;
			}
		}
		child = child->next;
	}

	return NULL;
}

gboolean
glade_xml_node_verify (GladeXmlNode *node_in, const gchar *name)
{
	xmlNodePtr node = (xmlNodePtr) node_in;

	g_return_val_if_fail (node != NULL, FALSE);
	
	if (strcmp (node->name, name) != 0) {
		g_warning  ("Expected node was \"%s\", encountered \"%s\"",
			    name, node->name);
		return FALSE;
	}
	return TRUE;
}


/*
 * Get an integer value for a node either carried as an attibute or as
 * the content of a child.
 */
gboolean
glade_xml_get_value_int (GladeXmlNode *node_in, const char *name, int *val)
{
	xmlNodePtr node = (xmlNodePtr) node_in;
	char *ret;
	int i;
	int res;

	ret = glade_xml_get_value (node, name);
	if (ret == NULL) return 0;
	res = sscanf (ret, "%d", &i);
	g_free (ret);

	if (res == 1) {
	        *val = i;
		return TRUE;
	}
	return FALSE;
}

/**
 * glade_xml_get_value_int_required:
 * @node: 
 * @name: 
 * @val: 
 * 
 * A wrapper arround get_value_int that displays a warning if the
 * node did not contained the requested tag
 * 
 * Return Value: 
 **/
gboolean
glade_xml_get_value_int_required (GladeXmlNode *node, const char *name, int *val)
{
	gboolean ret;
	
	ret = glade_xml_get_value_int (node, name, val);

	if (ret == FALSE)
		g_warning ("The file did not contained the required value \"%s\"\n"
			   "Under the \"%s\" tag.", name, glade_xml_node_get_name (node));
			
	return ret;
}


/*
 * Get a String value for a node either carried as an attibute or as
 * the content of a child.
 */
gchar *
glade_xml_get_value_string (GladeXmlNode *node_in, const char *name)
{
	xmlNodePtr node = (xmlNodePtr) node_in;
	return glade_xml_get_value (node, name);
}

static char *
glade_xml_get_property (xmlNodePtr node, const char *name)
{
	char *ret, *val;

	val = (char *) xmlGetProp (node, name);
	if (val != NULL) {
		ret = g_strdup (val);
		xmlFree (val);
		return ret;
	}

	return NULL;
}


static void
glade_xml_set_property (xmlNodePtr node, const char *name, const char *value)
{
	if (value != NULL) {
		xmlSetProp (node, name, value);
	}
}


#define GLADE_TAG_TRUE   "True"
#define GLADE_TAG_FALSE  "False"
#define GLADE_TAG_TRUE2  "TRUE"
#define GLADE_TAG_FALSE2 "FALSE"
/*
 * Get a String value for a node either carried as an attibute or as
 * the content of a child.
 */
gboolean
glade_xml_get_boolean (GladeXmlNode *node_in, const char *name)
{
	xmlNodePtr node = (xmlNodePtr) node_in;
	gchar * value;
	gboolean ret = FALSE;

	value = glade_xml_get_value (node, name);
	if (value == NULL)
		return FALSE;
	
	if (strcmp (value, GLADE_TAG_FALSE) == 0)
		ret = FALSE;
	else if (strcmp (value, GLADE_TAG_FALSE2) == 0)
		ret = FALSE;
	else if (strcmp (value, GLADE_TAG_TRUE) == 0)
		ret = TRUE;
	else if (strcmp (value, GLADE_TAG_TRUE2) == 0)
		ret = TRUE;
	else	
		g_warning ("Boolean tag unrecognized *%s*\n", value);

	g_free (value);

	return ret;
}

/*
 * Get a String value for a node either carried as an attibute or as
 * the content of a child.
 */
gboolean
glade_xml_property_get_boolean (GladeXmlNode *node_in, const char *name)
{
	xmlNodePtr node = (xmlNodePtr) node_in;
	gchar * value;
	gboolean ret = FALSE;

	value = glade_xml_get_property (node, name);
	if (value == NULL)
		return FALSE;
	
	if (strcmp (value, GLADE_TAG_FALSE) == 0)
		ret = FALSE;
	else if (strcmp (value, GLADE_TAG_FALSE2) == 0)
		ret = FALSE;
	else if (strcmp (value, GLADE_TAG_TRUE) == 0)
		ret = TRUE;
	else if (strcmp (value, GLADE_TAG_TRUE2) == 0)
		ret = TRUE;
	else	
		g_warning ("Boolean tag unrecognized *%s*\n", value);

	g_free (value);

	return ret;
}


#undef GLADE_TAG_TRUE
#undef GLADE_TAG_FALSE
#undef GLADE_TAG_TRUE2
#undef GLADE_TAG_FALSE2

gchar *
glade_xml_get_value_string_required (GladeXmlNode *node_in, const char *name, const char *xtra)
{
	xmlNodePtr node = (xmlNodePtr) node_in;
	gchar *value = glade_xml_get_value (node, name);

	if (value == NULL) {
		if (xtra == NULL)
			g_warning ("The file did not contained the required value \"%s\"\n"
				   "Under the \"%s\" tag.", name, node->name);
		else
			g_warning ("The file did not contained the required value \"%s\"\n"
				   "Under the \"%s\" tag (%s).", name, node->name, xtra);
	}
	return value;
}

gchar *
glade_xml_get_property_string (GladeXmlNode *node_in, const gchar *name)
{
	xmlNodePtr node = (xmlNodePtr) node_in;

	return glade_xml_get_property (node, name);
}

void
glade_xml_node_set_property_string (GladeXmlNode *node_in, const gchar *name, const gchar *string)
{
	xmlNodePtr node = (xmlNodePtr) node_in;

	glade_xml_set_property (node, name, string);
}


gchar *
glade_xml_get_property_string_required (GladeXmlNode *node_in, const char *name, const char *xtra)
{
	xmlNodePtr node = (xmlNodePtr) node_in;
	gchar *value = glade_xml_get_property_string (node_in, name);

	if (value == NULL) {
		if (xtra == NULL)
			g_warning ("The file did not contained the required property \"%s\"\n"
				   "Under the \"%s\" tag.", name, node->name);
		else
			g_warning ("The file did not contained the required property \"%s\"\n"
				   "Under the \"%s\" tag (%s).", name, node->name, xtra);
	}
	return value;
}



/*
 * Search a child by name,
 */
GladeXmlNode *
glade_xml_search_child (GladeXmlNode *node_in, const char *name)
{
	xmlNodePtr node = (xmlNodePtr) node_in;
	xmlNodePtr child;

	child = node->children;
	while (child != NULL){
		if (!strcmp (child->name, name))
			return (GladeXmlNode *)child;
		child = child->next;
	}

	return NULL;
}

/**
 * glade_xml_search_child_required:
 * @tree: 
 * @name: 
 * 
 * just a small wrapper arround glade_xml_searc_hchild that displays
 * an error if the child was not found
 *
 * Return Value: 
 **/
GladeXmlNode *
glade_xml_search_child_required (GladeXmlNode *node, const gchar* name)
{
	GladeXmlNode *child;
	
	child = glade_xml_search_child (node, name);

	if (child == NULL)
		g_warning ("The file did not contained the required tag \"%s\"\n"
			   "Under the \"%s\" node.", name, glade_xml_node_get_name (node));

	return child;
}
	




static void
glade_xml_utils_new_hash_item_from_node (xmlNodePtr node,
					 gchar **key_,
					 gchar **value_,
					 const gchar *hash_type)
{
	gchar *key;
	gchar *value;

	*key_   = NULL;
	*value_ = NULL;

	key = g_strdup (node->name);
	value = xmlNodeGetContent (node);

	*key_   = key;
	*value_ = value;

	if ((key == NULL) || (value == NULL))
		g_warning ("Could not load item for hash \"%s\"\n", hash_type);
}

GHashTable *
glade_xml_utils_new_hash_from_node (GladeXmlNode *node_in, const gchar *hash_type)
{
	xmlNodePtr node = (xmlNodePtr) node_in;
	GHashTable *hash;
	gchar *key;
	gchar *value;

	g_return_val_if_fail (node != NULL, NULL);
	g_return_val_if_fail (hash_type != NULL, NULL);

	if (!glade_xml_node_verify (node_in, hash_type))
		return NULL;
		
	hash = g_hash_table_new (g_str_hash, g_str_equal);
	/* This is not an error, since the hash can be empty */
	if (node->children == NULL)
		return hash;

	node = node->children;
	while (node != NULL) {
		skip_text_libxml (node);
		glade_xml_utils_new_hash_item_from_node (node, &key, &value, hash_type);
		if ((key == NULL) || (value == NULL))
			return NULL;
		g_hash_table_insert (hash, key, value);
		node = node->next;
	}

	return hash;
}

#if 0
static void
glade_xml_utils_hash_item_write (gpointer key_in, gpointer value_in, gpointer data)
{
	GladeXmlNode *item;
	GladeXmlNode *node;
	gchar *key;
	gchar *value;

	key   = (gchar *) key_in;
	value = (gchar *) value_in;
	node  = (GladeXmlNode *) data;

	item = xmlNewChild (node, NULL, key, value);

	if (item == NULL)
		g_warning ("Could not add the key \"%s\" with value \"%s\" to the tree",
			   key, value);
}	

GladeXmlNode *
glade_xml_utils_hash_write (GladeXmlContext *context, GHashTable *hash, const gchar *name)
{
	GladeXmlNode *node;
	
	g_return_val_if_fail (context != NULL, NULL);
	g_return_val_if_fail (hash    != NULL, NULL);

	node = glade_xml_node_new (context, name)

	g_hash_table_foreach (hash,
			      glade_xml_utils_hash_item_write,
			      node);

	return node;
}
#endif


/* --------------------------- Parse Context ----------------------------*/
GladeXmlContext *
glade_xml_context_new (GladeXmlDoc *doc,
		       xmlNsPtr  ns)
{
	GladeXmlContext *context = g_new0 (GladeXmlContext, 1);
	
	context->doc = doc;
	context->ns  = ns;
	
	return context;
}

void
glade_xml_context_destroy (GladeXmlContext *context)
{
	g_return_if_fail (context != NULL);
	g_free (context);
}


GladeXmlContext *
glade_xml_context_new_from_path (const gchar *full_path,
				 const gchar *nspace,
				 const gchar *root_name)
{
	GladeXmlContext *context;
	xmlDocPtr doc;
	xmlNsPtr  name_space;

	g_return_val_if_fail (full_path != NULL, NULL);
	
	doc = xmlParseFile (full_path);
	
	if (doc == NULL) {
		g_warning ("File not found or file with errors [%s]", full_path);
		return NULL;
	}
	if (doc->children == NULL) {
		g_warning ("Invalid xml File, tree empty [%s]&", full_path);
		xmlFreeDoc (doc);
		return NULL;
	}

	name_space = xmlSearchNsByHref (doc, doc->children, nspace);
	if (name_space == NULL && nspace != NULL) {
		g_warning ("The file did not contained the expected name space\n"
			   "Expected \"%s\" [%s]",
			   nspace, full_path);
		xmlFreeDoc (doc);
		return NULL;
	}

	if ((doc->children->name == NULL) ||
	    (strcmp (doc->children->name, root_name)!=0)) {
		g_warning ("The file did not contained the expected root name\n"
			   "Expected \"%s\", actual : \"%s\" [%s]",
			   root_name, doc->children->name, full_path);
		xmlFreeDoc (doc);
		return FALSE;
	}
	
	context = glade_xml_context_new ((GladeXmlDoc *)doc, name_space);

	return context;
}

/**
 * glade_xml_context_free:
 * @context: 
 * 
 * Similar to glade_xml_context_destroy but it also frees the document set in the context
 **/
void
glade_xml_context_free (GladeXmlContext *context)
{
	g_return_if_fail (context != NULL);
	if (context->doc)
		xmlFreeDoc ((xmlDocPtr) context->doc);
	context->doc = NULL;
			
	g_free (context);
}


void
glade_xml_append_child (GladeXmlNode *node_in, GladeXmlNode *child_in)
{
	xmlNodePtr node = (xmlNodePtr) node_in;
	xmlNodePtr child = (xmlNodePtr) child_in;
	
	g_return_if_fail (node  != NULL);
	g_return_if_fail (child != NULL);
	
	xmlAddChild (node, child);
}

GladeXmlNode *
glade_xml_node_new (GladeXmlContext *context, const gchar *name)
{
	g_return_val_if_fail (context != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);

	return (GladeXmlNode *) xmlNewDocNode ((xmlDocPtr) context->doc, context->ns, name, NULL);
}
					   

GladeXmlNode *
glade_xml_context_get_root (GladeXmlContext *context)
{
	xmlNodePtr node;

	node = ((xmlDocPtr)(context->doc))->children;

	return (GladeXmlNode *)node;
}

GladeXmlNode *
glade_xml_node_get_children (GladeXmlNode *node_in)
{
	xmlNodePtr node = (xmlNodePtr) node_in;

	return (GladeXmlNode *)node->children;
}

GladeXmlNode *
glade_xml_node_next (GladeXmlNode *node_in)
{
	xmlNodePtr node = (xmlNodePtr) node_in;
	
	return (GladeXmlNode *)node->next;
}

const gchar *
glade_xml_node_get_name (GladeXmlNode *node_in)
{
	xmlNodePtr node = (xmlNodePtr) node_in;

	return node->name;
}
	

GladeXmlDoc *
glade_xml_doc_new (void)
{
	xmlDocPtr xml_doc = xmlNewDoc ("1.0");

	return (GladeXmlDoc *)xml_doc;
}
	
	
void
glade_xml_doc_set_root (GladeXmlDoc *doc_in, GladeXmlNode *node_in)
{
	xmlNodePtr node = (xmlNodePtr) node_in;
	xmlDocPtr doc = (xmlDocPtr) doc_in;

	xmlDocSetRootElement (doc, node);
}

gint
glade_xml_doc_save (GladeXmlDoc *doc_in, const gchar *full_path)
{
	xmlDocPtr doc = (xmlDocPtr) doc_in;

	return xmlSaveFile (full_path, doc);
}

void
glade_xml_doc_free (GladeXmlDoc *doc_in)
{
	xmlDocPtr doc = (xmlDocPtr) doc_in;
	
	xmlFreeDoc (doc);
}

GladeXmlNode *
glade_xml_doc_get_root (GladeXmlDoc *doc)
{
	xmlNodePtr node;

	node = ((xmlDocPtr)(doc))->children;

	return (GladeXmlNode *)node;
}

