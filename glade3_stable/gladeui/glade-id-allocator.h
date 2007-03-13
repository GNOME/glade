#ifndef __GLADE_ID_ALLOCATOR_H__
#define __GLADE_ID_ALLOCATOR_H__

typedef struct _GladeIDAllocator GladeIDAllocator;

struct _GladeIDAllocator
{
	guint n_words;
	guint32 *data;
};

GladeIDAllocator *	glade_id_allocator_new		(void);
void			glade_id_allocator_free		(GladeIDAllocator *allocator);
guint			glade_id_allocator_alloc	(GladeIDAllocator *allocator);
void			glade_id_allocator_release	(GladeIDAllocator *allocator, guint id);

#endif
