
#include <stdbool.h>
#include <stdint.h>

#include <sys/kassert.h>
#include <sys/queue.h>
#include <sys/kmem.h>
#include <sys/handle.h>
#include <sys/thread.h>
#include <sys/syscall.h>

Slab handleSlab;

void
Handle_GlobalInit()
{
    Slab_Init(&handleSlab, "Handle Objects", sizeof(Handle), 16);
}

DEFINE_SLAB(Handle, &handleSlab);

void
Handle_Init(Process *proc)
{
    int i;

    for (i = 0; i < PROCESS_HANDLE_SLOTS; i++) {
	TAILQ_INIT(&proc->handles[i]);
    }
}

void
Handle_Destroy(Process *proc)
{
    int i;
    Handle *handle, *handle_tmp;

    for (i = 0; i < PROCESS_HANDLE_SLOTS; i++) {
	TAILQ_FOREACH_SAFE(handle, &proc->handles[i], handleList, handle_tmp) {
	    TAILQ_REMOVE(&proc->handles[i], handle, handleList);
	    (handle->close)(handle);
	}
    }
}

uint64_t
Handle_Add(Process *proc, Handle *handle)
{
    int slot;

    handle->fd = proc->nextFD;
    proc->nextFD++;
    handle->processId = proc->pid;

    slot = handle->fd % PROCESS_HANDLE_SLOTS;

    TAILQ_INSERT_HEAD(&proc->handles[slot], handle, handleList);

    return handle->fd;
}

void
Handle_Remove(Process *proc, Handle *handle)
{
    int slot = handle->fd % PROCESS_HANDLE_SLOTS;

    TAILQ_REMOVE(&proc->handles[slot], handle, handleList);
}

Handle *
Handle_Lookup(Process *proc, uint64_t fd)
{
    int slot = fd % PROCESS_HANDLE_SLOTS;
    Handle *handle;

    TAILQ_FOREACH(handle, &proc->handles[slot], handleList) {
	if (handle->fd == fd)
	    return handle;
    }

    return NULL;
}

