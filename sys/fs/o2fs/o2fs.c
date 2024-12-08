
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <sys/kassert.h>
#include <sys/kmem.h>
#include <sys/spinlock.h>
#include <sys/disk.h>
#include <sys/bufcache.h>
#include <sys/vfs.h>
#include <sys/dirent.h>
#include <sys/thread.h>

#include "o2fs.h"

VFS *O2FS_Mount(Disk *disk);
int O2FS_Unmount(VFS *fs);
int O2FS_GetRoot(VFS *fs, VNode **dn);
int O2FS_Lookup(VNode *dn, VNode **fn, const char *name);
int O2FS_Open(VNode *fn);
int O2FS_Close(VNode *fn);
int O2FS_Stat(VNode *fn, struct stat *statinfo);
int O2FS_Read(VNode *fn, void *buf, uint64_t off, uint64_t len);
int O2FS_Write(VNode *fn, void *buf, uint64_t off, uint64_t len);
int O2FS_ReadDir(VNode *fn, void *buf, uint64_t len, uint64_t *off);

static VFSOp O2FSOperations = {
    .unmount = O2FS_Unmount,
    .getroot = O2FS_GetRoot,
    .lookup = O2FS_Lookup,
    .open = O2FS_Open,
    .close = O2FS_Close,
    .stat = O2FS_Stat,
    .read = O2FS_Read,
    .write = O2FS_Write,
    .readdir = O2FS_ReadDir,
};

VFS *
O2FS_Mount(Disk *disk)
{
    int status;
    VFS *fs = VFS_Alloc();
    BufCacheEntry *entry;
    SuperBlock *sb;

    ASSERT(sizeof(BDirEntry) == 512);

    if (!fs)
	return NULL;

    status = BufCache_Read(disk, 0, &entry);
    if (status < 0) {
	Alert(o2fs, "Disk cache read failed\n");
	return NULL;
    }

    // Read superblock
    sb = entry->buffer;
    if (memcmp(sb->magic, SUPERBLOCK_MAGIC, 8) != 0) {
	Alert(o2fs, "Invalid file system\n");
	BufCache_Release(entry);
	return NULL;
    }
    if (sb->versionMajor != O2FS_VERSION_MAJOR ||
	sb->versionMinor != O2FS_VERSION_MINOR) {
	Alert(o2fs, "Unsupported file system version\n");
	BufCache_Release(entry);
	return NULL;
    }

    // Read bitmap
    for (int i = 0; i < sb->bitmapSize; i++) {
	ASSERT(i < 16);

	BufCacheEntry *bentry;
	uint64_t offset = sb->bitmapOffset + i * sb->blockSize;

	if (BufCache_Read(disk, offset, &bentry) < 0) {
	    Alert(o2fs, "Bitmap read failed\n");
	    for (i = 0; i < 16; i++)
		BufCache_Release(fs->bitmap[i]);
	    BufCache_Release(entry);
	    return NULL;
	}

	fs->bitmap[i] = bentry;
    }

    DLOG(o2fs, "File system mounted\n");
    DLOG(o2fs, "Root @ 0x%llx\n", sb->root.offset);

    fs->fsptr = entry;
    fs->fsval = sb->root.offset;
    fs->blksize = sb->blockSize;

    // Setup VFS structure
    fs->op = &O2FSOperations;
    fs->disk = disk;
    Spinlock_Init(&fs->lock, "O2FS Lock", SPINLOCK_TYPE_NORMAL);
    fs->refCount = 1;
    fs->root = NULL;

    status = O2FS_GetRoot(fs, &fs->root);
    if (status < 0) {
	Alert(o2fs, "Mount failed");
	BufCache_Release(entry);
	return NULL;
    }

    return fs;
}

int
O2FS_Unmount(VFS *fs)
{
    NOT_IMPLEMENTED();
    return -1;
}

/**
 * O2FSBAlloc --
 *
 * Allocate a block.
 *
 * @param [in] vfs VFS Instance.
 *
 * @return Block number.
 */
