/*
 * glade-xml-utils.c - This functions are based on gnome-print/libgpa/gpa-xml.c
 * which were in turn based on gnumeric/xml-io.c
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
   *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Daniel Veillard <Daniel.Veillard@w3.org>
 *   Miguel de Icaza <miguel@gnu.org>
 *   Chema Celorio <chema@gnome.org>
 */

/**
 * SECTION:glade-xml-utils
 * @Title: Xml Utils
 * @Short_Description: An api to read and write xml.
 *
 * You may need these tools if you are implementing #GladeReadWidgetFunc
 * and/or #GladeWriteWidgetFunc on your #GladeWidgetAdaptor to read
 * and write widgets in custom ways
 */


#include "config.h"

#include <string.h>
#include <glib.h>
#include <errno.h>

#include "glade-xml-utils.h"
#include "glade-catalog.h"
#include "glade-utils.h"

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/xmlmemory.h>

struct _GladeXmlNode
{
  xmlNodePtr node;
};

struct _GladeXmlDoc
{
  xmlDocPtr doc;
  gint reference_count;
};

struct _GladeXmlContext
{
  GladeXmlDoc *doc;
  xmlNsPtr ns;
};

G_DEFINE_BOXED_TYPE(GladeXmlNode, glade_xml_node, glade_xml_node_copy, glade_xml_node_delete);
G_DEFINE_BOXED_TYPE(GladeXmlDoc, glade_xml_doc, glade_xml_doc_ref, glade_xml_doc_unref);
G_DEFINE_BOXED_TYPE(GladeXmlContext, glade_xml_context, glade_xml_context_copy, glade_xml_context_free);

static GladeXmlDoc *glade_xml_doc_new_from_doc (xmlDocPtr docptr);

/* This is used inside for loops so that we skip xml comments 
 * <!-- i am a comment ->
 * also to skip whitespace between nodes
 */
#define skip_text(node) if ((xmlStrcmp ( ((xmlNodePtr)node)->name, BAD_CAST("text")) == 0) ||\
                            (xmlStrcmp ( ((xmlNodePtr)node)->name, BAD_CAST("comment")) == 0)) { \
                                 node = (GladeXmlNode *)((xmlNodePtr)node)->next; continue ; };
#define skip_text_libxml(node) if ((xmlStrcmp ( ((xmlNodePtr)node)->name, BAD_CAST("text")) == 0) ||\
                                   (xmlStrcmp ( ((xmlNodePtr)node)->name, BAD_CAST("comment")) == 0)) { \
                                       node = ((xmlNodePtr)node)->next; continue ; };


static gchar *
claim_string (xmlChar *string)
{
  gchar *ret;
  ret = g_strdup (CAST_BAD (string));
  xmlFree (string);
  return ret;
}

/**
 * glade_xml_set_value:
 * @node_in: a #GladeXmlNode
 * @name: a string
 * @val: a string
 *
 * Sets the property @name in @node_in to @val
 */
void
glade_xml_set_value (GladeXmlNode *node_in, const gchar *name, const gchar *val)
{
  xmlNodePtr node = (xmlNodePtr) node_in;
  xmlChar *ret;

  ret = xmlGetProp (node, BAD_CAST (name));
  if (ret)
    {
      xmlFree (ret);
      xmlSetProp (node, BAD_CAST (name), BAD_CAST (val));
      return;
    }
}

/**
 * glade_xml_get_content:
 * @node_in: a #GladeXmlNode
 *
 * Gets a string containing the content of @node_in.
 *
 * Returns: A newly allocated string
 */
gchar *
glade_xml_get_content (GladeXmlNode *node_in)
{
  xmlNodePtr node = (xmlNodePtr) node_in;
  xmlChar *val = xmlNodeGetContent (node);

  return claim_string (val);
}

/**
 * glade_xml_set_content:
 * @node_in: a #GladeXmlNode
 * @content: a string
 *
 * Sets the content of @node to @content.
 */
void
glade_xml_set_content (GladeXmlNode *node_in, const gchar *content)
{
  xmlNodePtr node = (xmlNodePtr) node_in;
  xmlChar *content_encoded;

  g_return_if_fail (node != NULL);
  g_return_if_fail (node->doc != NULL);

  content_encoded = xmlEncodeSpecialChars (node->doc, BAD_CAST (content));
  xmlNodeSetContent (node, BAD_CAST (content_encoded));
  xmlFree (content_encoded);
}

