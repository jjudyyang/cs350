
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
#include <sys/vfsuio.h>

static int
VFSUIO_Read(Handle *handle, void *buf, uint64_t len, uint64_t off)
{
    ASSERT(handle->type == HANDLE_TYPE_FILE);

    // XXX: Need to pin memory

    return VFS_Read(handle->vnode, buf, len, off);
}

static int
VFSUIO_Write(Handle *handle, void *buf, uint64_t len, uint64_t off)
{
    ASSERT(handle->type == HANDLE_TYPE_FILE);

    // XXX: Need to pin memory

    return VFS_Write(handle->vnode, buf, len, off);
}

static int
VFSUIO_Flush(Handle *handle)
{
    ASSERT(handle->type == HANDLE_TYPE_FILE);
    return -EINVAL;
}

static int
VFSUIO_Close(Handle *handle)
{
    int status;

    ASSERT(handle->type == HANDLE_TYPE_FILE);

    status = VFS_Close(handle->vnode);
    Handle_Free(handle);

    return status;
}

int
VFSUIO_Open(const char *path, Handle **handle)
{
    int status;

    Handle *hdl = Handle_Alloc();
    if (!hdl) {
	return -ENOMEM;
    }

    VNode *vn = VFS_Lookup(path);
    if (!vn) {
	Handle_Free(hdl);
	return -ENOENT;
    }

    status = VFS_Open(vn);
    if (status != 0) {
	// XXX: Release VNode
	Handle_Free(hdl);
	return status;
    }

    hdl->vnode = vn;
    hdl->type = HANDLE_TYPE_FILE;
    hdl->read = VFSUIO_Read;
    hdl->write = VFSUIO_Write;
    hdl->flush = VFSUIO_Flush;
    hdl->close = VFSUIO_Close;

    *handle = hdl;

    return 0;
}


