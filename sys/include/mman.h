
#ifndef __SYS_MMAN_H__
#define __SYS_MMAN_H__

#define PROT_NONE	0x00
#define PROT_READ	0x01
#define PROT_WRITE	0x02
#define PROT_EXEC	0x04

#define MAP_FILE	0x0010
#define MAP_ANON	0x0020
#define MAP_FIXED	0x0040


#ifdef _KERNEL
#else /* _KERNEL */
int getpagesizes(size_t *pagesize, int nlem);
void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t len);
int mprotect(void *addr, size_t len, int prot);
int madvise(void *addr, size_t len, int behav);
#endif /* _KERNEL */

#endif /* __SYS_MMAN_H__ */

