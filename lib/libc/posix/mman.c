
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>

#if defined(__x86_64__)
#include <sys/cdefs.h>
#include <machine/amd64.h>
#else
#error "Unsupported Architecture!"
#endif

#include <syscall.h>

int
getpagesizes(size_t *pagesize, int nelem)
{
#if defined(__x86_64__)
    if (pagesize) {
	if (nelem >= 1)
	    pagesize[0] = PGSIZE;
	if (nelem >= 2)
	    pagesize[1] = LARGE_PGSIZE;
	if (nelem >= 3)
	    pagesize[2] = HUGE_PGSIZE;
	return (nelem > 3 ? 3 : nelem);
    }
    return 3;
#else
#error "Unsupported Architecture!"
#endif
}

void*
mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    void *realAddr;

    if (!(flags & MAP_FIXED)) {
	// Find an address
    }

    if (flags & MAP_ANON) {
	realAddr = OSMemMap(addr, len, prot | flags);
    } else {
	abort();
    }

    // XXX: Update mapping

    return realAddr;
}

int
munmap(void *addr, size_t len)
{
    // XXX: Update mappings

    return OSMemUnmap(addr, len);
}

int
mprotect(void *addr, size_t len, int prot)
{
    // XXX: Update mappings

    return OSMemProtect(addr, len, prot);
}

int
madvise(void *addr, size_t len, int behav)
{
    return 0;
}

