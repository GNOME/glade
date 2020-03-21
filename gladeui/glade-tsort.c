/*
 * glade-tsort.c: a topological sorting algorithm implementation
 *
 * Copyright (C) 2013 Juan Pablo Ugarte.
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
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#include "glade-tsort.h"

/**
 * _node_edge_prepend:
 * @node: a _NodeEdge pointer or NULL
 * @predecessor:
 * @successor:
 * 
 * Adds a new node with the values @predecessor and @successor to the start of
 * @node list.
 * 
 * Returns: the new start of the node list.
 */
GList *
_node_edge_prepend  (GList *list,
                     gpointer predecessor,
                     gpointer successor)
{
  _NodeEdge *edge = g_slice_new (_NodeEdge);

  edge->predecessor = predecessor;
  edge->successor = successor;
  
  return g_list_prepend (list, edge);
}

static void
_node_edge_free (gpointer data)
{
  g_slice_free (_NodeEdge, data);
}

void
_node_edge_list_free (GList *list)
{
  g_list_free_full (list, _node_edge_free);
}

static inline void
tsort_remove_non_start_nodes (GList **nodes, GList *edges)
{
  GList *l;

  for (l = edges; l; l = g_list_next (l))
    {
      _NodeEdge *edge = l->data;
      *nodes = g_list_remove (*nodes, edge->successor);
    }
}


static inline gboolean
tsort_node_has_no_incoming_edge (gpointer node, GList *edges)
{
  GList *l;

  for (l = edges; l; l = g_list_next (l))
    {
      _NodeEdge *edge = l->data;

      if (node == edge->successor)
        return FALSE;
    }

  return TRUE;
}

/**
 * _glade_tsort:
 * @nodes: list of pointers to sort
 * @edges: pointer to the list of #_NodeEdge that conform the dependency 
 *         graph of @nodes.
 * 
 * Topological sorting implementation.
 * After calling this function only graph cycles (circular dependencies) are left
 * in @edges list. So if @edges is NULL it means the returned list has all the 
 * elements topologically sorted if not it means there are at least one
 * circular dependency.
 *
 * L ← Empty list that will contain the sorted elements
 * S ← Set of all nodes with no incoming edges
 * while S is non-empty do
 *     remove a node n from S
 *     insert n into L
 *     for each node m with an edge e from n to m do
 *         remove edge e from the graph
 *         if m has no other incoming edges then
 *             insert m into S
 * return L (a topologically sorted order if graph has no edges)
 *
 * see: http://en.wikipedia.org/wiki/Topological_sorting
 * 
 * Returns: a new list sorted by dependency including nodes only present in @edges
 */
GList *
_glade_tsort (GList **nodes, GList **edges)
{
  GList *sorted_nodes;

  /* L ← Empty list that will contain the sorted elements */
  sorted_nodes = NULL;

  /* S ← Set of all nodes with no incoming edges */
  tsort_remove_non_start_nodes (nodes, *edges);

  /* while S is non-empty do */
  while (*nodes)
    {
      GList *l, *next;
      gpointer n;

      /* remove a node n from S */
      n = (*nodes)->data;
      *nodes = g_list_delete_link (*nodes, *nodes);

      /* insert n into L */
      /*if (!glade_widget_get_parent (n)) this would be a specific optimization */
      sorted_nodes = g_list_prepend (sorted_nodes, n);

      /* for each node m ... */
      for (l = *edges; l; l = next)
        {
          _NodeEdge *edge = l->data;

          next = g_list_next (l);

          /* ... with an edge e from n to m do */
          if (edge->predecessor == n)
            {
              /* remove edge e from the graph */
              *edges = g_list_delete_link (*edges, l);

              /* if m has no other incoming edges then */
              if (tsort_node_has_no_incoming_edge (edge->successor, *edges))
                /* insert m into S */
                *nodes = g_list_prepend (*nodes, edge->successor);
              
              g_slice_free (_NodeEdge, edge);
            }
        }
    }

  /* if graph has edges then return error (graph has at least one cycle) */
#if 0   /* We rather not return NULL, caller must check if edge */
  if (*edges)
    {      
      g_list_free (sorted_nodes);
      _node_edge_list_free (*edges);
      *edges = NULL;
      return NULL;
    }
#endif  

  /* return L (a topologically sorted order if edge is NULL) */
  return g_list_reverse (sorted_nodes);
}