/*
 * Get a value for a node either carried as an attibute or as
 * the content of a child.
 *
 * Returns a g_malloc'ed string.  Caller must free.
 * (taken from gnumeric )
 *
 */
static gchar *
glade_xml_get_value (xmlNodePtr node, const gchar *name)
{
  xmlNodePtr child;

  for (child = node->children; child; child = child->next)
    if (!xmlStrcmp (child->name, BAD_CAST (name)))
      return claim_string (xmlNodeGetContent (child));

  return NULL;
}

/**
 * glade_xml_node_verify_silent:
 * @node_in: a #GladeXmlNode
 * @name: a string
 *
 * Returns: %TRUE if @node_in's name is equal to @name, %FALSE otherwise
 */
gboolean
glade_xml_node_verify_silent (GladeXmlNode *node_in, const gchar *name)
{
  xmlNodePtr node = (xmlNodePtr) node_in;

  g_return_val_if_fail (node != NULL, FALSE);

  if (xmlStrcmp (node->name, BAD_CAST (name)) != 0)
    return FALSE;
  return TRUE;
}

/**
 * glade_xml_node_verify:
 * @node_in: a #GladeXmlNode
 * @name: a string
 *
 * This is a wrapper around glade_xml_node_verify_silent(), only it emits
 * a g_warning() if @node_in has a name different than @name.
 *
 * Returns: %TRUE if @node_in's name is equal to @name, %FALSE otherwise
 */
gboolean
glade_xml_node_verify (GladeXmlNode *node_in, const gchar *name)
{
  xmlNodePtr node = (xmlNodePtr) node_in;

  if (!glade_xml_node_verify_silent (node_in, name))
    {
      g_warning ("Expected node was \"%s\", encountered \"%s\"",
                 name, node->name);
      return FALSE;
    }

  return TRUE;
}

/**
 * glade_xml_get_value_int:
 * @node_in: a #GladeXmlNode
 * @name: a string
 * @val: a pointer to an #int
 *
 * Gets an integer value for a node either carried as an attribute or as
 * the content of a child.
 *
 * Returns: %TRUE if the node is found, %FALSE otherwise
 */
gboolean
glade_xml_get_value_int (GladeXmlNode *node_in, const gchar *name, gint *val)
{
  xmlNodePtr node = (xmlNodePtr) node_in;
  gchar *value, *endptr = NULL;
  gint64 i;

  value = glade_xml_get_value (node, name);
  if (value == NULL)
    return FALSE;

  errno = 0;
  i = g_ascii_strtoll (value, &endptr, 10);
  if (errno != 0 || (i == 0 && endptr == value))
    {
      g_free (value);
      return FALSE;
    }

  g_free (value);
  *val = (gint) i;
  return TRUE;
}

/**
 * glade_xml_get_value_int_required:
 * @node: a #GladeXmlNode
 * @name: a string
 * @val: a pointer to an #int
 * 
 * This is a wrapper around glade_xml_get_value_int(), only it emits
 * a g_warning() if @node did not contain the requested tag
 * 
 * Returns: %FALSE if @name is not in @node
 **/
gboolean
glade_xml_get_value_int_required (GladeXmlNode *node,
                                  const gchar  *name,
                                  gint         *val)
{
  gboolean ret;

  ret = glade_xml_get_value_int (node, name, val);

  if (ret == FALSE)
    g_warning ("The file did not contain the required value \"%s\"\n"
               "Under the \"%s\" tag.", name, glade_xml_node_get_name (node));

  return ret;
}

/*
 * Get a String value for a node either carried as an attibute or as
 * the content of a child.
 */
gchar *
glade_xml_get_value_string (GladeXmlNode *node_in, const gchar *name)
{
  xmlNodePtr node = (xmlNodePtr) node_in;
  return glade_xml_get_value (node, name);
}

static gchar *
glade_xml_get_property (xmlNodePtr node, const gchar *name)
{
  xmlChar *val;

  val = xmlGetProp (node, BAD_CAST (name));

  if (val)
    return claim_string (val);

  return NULL;
}

static void
glade_xml_set_property (xmlNodePtr   node,
                        const gchar *name,
                        const gchar *value)
{
  if (value)
    xmlSetProp (node, BAD_CAST (name), BAD_CAST (value));
}

