/*
 * Copyright (c) 2013-2023 Ali Mashtizadeh
 * All rights reserved.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <sys/kassert.h>
#include <sys/kdebug.h>
#include <sys/spinlock.h>
#include <sys/disk.h>
#include <sys/vfs.h>
#include <sys/handle.h>

extern VFS *O2FS_Mount(Disk *root);

static Spinlock vfsLock;
static VFS *rootFS;
static VNode *rootNode;

static Slab vfsSlab;
static Slab vnodeSlab;

DEFINE_SLAB(VFS, &vfsSlab);
DEFINE_SLAB(VNode, &vnodeSlab);

/**
 * VFS_MountRoot --
 *
 * Mount the root file system from a specific disk.
 */
int
VFS_MountRoot(Disk *rootDisk)
{
    int status;

    Spinlock_Init(&vfsLock, "VFS Lock", SPINLOCK_TYPE_NORMAL);

    Slab_Init(&vfsSlab, "VFS Slab", sizeof(VFS), 16);
    Slab_Init(&vnodeSlab, "VNode Slab", sizeof(VNode), 16);

    rootFS = O2FS_Mount(rootDisk);
    if (!rootFS)
	return -1;

    status = rootFS->op->getroot(rootFS, &rootNode);
    if (status < 0)
	Panic("Failed to get root VNode\n");

    return 0;
}

/**
 * VFS_Lookup --
 *
 * Lookup a VNode by a path.  This function recursively searches the directory 
 * heirarchy until the given path is found otherwise returns NULL if not found.
 */
VNode *
VFS_Lookup(const char *path)
{
    int status;
    const char *start = path + 1; 
    const char *end = path + 1;
    uint64_t len;
    VNode *curNode;
    VNode *oldNode;
    char curName[256];

    if (path[0] != '/')
	return NULL;

    status = rootFS->op->getroot(rootFS, &curNode);
    if (status < 0)
	Panic("Failed to get root VNode\n");

    while (1) {
	while (*end != '\0' && *end != '/')
	    end++;

	len = (size_t)(end - start);
	if (len == 0) {
	    // Handle root and trailing slash
	    return curNode;
	}
	if (len > 256) {
	    // Release
	    return NULL;
	}

	memcpy(curName, start, len);
	curName[len] = '\0';

	oldNode = curNode;
	curNode = NULL;
	status = oldNode->op->lookup(oldNode, &curNode, curName);
	if (status < 0) {
	    // Release
	    return NULL;
	}

	// Release oldNode

	if (*end == '\0') {
	    Log(vfs, "%s %lx\n", path, curNode);
	    return curNode;
	}

	start = end + 1;
	end = end + 1;
    }
}

/**
 * VFS_Stat --
 *
 * Return the struct stat that contains the file and directory information for 
 * a given VNode.
 */
int
VFS_Stat(const char *path, struct stat *sb)
{
    VNode *vn = VFS_Lookup(path);
    if (vn == NULL)
	return -ENOENT;

    vn->op->stat(vn, sb);

    // Release

    return 0;
}

/**
 * VFS_Open --
 *
 * Open a vnode for reading and writing.
 *
 * @param [in] fn VNode to open.
 *
 * @return Return status
 */
int
VFS_Open(VNode *fn)
{
    return fn->op->open(fn);
}

/**
 * VFS_Close --
 *
 * Close a vnode.
 *
 * @param [in] fn VNode to close.
 *
 * @return Return status
 */
int
VFS_Close(VNode *fn)
{
    return fn->op->close(fn);
}

/**
 * VFS_Read --
 *
 * Read from a vnode.
 *
 * @param [in] fn VNode to read from.
 * @param [in] buf Buffer to write the data to.
 * @param [in] off File offset in bytes.
 * @param [in] len Length to read in bytes.
 *
 * @return Return status
 */
int
VFS_Read(VNode *fn, void *buf, uint64_t off, uint64_t len)
{
    return fn->op->read(fn, buf, off, len);
}

/**
 * VFS_Write --
 *
 * Write from a vnode.
 *
 * @param [in] fn VNode to write to.
 * @param [in] buf Buffer to read the data from.
 * @param [in] off File offset in bytes.
 * @param [in] len Length to read in bytes.
 *
 * @return Return status
 */
int
VFS_Write(VNode *fn, void *buf, uint64_t off, uint64_t len)
{
    return fn->op->write(fn, buf, off, len);
}

/**
 * VFS_ReadDir --
 *
 * Read a directory entry from a vnode.
 *
 * @param [in] fn VNode to read from.
 * @param [in] buf Buffer to write the data to.
 * @param [in] off Directory offset in bytes.
 * @param [in] len Length to read in bytes.
 *
 * @return Return status
 */
int
VFS_ReadDir(VNode *fn, void *buf, uint64_t len, uint64_t *off)
{
    return fn->op->readdir(fn, buf, len, off);
}