uint64_t
O2FSBAlloc(VFS *fs)
{
    for (int i = 0; i < 16; i++) {
	char *bitmap;
	BufCacheEntry *bentry = fs->bitmap[i];

	if (fs->bitmap[i] == NULL)
	    break;

	bitmap = bentry->buffer;

	// XXX: Check for end of disk
	for (int b = 0; b < fs->blksize; b++) {
	    for (int bi = 0; bi < 8; bi++) {
		if (((bitmap[b] >> bi) & 0x1) == 0) {
		    /* Set bit */
		    bitmap[b] |= (1 << bi);

		    /* Write bitmap */
		    BufCache_Write(bentry);

		    /*
		     * Block index is the sum of:
		     *   blksize*8 blocks per bitmap entry
		     *   8 blocks per bitmap byte
		     *   bit #
		     */
		    uint64_t blk = fs->blksize*8*i + 8*b + bi;
		    DLOG(o2fs, "BAlloc %lu\n", blk);
		    return blk;
		}
	    }
	}
    }

    Alert(o2fs, "Out of space!\n");
    return 0;
}

/**
 * O2FSBFree --
 *
 * Free a block.
 *
 * @param [in] vfs VFS Instance.
 * @param [in] block Block number.
 */
void
O2FSBFree(VFS *fs, uint64_t block)
{
    uint64_t bitoff = block & 0x7;
    uint64_t bytoff = (block >> 8) % fs->blksize;
    uint64_t bufoff = block / (fs->blksize*8);

    DLOG(o2fs, "BFree %lu\n", block);

    BufCacheEntry *bentry = fs->bitmap[bufoff];
    ASSERT(bentry != NULL);

    char *bitmap = bentry->buffer;

    /* Mask out the bit */
    bitmap[bytoff] &= ~(1 << bitoff);

    /* Write the bitmap */
    BufCache_Write(bentry);
}

/**
 * O2FSLoadVNode --
 *
 * Load a VNode from the disk given an ObjID.
 *
 * @param [in] vfs VFS Instance.
 * @param [in] oobjid Object ID.
 */
VNode *
O2FSLoadVNode(VFS *fs, ObjID *objid)
{
    int status;
    VNode *vn;
    BNode *bn;
    BufCacheEntry *entry;

    status = BufCache_Read(fs->disk, objid->offset, &entry);
    if (status < 0) {
	Alert(o2fs, "disk read error\n");
	return NULL;
    }

    bn = entry->buffer;
    if (memcmp(&bn->magic, BNODE_MAGIC, 8) != 0) {
	Alert(o2fs, "bad BNode magic\n");
	BufCache_Release(entry);
	return NULL;
    }
    if (bn->versionMajor != O2FS_VERSION_MAJOR ||
	bn->versionMinor != O2FS_VERSION_MINOR) {
	Alert(o2fs, "unsupported BNode version\n");
	BufCache_Release(entry);
	return NULL;
    }

    vn = VNode_Alloc();
    if (!vn) {
	return NULL;
    }

    vn->op = &O2FSOperations;
    vn->disk = fs->disk;
    Spinlock_Init(&vn->lock, "VNode Lock", SPINLOCK_TYPE_NORMAL);
    vn->refCount = 1;
    vn->fsptr = entry;
    vn->vfs = fs;

    return vn;
}

/**
 * O2FSGrowVNode --
 *
 * Grow a VNode.
 *
 * @param [in] vfs VFS Instance.
 * @param [in] bn BNode for the object.
 * @param [in] filesz New file size.
 *
 * @return 0 on success, otherwise error code.
 */
int
O2FSGrowVNode(VNode *vn, uint64_t filesz)
{
    VFS *fs = vn->vfs;
    BufCacheEntry *bufEntry = (BufCacheEntry *)vn->fsptr;
    BNode *node = bufEntry->buffer;
    uint64_t startBlock = (node->size + fs->blksize - 1) / fs->blksize;

    if (filesz > (fs->blksize * O2FS_DIRECT_PTR * O2FS_INDIRECT_PTR))
        return -EINVAL;

    BufCacheEntry *indirectCache = NULL;
    uint64_t requiredBlocks = (filesz + fs->blksize - 1) / fs->blksize;

    for (uint64_t blkIdx = startBlock; blkIdx < requiredBlocks; blkIdx++) {
        
        uint64_t indirectPos = blkIdx / O2FS_INDIRECT_PTR;
        uint64_t directPos = blkIdx % O2FS_INDIRECT_PTR;

        if (node->indirect[indirectPos].offset == 0) {
            uint64_t newBlock = O2FSBAlloc(fs);
            if (newBlock == 0) {
                return -ENOSPC;
            }

            node->indirect[indirectPos].device = 0;
            node->indirect[indirectPos].offset = newBlock * fs->blksize;

            BufCache_Read(vn->disk, node->indirect[indirectPos].offset, &indirectCache);
            memset(indirectCache->buffer, 0, fs->blksize);
            BufCache_Write(indirectCache);
        } else {
            BufCache_Read(vn->disk, node->indirect[indirectPos].offset, &indirectCache);
        }

        BInd *indirectPtr = (BInd *)indirectCache->buffer;

        if (indirectPtr->direct[directPos].offset != 0) {
            BufCache_Release(indirectCache);
            continue;
        }

        uint64_t newDirectBlock = O2FSBAlloc(fs);
        if (newDirectBlock == 0) {
            BufCache_Release(indirectCache);
            return -ENOSPC;
        }

        indirectPtr->direct[directPos].device = 0;
        indirectPtr->direct[directPos].offset = newDirectBlock * fs->blksize;

        BufCache_Write(indirectCache);
        BufCache_Release(indirectCache);
    }

    node->size = filesz;
    BufCache_Write(bufEntry);

    return 0;
}


