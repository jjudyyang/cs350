
#ifndef __KMEM_H__
#define __KMEM_H__

#include <sys/queue.h>
#include <sys/spinlock.h>

typedef struct Page {
    uint32_t	refcount;	// Number of references
    uint32_t	pincount;	// Pin count (HW, Software)
    uint32_t	flags;		// Flags
    uint32_t	_unused;
} Page;

/*
 * Page Allocator
 */
void PAlloc_Init();
void PAlloc_AddRegion(uintptr_t start, uintptr_t len);
void *PAlloc_AllocPage();
void PAlloc_Retain(void *pg);
void PAlloc_Release(void *pg);

/*
 * XMem Memory Mapping Region
 */
typedef struct XMem XMem;

void XMem_Init();
XMem *XMem_New();
void XMem_Destroy(XMem *xmem);
uintptr_t XMem_GetBase(XMem *xmem);
uintptr_t XMem_GetLength(XMem *xmem);
bool XMem_Allocate(XMem *xmem, uintptr_t length);

/*
 * Slab Allocator
 */

#define SLAB_NAMELEN	32

typedef struct SlabElement {
    LIST_ENTRY(SlabElement)	free;
} SlabElement;

typedef struct Slab {
    uintptr_t		objsz;
    uintptr_t		align;
    XMem		*xmem;
    Spinlock		lock;
    uint64_t		objs;
    uint64_t		freeObjs;
    LIST_HEAD(SlabElementHead, SlabElement) freeList;
    // Debugging
    uint64_t		allocs;
    uint64_t		frees;
    char		name[SLAB_NAMELEN];
    LIST_ENTRY(Slab)	slabList;
} Slab;

void Slab_Init(Slab *slab, const char *name, uintptr_t objsz, uintptr_t align);
void *Slab_Alloc(Slab *slab) __attribute__((malloc));
void Slab_Free(Slab *slab, void *obj);

#define DECLARE_SLAB(_type) \
    _type *_type##_Alloc();		\
    void _type##_Free(_type *obj);

#define DEFINE_SLAB(_type, _pool) \
    _type *_type##_Alloc() {			\
	return (_type *)Slab_Alloc(_pool);	\
    }						\
    void _type##_Free(_type *obj) {		\
	Slab_Free(_pool, obj);			\
    }

#endif /* __KMEM_H__ */