/*
 * Get a String value for a node either carried as an attibute or as
 * the content of a child.
 */
gboolean
glade_xml_get_boolean (GladeXmlNode *node_in,
                       const gchar  *name,
                       gboolean      _default)
{
  xmlNodePtr node = (xmlNodePtr) node_in;
  gchar *value;
  gboolean ret = FALSE;

  value = glade_xml_get_value (node, name);
  if (value == NULL)
    return _default;

  if (glade_utils_boolean_from_string (value, &ret))
    g_warning ("Boolean tag unrecognized *%s*\n", value);
  g_free (value);

  return ret;
}

/*
 * Get a String value for a node either carried as an attibute or as
 * the content of a child.
 */
gboolean
glade_xml_get_property_boolean (GladeXmlNode *node_in,
                                const gchar  *name,
                                gboolean      _default)
{
  xmlNodePtr node = (xmlNodePtr) node_in;
  gchar *value;
  gboolean ret = FALSE;

  value = glade_xml_get_property (node, name);
  if (value == NULL)
    return _default;

  if (glade_utils_boolean_from_string (value, &ret))
    g_warning ("Boolean tag unrecognized *%s*\n", value);
  g_free (value);

  return ret;
}

gdouble
glade_xml_get_property_double (GladeXmlNode *node_in,
                               const gchar  *name,
                               gdouble       _default)
{
  xmlNodePtr node = (xmlNodePtr) node_in;
  gdouble retval;
  gchar *value;

  if ((value = glade_xml_get_property (node, name)) == NULL)
    return _default;

  errno = 0;

  retval = g_ascii_strtod (value, NULL);

  if (errno)
    {
      g_free (value);
      return _default;
    }
  else
    {
      g_free (value);
      return retval;
    }
}

gint
glade_xml_get_property_int (GladeXmlNode *node_in,
                            const gchar  *name,
                            gint          _default)
{
  xmlNodePtr node = (xmlNodePtr) node_in;
  gint retval;
  gchar *value;

  if ((value = glade_xml_get_property (node, name)) == NULL)
    return _default;

  retval = g_ascii_strtoll (value, NULL, 10);

  g_free (value);

  return retval;
}

void
glade_xml_node_set_property_boolean (GladeXmlNode *node_in,
                                     const gchar  *name,
                                     gboolean      value)
{
  xmlNodePtr node = (xmlNodePtr) node_in;

  if (value)
    glade_xml_set_property (node, name, "True");
  else
    glade_xml_set_property (node, name, "False");
}