/**
 * O2FSRetainVNode --
 *
 * Increment VNode reference count.
 *
 * @param [in] vn VNode.
 */
void
O2FSRetainVNode(VNode *vn)
{
    vn->refCount++;
}

/**
 * O2FSReleaseVNode --
 *
 * Decrement VNode reference count and release it if reaches zero.
 *
 * @param [in] vn VNode.
 */
void
O2FSReleaseVNode(VNode *vn)
{
    vn->refCount--;
    if (vn->refCount == 0) {
	BufCache_Release(vn->fsptr);
	Spinlock_Destroy(&vn->lock);
	VNode_Free(vn);
    }
}

int
O2FSResolveBuf(VNode *vnode, uint64_t blkNum, BufCacheEntry **resolvedEntry)
{
    BufCacheEntry *vnodeCache = (BufCacheEntry *)vnode->fsptr;
    BufCacheEntry *blockCache;
    BNode *nodeData = vnodeCache->buffer;
    int result;

    uint64_t indirectPos = blkNum / O2FS_INDIRECT_PTR;
    uint64_t directPos = blkNum % O2FS_DIRECT_PTR;

    BufCacheEntry *indirectBlockCache;
    
    result = BufCache_Read(vnode->disk, nodeData->indirect[indirectPos].offset, &indirectBlockCache);
    if (result < 0) {
        BufCache_Release(indirectBlockCache);
        return result;
    }

    BInd *indirectBlockData = indirectBlockCache->buffer;

    result = BufCache_Read(vnode->disk, indirectBlockData->direct[directPos].offset, &blockCache);
    if (result < 0) {
        BufCache_Release(indirectBlockCache);
        return result;
    }

    BufCache_Release(indirectBlockCache);

    *resolvedEntry = blockCache;

    return result;
}



   

/**
 * O2FS_GetRoot --
 *
 * Read the root directory Inode.
 *
 * @param [in] fs VFS Instance.
 * @param [out] dn VNode of the root directory.
 *
 * @return 0 on success, otherwise error.
 */
int
O2FS_GetRoot(VFS *fs, VNode **dn)
{
    int status;
    VNode *vn;
    BufCacheEntry *entry;
    BNode *bn;

    if (fs->root) {
	fs->root->refCount++;
	*dn = fs->root;
	return 0;
    }

    status = BufCache_Read(fs->disk, fs->fsval, &entry);
    if (status < 0) {
	Alert(o2fs, "disk read error\n");
	return status;
    }

    bn = entry->buffer;
    if (memcmp(&bn->magic, BNODE_MAGIC, 8) != 0) {
	Alert(o2fs, "bad BNode magic\n");
	BufCache_Release(entry);
	return -1;
    }
    if (bn->versionMajor != O2FS_VERSION_MAJOR ||
	bn->versionMinor != O2FS_VERSION_MINOR) {
	Alert(o2fs, "unsupported BNode version\n");
	BufCache_Release(entry);
	return -1;
    }

    vn = VNode_Alloc();
    vn->op = &O2FSOperations;
    vn->disk = fs->disk;
    Spinlock_Init(&vn->lock, "VNode Lock", SPINLOCK_TYPE_NORMAL);
    vn->refCount = 1;
    vn->fsptr = entry;
    vn->vfs = fs;

    *dn = vn;

    return 0;
}

