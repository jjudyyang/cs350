
#ifndef __SYS_HANDLE_H__
#define __SYS_HANDLE_H__

#include <sys/queue.h>
#include <sys/disk.h>
#include <sys/vfs.h>

struct Handle;
typedef struct Handle Handle;

#define HANDLE_TYPE_FILE	1
#define HANDLE_TYPE_CONSOLE	2

typedef TAILQ_HEAD(HandleQueue, Handle) HandleQueue;

typedef struct Handle {
    uint64_t		fd;				// FD Number
    uint64_t		type;				// Type
    uint64_t		processId;			// Process ID
    VNode		*vnode;				// File VNode
    TAILQ_ENTRY(Handle)	handleList;			// Hash table
    int (*read)(Handle *, void *, uint64_t, uint64_t);	// Read
    int (*write)(Handle *, void *, uint64_t, uint64_t);	// Write
    int (*flush)(Handle *);				// Flush
    int (*close)(Handle *);				// Close
} Handle;

DECLARE_SLAB(Handle);

#endif /* __SYS_HANDLE_H__ */