gchar *
glade_xml_get_value_string_required (GladeXmlNode *node_in,
                                     const gchar  *name,
                                     const gchar  *xtra)
{
  xmlNodePtr node = (xmlNodePtr) node_in;
  gchar *value = glade_xml_get_value (node, name);

  if (value == NULL)
    {
      if (xtra == NULL)
        g_warning ("The file did not contain the required value \"%s\"\n"
                   "Under the \"%s\" tag.", name, node->name);
      else
        g_warning ("The file did not contain the required value \"%s\"\n"
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

/**
 * glade_xml_node_set_property_string:
 * @node_in: a #GladeXmlNode
 * @name: the name of the property to set
 * @string: (nullable): the string value of the property to set
 *
 * Set a property as a string in the @node_in. Note that %NULL @string value
 * Are simply ignored and not written in the XML.
 */
void
glade_xml_node_set_property_string (GladeXmlNode *node_in,
                                    const gchar  *name,
                                    const gchar  *string)
{
  xmlNodePtr node = (xmlNodePtr) node_in;

  glade_xml_set_property (node, name, string);
}

gchar *
glade_xml_get_property_string_required (GladeXmlNode *node_in,
                                        const gchar  *name,
                                        const gchar  *xtra)
{
  xmlNodePtr node = (xmlNodePtr) node_in;
  gchar *value = glade_xml_get_property_string (node_in, name);

  if (value == NULL)
    {
      if (xtra == NULL)
        g_warning ("The file did not contain the required property \"%s\"\n"
                   "Under the \"%s\" tag.", name, node->name);
      else
        g_warning ("The file did not contain the required property \"%s\"\n"
                   "Under the \"%s\" tag (%s).", name, node->name, xtra);
    }
  return value;
}

gboolean
glade_xml_get_property_version (GladeXmlNode *node_in,
                                const gchar  *name,
                                guint16      *major,
                                guint16      * minor)
{
  xmlNodePtr node = (xmlNodePtr) node_in;
  gchar *value = glade_xml_get_property_string (node_in, name);
  gchar **split;

  if (!value)
    return FALSE;

  if ((split = g_strsplit (value, ".", 2)))
    {
      if (!split[0] || !split[1])
        {
          g_warning ("Malformed version property \"%s\"\n"
                     "Under the \"%s\" tag (%s)", name, node->name, value);
          return FALSE;
        }

      *major = g_ascii_strtoll (split[0], NULL, 10);
      *minor = g_ascii_strtoll (split[1], NULL, 10);

      g_strfreev (split);
    }

  g_free (value);

  return TRUE;
}

/**
 * glade_xml_get_property_targetable_versions:
 * @node_in: a #GladeXmlNode
 * @name: a property name
 *
 * Get the list of targetable versions for a property
 *
 * Returns: (element-type GladeTargetableVersion) (transfer full): a list of #GladeTargetableVersion
 */
GList *
glade_xml_get_property_targetable_versions (GladeXmlNode *node_in,
                                            const gchar  *name)
{
  GladeTargetableVersion *version;
  GList *targetable = NULL;
  xmlNodePtr node = (xmlNodePtr) node_in;
  gchar *value;
  gchar **split, **maj_min;
  gint i;

  if (!(value = glade_xml_get_property_string (node_in, name)))
    return NULL;

  if ((split = g_strsplit (value, ",", 0)) != NULL)
    {
      for (i = 0; split[i]; i++)
        {
          maj_min = g_strsplit (split[i], ".", 2);

          if (!maj_min[0] || !maj_min[1])
            {
              g_warning ("Malformed version property \"%s\"\n"
                         "Under the \"%s\" tag (%s)", name, node->name, value);
            }
          else
            {
              version = g_new (GladeTargetableVersion, 1);
              version->major = g_ascii_strtoll (maj_min[0], NULL, 10);
              version->minor = g_ascii_strtoll (maj_min[1], NULL, 10);

              targetable = g_list_append (targetable, version);
            }
          g_strfreev (maj_min);
        }

      g_strfreev (split);
    }

  g_free (value);

  return targetable;
}



/*
 * Search a child by name,
 */
GladeXmlNode *
glade_xml_search_child (GladeXmlNode *node_in, const gchar *name)
{
  xmlNodePtr node;
  xmlNodePtr child;

  g_return_val_if_fail (node_in != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  node = (xmlNodePtr) node_in;

  for (child = node->children; child; child = child->next)
    {
      if (!xmlStrcmp (child->name, BAD_CAST (name)))
        return (GladeXmlNode *) child;
    }

  return NULL;
}

/**
 * glade_xml_search_child_required:
 * @tree: A #GladeXmlNode
 * @name: the name of the child
 * 
 * just a small wrapper arround glade_xml_search_child that displays
 * an error if the child was not found
 *
 * Returns: (nullable): the requested #GladeXmlNode
 **/
GladeXmlNode *
glade_xml_search_child_required (GladeXmlNode *node, const gchar *name)
{
  GladeXmlNode *child;

  child = glade_xml_search_child (node, name);

  if (child == NULL)
    g_warning ("The file did not contain the required tag \"%s\"\n"
               "Under the \"%s\" node.", name, glade_xml_node_get_name (node));

  return child;
}

/* --------------------------- Parse Context ----------------------------*/

/*
 * glade_xml_context_new_from_xml_namespace:
 * @doc: (transfer full): a #GladeXmlDoc
 * @ns: (nullable): a #xmlNs
 *
 * Returns: (transfer full): a new #GladeXmlContext
 */
static GladeXmlContext *
glade_xml_context_new_from_xml_namespace (GladeXmlDoc *doc, xmlNsPtr ns)
{
  GladeXmlContext *context = g_new0 (GladeXmlContext, 1);

  context->doc = doc;
  context->ns = ns;

  return context;
}

/**
 * glade_xml_context_new:
 * @doc: (transfer full): a #GladeXmlDoc
 * @name_space: (nullable): unused argument
 *
 * Returns: (transfer full): a new #GladeXmlContext
 */
GladeXmlContext *
glade_xml_context_new (GladeXmlDoc *doc, const gchar *name_space)
{
  /* We are not using the namespace now */
  return glade_xml_context_new_from_xml_namespace (doc, NULL);
}

/**
 * glade_xml_context_copy:
 * @context: a #GladeXmlDoc
 *
 * Returns: (transfer full): a copy of the given #GladeXmlContext
 */
GladeXmlContext *
glade_xml_context_copy (GladeXmlContext *context)
{
  return glade_xml_context_new_from_xml_namespace (glade_xml_doc_ref (context->doc), context->ns);
}

/**
 * glade_xml_context_new_from_path:
 * @full_path: the path to the XML file
 * @nspace: (nullable): the expected namespace
 * @root_name: (nullable): the expected root name
 *
 * Creates a new #GladeXmlContext from the given path.
 *
 * Returns: (transfer full) (nullable): a new #GladeXmlContext or %NULL on failure
 */
GladeXmlContext *
glade_xml_context_new_from_path (const gchar *full_path,
                                 const gchar *nspace,
                                 const gchar *root_name)
{
  GladeXmlContext *context;
  GladeXmlDoc *glade_doc;
  xmlDocPtr doc;
  xmlNsPtr name_space;
  xmlNodePtr root;

  g_return_val_if_fail (full_path != NULL, NULL);

  doc = xmlParseFile (full_path);

  /* That's not an error condition.  The file is not readable, and we can't know it
   * before we try to read it (testing for readability is a call to race conditions).
   * So we should not print a warning */
  if (doc == NULL)
    return NULL;

  if (doc->children == NULL)
    {
      g_warning ("Invalid xml File, tree empty [%s]&", full_path);
      xmlFreeDoc (doc);
      return NULL;
    }

  name_space = xmlSearchNsByHref (doc, doc->children, BAD_CAST (nspace));
  if (name_space == NULL && nspace != NULL)
    {
      g_warning ("The file did not contain the expected name space\n"
                 "Expected \"%s\" [%s]", nspace, full_path);
      xmlFreeDoc (doc);
      return NULL;
    }

  root = xmlDocGetRootElement (doc);
  if (root_name != NULL &&
      ((root->name == NULL) ||
       (xmlStrcmp (root->name, BAD_CAST (root_name)) != 0)))
    {
      g_warning ("The file did not contain the expected root name\n"
                 "Expected \"%s\", actual : \"%s\" [%s]",
                 root_name, root->name, full_path);
      xmlFreeDoc (doc);
      return NULL;
    }

  glade_doc = glade_xml_doc_new_from_doc (doc);
  context = glade_xml_context_new_from_xml_namespace (glade_doc, name_space);

  return context;
}

/**
 * glade_xml_context_free:
 * @context: An #GladeXmlContext
 * 
 * Frees the memory allocated by #GladeXmlContext.
 **/
void
glade_xml_context_free (GladeXmlContext *context)
{
  if (!context)
    return;

  g_clear_pointer (&context->doc, glade_xml_doc_unref);
  g_free (context);
}

void
glade_xml_node_append_child (GladeXmlNode *node_in,
                             GladeXmlNode *child_in)
{
  xmlNodePtr node = (xmlNodePtr) node_in;
  xmlNodePtr child = (xmlNodePtr) child_in;

  g_return_if_fail (node != NULL);
  g_return_if_fail (child != NULL);

  xmlAddChild (node, child);
}

void
glade_xml_node_remove (GladeXmlNode * node_in)
{
  xmlNodePtr node = (xmlNodePtr) node_in;

  g_return_if_fail (node != NULL);

  xmlReplaceNode (node, NULL);
}


GladeXmlNode *
glade_xml_node_new (GladeXmlContext *context, const gchar *name)
{
  g_return_val_if_fail (context != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return (GladeXmlNode *) xmlNewDocNode (context->doc->doc, context->ns,
                                         BAD_CAST (name), NULL);
}

GladeXmlNode *
glade_xml_node_new_comment (GladeXmlContext *context, const gchar *comment)
{
  g_return_val_if_fail (context != NULL, NULL);
  g_return_val_if_fail (comment != NULL, NULL);

  return (GladeXmlNode *) xmlNewDocComment (context->doc->doc,
                                            BAD_CAST (comment));
}

GladeXmlNode *
glade_xml_node_copy (GladeXmlNode *node)
{
  if (node)
    {
      xmlNodePtr xnode = (xmlNodePtr) node;
      return (GladeXmlNode *) xmlDocCopyNode (xnode, NULL, 1);
    }
  else
    return NULL;
}

void
glade_xml_node_delete (GladeXmlNode *node)
{
  xmlFreeNode ((xmlNodePtr) node);
}

/**
 * glade_xml_context_get_doc:
 * @context: a #GladeXmlContext
 *
 * Get the #GladeXmlDoc this @context refers to.
 *
 * Returns: (transfer none): the #GladeXmlDoc that the @context refers to
 */
GladeXmlDoc *
glade_xml_context_get_doc (GladeXmlContext *context)
{
  return context->doc;
}

/**
 * glade_xml_dump_from_context:
 * @context: a #GladeXmlContext
 *
 * Dump the XML string from the context.
 *
 * Returns: the XML string, free the allocated memory with g_free() after use
 */
gchar *
glade_xml_dump_from_context (GladeXmlContext *context)
{
  GladeXmlDoc *doc;
  xmlChar *string = NULL;
  gchar *text;
  int size;

  doc = glade_xml_context_get_doc (context);
  xmlDocDumpFormatMemory (doc->doc, &string, &size, 1);

  text = claim_string (string);

  return text;
}

gboolean
glade_xml_node_is_comment (GladeXmlNode *node_in)
{
  xmlNodePtr node = (xmlNodePtr) node_in;
  return (node) ? node->type == XML_COMMENT_NODE : FALSE;
}

static inline gboolean
glade_xml_node_is_comment_or_text (GladeXmlNode *node_in)
{
  xmlNodePtr node = (xmlNodePtr) node_in;
  return (node) ? node->type == XML_COMMENT_NODE || node->type == XML_TEXT_NODE : FALSE;
}

GladeXmlNode *
glade_xml_node_get_children (GladeXmlNode * node_in)
{
  xmlNodePtr node = (xmlNodePtr) node_in;
  xmlNodePtr children;

  children = node->children;
  while (glade_xml_node_is_comment_or_text ((GladeXmlNode *) children))
    children = children->next;

  return (GladeXmlNode *) children;
}

GladeXmlNode *
glade_xml_node_get_parent (GladeXmlNode *node_in)
{
  xmlNodePtr node = (xmlNodePtr) node_in;

  return (GladeXmlNode *) node->parent;
}


GladeXmlNode *
glade_xml_node_get_children_with_comments (GladeXmlNode *node_in)
{
  xmlNodePtr node = (xmlNodePtr) node_in;

  return (GladeXmlNode *) node->children;
}

GladeXmlNode *
glade_xml_node_next (GladeXmlNode *node_in)
{
  xmlNodePtr node = (xmlNodePtr) node_in;

  node = node->next;
  while (glade_xml_node_is_comment_or_text ((GladeXmlNode *) node))
    node = node->next;

  return (GladeXmlNode *) node;
}

GladeXmlNode *
glade_xml_node_next_with_comments (GladeXmlNode *node_in)
{
  xmlNodePtr node = (xmlNodePtr) node_in;

  return (GladeXmlNode *) node->next;
}

GladeXmlNode *
glade_xml_node_prev_with_comments (GladeXmlNode *node_in)
{
  xmlNodePtr node = (xmlNodePtr) node_in;

  return (GladeXmlNode *) node->prev;
}

const gchar *
glade_xml_node_get_name (GladeXmlNode *node_in)
{
  xmlNodePtr node = (xmlNodePtr) node_in;

  return CAST_BAD (node->name);
}

static GladeXmlDoc *
glade_xml_doc_new_from_doc (xmlDocPtr docptr)
{
  GladeXmlDoc *doc = g_new (GladeXmlDoc, 1);
  doc->doc = docptr;
  doc->reference_count = 1;

  return doc;
}

/**
 * glade_xml_doc_new:
 *
 * Creates a new #GladeXmlDoc.
 *
 * Returns: (transfer full): a new #GladeXmlDoc
 */
GladeXmlDoc *
glade_xml_doc_new (void)
{
  return glade_xml_doc_new_from_doc (xmlNewDoc (BAD_CAST ("1.0")));
}

/**
 * glade_xml_doc_ref:
 * @doc: a #GladeXmlDoc
 *
 * Increases the reference of the #GladeXmlDoc.
 *
 * Returns: (transfer full): the given #GladeXmlDoc
 */
GladeXmlDoc *
glade_xml_doc_ref (GladeXmlDoc *doc)
{
  g_return_val_if_fail (doc != NULL, NULL);

  g_atomic_int_inc (&doc->reference_count);
  return doc;
}

/**
 * glade_xml_doc_unref:
 * @doc: a #GladeXmlDoc
 *
 * Decreases the reference of the #GladeXmlDoc.
 */
void
glade_xml_doc_unref (GladeXmlDoc *doc)
{
  if (!doc)
    return;

  if (g_atomic_int_dec_and_test (&doc->reference_count))
    {
      g_clear_pointer (&doc->doc, xmlFreeDoc);
      g_free (doc);
    }
}

void
glade_xml_doc_set_root (GladeXmlDoc *doc_in, GladeXmlNode *node_in)
{
  xmlNodePtr node = (xmlNodePtr) node_in;

  g_return_if_fail (doc_in != NULL);

  xmlDocSetRootElement (doc_in->doc, node);
}

gint
glade_xml_doc_save (GladeXmlDoc *doc_in, const gchar *full_path)
{
  g_return_val_if_fail (doc_in != NULL, 0);

  xmlKeepBlanksDefault (0);
  return xmlSaveFormatFileEnc (full_path, doc_in->doc, "UTF-8", 1);
}

/**
 * glade_xml_doc_get_root:
 * @doc: a #GladeXmlDoc
 *
 * Returns: (transfer none): the #GladeXmlNode that is the document root of @doc
 */
GladeXmlNode *
glade_xml_doc_get_root (GladeXmlDoc *doc)
{
  xmlNodePtr node;

  g_return_val_if_fail (doc != NULL, NULL);

  node = xmlDocGetRootElement (doc->doc);

  return (GladeXmlNode *) node;
}

gboolean
glade_xml_load_sym_from_node (GladeXmlNode *node_in,
                              GModule      *module,
                              gchar        *tagname,
                              gpointer     *sym_location)
{
  static GModule *self = NULL;
  gboolean retval = FALSE;
  gchar *buff;

  if (!self)
    self = g_module_open (NULL, 0);

  if ((buff = glade_xml_get_value_string (node_in, tagname)) != NULL)
    {
      if (!module)
        {
          g_warning ("Catalog specified symbol '%s' for tag '%s', "
                     "no module available to load it from !", buff, tagname);
          g_free (buff);
          return FALSE;
        }

      /* I use here a g_warning to signal these errors instead of a dialog 
       * box, as if there is one of this kind of errors, there will probably 
       * a lot of them, and we don't want to inflict the user the pain of 
       * plenty of dialog boxes.  Ideally, we should collect these errors, 
       * and show all of them at the end of the load process.
       *
       * I dont know who wrote the above in glade-property-def.c, but
       * its a good point... makeing a bugzilla entry.
       *  -Tristan
       *
       * XXX http://bugzilla.gnome.org/show_bug.cgi?id=331797
       */
      if (g_module_symbol (module, buff, sym_location) ||
          g_module_symbol (self, buff, sym_location))
        retval = TRUE;
      else
        g_warning ("Could not find %s in %s or in global namespace\n",
                   buff, g_module_name (module));

      g_free (buff);
    }
  return retval;
}

GladeXmlNode *
glade_xml_doc_new_comment (GladeXmlDoc *doc, const gchar *comment)
{
  return (GladeXmlNode *) xmlNewDocComment (doc->doc, BAD_CAST (comment));
}

GladeXmlNode *
glade_xml_node_add_prev_sibling (GladeXmlNode *node, GladeXmlNode *new_node)
{
  return (GladeXmlNode *) xmlAddPrevSibling ((xmlNodePtr) node, (xmlNodePtr) new_node);
}

GladeXmlNode *
glade_xml_node_add_next_sibling (GladeXmlNode *node, GladeXmlNode *new_node)
{
  return (GladeXmlNode *) xmlAddNextSibling ((xmlNodePtr) node, (xmlNodePtr) new_node);
}


/* Private API */
#include "glade-private.h"

void
_glade_xml_error_reset_last (void)
{
  xmlResetLastError ();
}

gchar *
_glade_xml_error_get_last_message ()
{
  xmlErrorPtr error = xmlGetLastError ();

  if (error)
    return g_strdup_printf ("Error parsing file '%s' on line %d \n%s",
                            error->file, error->line, error->message);
  return NULL;
}