void
O2FSDumpDirEntry(BDirEntry *entry)
{
    VLOG(o2fs, "%16s %08llx %08llx\n", entry->name, entry->objId.offset, entry->size);
}

/**
 * O2FS_Lookup --
 *
 * Lookup a directory entry within a given directory.
 *
 * @param [in] vn VNode of the directory to look through.
 * @param [out] fn VNode of the entry if found.
 * @param [in] name Name of the file.
 *
 * @return 0 on success, otherwise error.
 */
int
O2FS_Lookup(VNode *dn, VNode **fn, const char *name)
{
    int status;
    VFS *vfs = dn->vfs;
    BufCacheEntry *sbEntry = (BufCacheEntry *)vfs->fsptr;
    SuperBlock *sb = sbEntry->buffer;
    BufCacheEntry *dirEntry = (BufCacheEntry *)dn->fsptr;
    BNode *dirBN = dirEntry->buffer;
    uint64_t blocks = (dirBN->size + sb->blockSize - 1) / sb->blockSize;
    uint64_t b;

    DLOG(o2fs, "Lookup %lld %d\n", dirBN->size, blocks);

    for (b = 0; b < blocks; b++) {
	// Read block
	int e;
	int entryPerBlock = sb->blockSize / sizeof(BDirEntry);
	BufCacheEntry *entry;
	BDirEntry *dir;

	status = O2FSResolveBuf(dn, b, &entry);
	if (status != 0)
		return status;

	dir = (BDirEntry *)entry->buffer;
	for (e = 0; e < entryPerBlock; e++) {
	    if (strcmp((char *)dir[e].magic, BDIR_MAGIC) == 0) {
		O2FSDumpDirEntry(&dir[e]);

		if (strcmp((char *)dir[e].name, name) == 0) {
		    *fn = O2FSLoadVNode(vfs, &dir[e].objId);
		    return 0;
		}
	    }
	}

	BufCache_Release(entry);
    }

    return -1;
}

int
O2FS_Open(VNode *fn)
{
    return 0;
}

int
O2FS_Close(VNode *fn)
{
    return 0;
}

/**
 * O2FS_Stat --
 *
 * Stat a VNode.
 *
 * @param [in] fn VNode of the file to stat.
 * @param [out] statinfo Stat structure.
 *
 * @return 0 on success.
 */
int
O2FS_Stat(VNode *fn, struct stat *statinfo)
{
    VFS *vfs = fn->vfs;
    BufCacheEntry *sbEntry = (BufCacheEntry *)vfs->fsptr;
    SuperBlock *sb = sbEntry->buffer;
    BufCacheEntry *fileEntry = (BufCacheEntry *)fn->fsptr;
    BNode *fileBN = fileEntry->buffer;

    DLOG(o2fs, "O2FS %p %d\n", fileBN, fileBN->size);

    statinfo->st_ino = fileEntry->diskOffset;
    statinfo->st_size = fileBN->size;
    statinfo->st_blocks = (fileBN->size + sb->blockSize - 1) / sb->blockSize;
    statinfo->st_blksize = sb->blockSize;

    return 0;
}

/**
 * O2FS_Read --
 *
 * Read from a VNode.
 *
 * @param [in] fn VNode of the file.
 * @param [out] buf Buffer to read into.
 * @param [in] off Offset within the file.
 * @param [in] len Length of the buffer to read.
 *
 * @return number of bytes on success, otherwise negative error code.
 */
int
O2FS_Read(VNode *fn, void *buf, uint64_t off, uint64_t len)
{
    int status;
    VFS *vfs = fn->vfs;
    BufCacheEntry *sbEntry = (BufCacheEntry *)vfs->fsptr;
    SuperBlock *sb = sbEntry->buffer;
    BufCacheEntry *fileEntry = (BufCacheEntry *)fn->fsptr;
    BNode *fileBN = fileEntry->buffer;
    uint64_t blocks = (fileBN->size + sb->blockSize - 1) / sb->blockSize;
    uint64_t readBytes = 0;

    DLOG(o2fs, "Read %lld %d\n", fileBN->size, blocks);

    if (off > fileBN->size) {
	return 0;
    }

    if (off + len > fileBN->size) {
	len = fileBN->size - off;
    }

    while (1) {
	uint64_t b = off / sb->blockSize;
	uint64_t bOff = off % sb->blockSize;
	uint64_t bLen;
	BufCacheEntry *entry;

	if (bOff + len > sb->blockSize) {
	    bLen = sb->blockSize - bOff;
	} else {
	    bLen = len;
	}

	status = O2FSResolveBuf(fn, b, &entry);
	if (status != 0)
		return status;

	DLOG(o2fs, "READ %lx %lx %lld\n", buf, entry->buffer, bLen);
	memcpy(buf, entry->buffer + bOff, bLen);
	BufCache_Release(entry);

	readBytes += bLen;
	buf += bLen;
	off += bLen;
	len -= bLen;

	if (len == 0)
	    break;
    }

    return readBytes;
}

