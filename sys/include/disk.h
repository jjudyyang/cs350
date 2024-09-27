
#ifndef __SYS_DISK_H__
#define __SYS_DISK_H__

#include <sys/queue.h>
#include <sys/sga.h>

typedef void (*DiskCB)(int, void *);

typedef struct Disk Disk;
typedef struct Disk {
    void	*handle;					// Driver handle
    uint64_t	ctrlNo;						// Controller number
    uint64_t	diskNo;						// Disk number
    uint64_t	sectorSize;					// Sector Size
    uint64_t	sectorCount;					// Sector Count
    uint64_t	diskSize;					// Disk Size in Bytes
    int		(*read)(Disk *, void *, SGArray *, DiskCB, void *);	// Read
    int		(*write)(Disk *, void *, SGArray *, DiskCB, void *);	// Write
    int		(*flush)(Disk *, void *, SGArray *, DiskCB, void *);	// Flush
    LIST_ENTRY(Disk) entries;
} Disk;

void Disk_AddDisk(Disk *disk);
void Disk_RemoveDisk(Disk *disk);
Disk *Disk_GetByID(uint64_t ctrlNo, uint64_t diskNo);
int Disk_Read(Disk *disk, void * buf, SGArray *sga, DiskCB cb, void *arg);
int Disk_Write(Disk *disk, void * buf, SGArray *sga, DiskCB cb, void *arg);
int Disk_Flush(Disk *disk, void * buf, SGArray *sga, DiskCB cb, void *arg);

#endif /* __SYS_DISK_H__ */

