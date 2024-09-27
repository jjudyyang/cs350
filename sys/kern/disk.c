/*
 * Copyright (c) 2013-2023 Ali Mashtizadeh
 * All rights reserved.
 */

#include <stdbool.h>
#include <stdint.h>

#include <sys/kassert.h>
#include <sys/kdebug.h>
#include <sys/queue.h>
#include <sys/sga.h>
#include <sys/disk.h>
#include <sys/spinlock.h>

LIST_HEAD(DiskList, Disk) diskList = LIST_HEAD_INITIALIZER(diskList);

void
Disk_AddDisk(Disk *disk)
{
    LIST_INSERT_HEAD(&diskList, disk, entries);
}

void
Disk_RemoveDisk(Disk *disk)
{
    LIST_REMOVE(disk, entries);
}

Disk *
Disk_GetByID(uint64_t ctrlNo, uint64_t diskNo)
{
    Disk *d;

    LIST_FOREACH(d, &diskList, entries) {
	if (d->ctrlNo == ctrlNo && d->diskNo == diskNo)
	    return d;
    }

    return NULL;
}

int
Disk_Read(Disk *disk, void *buf, SGArray *sga, DiskCB cb, void *arg)
{
    return disk->read(disk, buf, sga, cb, arg);
}

int
Disk_Write(Disk *disk, void *buf, SGArray *sga, DiskCB cb, void *arg)
{
    return disk->write(disk, buf, sga, cb, arg);
}

int
Disk_Flush(Disk *disk, void *buf, SGArray *sga, DiskCB cb, void *arg)
{
    return disk->flush(disk, buf, sga, cb, arg);
}

static void
Debug_Disks(int argc, const char *argv[])
{
    Disk *d;

    LIST_FOREACH(d, &diskList, entries) {
	kprintf("disk%lld.%lld: %lld Sectors\n",
		d->ctrlNo, d->diskNo, d->sectorCount);
    }
}

REGISTER_DBGCMD(disks, "List disks", Debug_Disks);

static void
Debug_DumpDisk(int argc, const char *argv[])
{
    uint64_t ctrlNo, diskNo;
    uint64_t sector;
    char buf[512];
    SGArray sga;

    if (argc != 4) {
	kprintf("Usage: dumpdisk CTRL# DISK# SECTOR\n");
	return;
    }

    ctrlNo = Debug_StrToInt(argv[1]);
    diskNo = Debug_StrToInt(argv[2]);
    sector = Debug_StrToInt(argv[3]);

    Disk *d = Disk_GetByID(ctrlNo, diskNo);

    sga.len = 1;
    sga.entries[0].offset = sector;
    sga.entries[0].length = 512;

    kprintf("Reading Sector %lld from disk%lld.%lld\n", sector, ctrlNo, diskNo);

    Disk_Read(d, &buf, &sga, NULL, NULL);
    Debug_PrintHex((const char *)&buf, 512, 0, 512);
}

REGISTER_DBGCMD(dumpdisk, "Dump disk sector", Debug_DumpDisk);

