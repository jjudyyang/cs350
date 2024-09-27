
#ifndef __SYS_VFS_H__
#define __SYS_VFS_H__

#include <sys/kmem.h>
#include <sys/stat.h>

typedef struct VFSOp VFSOp;
typedef struct VNode VNode;

typedef struct VFS {
    VFSOp		*op;
    Disk		*disk;
    Spinlock		lock;
    uint64_t		refCount;
    // FS Fields
    void		*fsptr;
    uint64_t		fsval;
    uint64_t		blksize;
    VNode		*root;
    void		*bitmap[16];
} VFS;

typedef struct VNode {
    VFSOp		*op;
    Disk		*disk;
    Spinlock		lock;
    uint64_t		refCount;
    // FS Fields
    void		*fsptr;
    uint64_t		fsval;
    VFS			*vfs;
} VNode;

DECLARE_SLAB(VFS);
DECLARE_SLAB(VNode);

typedef struct VFSOp {
    // VFS Operations
    int (*unmount)(VFS *fs);
    int (*getroot)(VFS *fs, VNode **dn);
    // VNode Operations
    int (*lookup)(VNode *dn, VNode **fn, const char *name);
    int (*open)(VNode *fn);
    int (*close)(VNode *fn);
    int (*stat)(VNode *fn, struct stat *sb);
    int (*read)(VNode *fn, void *buf, uint64_t off, uint64_t len);
    int (*write)(VNode *fn, void *buf, uint64_t off, uint64_t len);
    int (*readdir)(VNode *fn, void *buf, uint64_t len, uint64_t *off);
} VFSOp;

int VFS_MountRoot(Disk *root);
VNode *VFS_Lookup(const char *path);
// XXX: Release/Retain
int VFS_Stat(const char *path, struct stat *sb);
int VFS_Open(VNode *fn);
int VFS_Close(VNode *fn);
int VFS_Read(VNode *fn, void *buf, uint64_t off, uint64_t len);
int VFS_Write(VNode *fn, void *buf, uint64_t off, uint64_t len);
int VFS_ReadDir(VNode *fn, void *buf, uint64_t len, uint64_t *off);

#endif /* __SYS_VFS_H__ */