/**
 * O2FS_Write --
 *
 * Write to a VNode.
 *
 * @param [in] fn VNode of the file.
 * @param [in] buf Buffer to write out.
 * @param [in] off Offset within the file.
 * @param [in] len Length of the buffer to write.
 *
 * @return number of bytes on success, otherwise negative error code.
 */
int
O2FS_Write(VNode *fn, void *buf, uint64_t off, uint64_t len)
{
    int status;
    VFS *vfs = fn->vfs;
    BufCacheEntry *sbEntry = (BufCacheEntry *)vfs->fsptr;
    SuperBlock *sb = sbEntry->buffer;
    BufCacheEntry *fileEntry = (BufCacheEntry *)fn->fsptr;
    BNode *fileBN = fileEntry->buffer;
    uint64_t blocks = (fileBN->size + sb->blockSize - 1) / sb->blockSize;
    uint64_t readBytes = 0;

    DLOG(o2fs, "Write %lld %d\n", fileBN->size, blocks);

    // XXX: Check permissions

    if (fileBN->size < (off+len)) {
	status = O2FSGrowVNode(fn, off+len);
	if (status < 0)
	    return status;
    }

    while (1) {
	uint64_t b = off / sb->blockSize;
	uint64_t bOff = off % sb->blockSize;
	uint64_t bLen;
	BufCacheEntry *entry;

	if (bOff + len > sb->blockSize) {
	    bLen = sb->blockSize - bOff;
	} else {
	    bLen = len;
	}

	status = O2FSResolveBuf(fn, b, &entry);
	if (status != 0)
		return status;

	DLOG(o2fs, "WRITE %lx %lx %lld\n", buf, entry->buffer, bLen);
	memcpy(entry->buffer + bOff, buf, bLen);

	BufCache_Write(entry);
	BufCache_Release(entry);

	readBytes += bLen;
	buf += bLen;
	off += bLen;
	len -= bLen;

	if (len == 0)
	    break;
    }

    return readBytes;
}

/**
 * O2FS_ReadDir --
 *
 * Read a directory entry.
 *
 * @param [in] fn VNode of the directory.
 * @param [out] buf Buffer to read the directory entry into.
 * @param [in] len Length of the buffer.
 * @param [inout] off Offset to start from and return the next offset.
 *
 * @return 0 on success, otherwise error.
 */
int
O2FS_ReadDir(VNode *fn, void *buf, uint64_t len, uint64_t *off)
{
    int count = 0;
    int status;
    BufCacheEntry *fileEntry = (BufCacheEntry *)fn->fsptr;
    BNode *fileBN = fileEntry->buffer;
    BDirEntry dirEntry;
    struct dirent de;

    while (len >= sizeof(de)) {
	if (*off == fileBN->size)
	    return count;
	if (*off > fileBN->size)
	    return -EINVAL;

	// XXX: Check offset

	status = O2FS_Read(fn, &dirEntry, *off, sizeof(dirEntry));
	if (status != sizeof(dirEntry)) {
	    kprintf("Unexpected error reading directory: %d\n", status);
	    return -ENOTDIR;
	}
	if (strncmp((char *)&dirEntry.magic[0], BDIR_MAGIC, sizeof(dirEntry.magic)) != 0) {
	    return -ENOTDIR;
	}

	// XXX: Validation and fill in all fields
	strcpy(de.d_name, (char *)dirEntry.name);

	status = Copy_Out(&de, (uintptr_t)buf, sizeof(de));
	if (status != 0)
	    return status;

	*off += sizeof(dirEntry);
	buf += sizeof(de);
	len -= sizeof(de);

	count++;
    }

    return count;
}

//fixed
