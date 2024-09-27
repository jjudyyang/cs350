
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <sys/kassert.h>
#include <sys/kmem.h>
#include <sys/spinlock.h>
#include <sys/disk.h>

#include "ioport.h"
#include "../ata.h"

/*
 * IDE Definitions
 */

#define IDE_PRIMARY_BASE	0x1F0
#define IDE_PRIMARY_DEVCTL	0x3F6
#define IDE_PRIMARY_IRQ		14

#define IDE_SECONDARY_BASE	0x170
#define IDE_SECONDARY_DEVCTL	0x376
#define IDE_SECONDARY_IRQ	15

// Port Offsets
#define IDE_DATAPORT		0
#define IDE_FEATURES		1
#define IDE_SECTORCOUNT		2
#define IDE_LBALOW		3
#define IDE_LBAMID		4
#define IDE_LBAHIGH		5
#define IDE_DRIVE		6
#define IDE_COMMAND		7   /* Write */
#define IDE_STATUS		7   /* Read */

// IDE Commands (PIO)
#define IDE_CMD_READ		0x20
#define IDE_CMD_READ_EXT	0x24
#define IDE_CMD_WRITE		0x30
#define IDE_CMD_WRITE_EXT	0x34
#define IDE_CMD_FLUSH		0xE7
#define IDE_CMD_IDENTIFY	0xEC

// Status
#define IDE_STATUS_ERR		0x01	/* Error */
#define IDE_STATUS_DRQ		0x08	/* Data Ready */
#define IDE_STATUS_SRV		0x10	/* Overlapped-mode Service Request */
#define IDE_STATUS_DF		0x20	/* Drive Fault Error */
#define IDE_STATUS_RDY		0x40	/* Ready */
#define IDE_STATUS_BSY		0x80	/* Busy */

#define IDE_CONTROL_SRST	0x04	/* Software Reset */

#define IDE_SECTOR_SIZE		512

typedef struct IDE
{
    uint16_t	base;		// Base Port
    uint16_t	devctl;		// Device Control
    uint8_t	lastDriveCode;	// Last Drive Code
    Spinlock	lock;
} IDE;

typedef struct IDEDrive
{
    IDE		*ide;		// IDE Controller
    int		drive;		// Drive Number
    bool	lba48;		// Supports 48-bit LBA
    uint64_t	size;		// Size of Disk
} IDEDrive;

bool IDE_HasController(IDE *ide);
void IDE_Reset(IDE *ide);
void IDE_Identify(IDE *ide, int drive);
int IDE_Read(Disk *disk, void *buf, SGArray *sga, DiskCB, void *arg);
int IDE_Write(Disk *disk, void *buf, SGArray *sga, DiskCB, void *arg);
int IDE_Flush(Disk *disk, void *buf, SGArray *sga, DiskCB, void *arg);
int IDE_ReadOne(IDEDrive *drive, void *buf, uint64_t off, uint64_t len);
int IDE_WriteOne(IDEDrive *drive, void *buf, uint64_t off, uint64_t len);

IDE primary;
IDEDrive primaryDrives[2];

void
IDE_Init()
{
    ASSERT(sizeof(ATAIdentifyDevice) == 512);

    primary.base = IDE_PRIMARY_BASE;
    primary.devctl = IDE_PRIMARY_DEVCTL;
    Spinlock_Init(&primary.lock, "IDE Primary Controller Lock",
		  SPINLOCK_TYPE_NORMAL);

    if (!IDE_HasController(&primary)) {
	kprintf("IDE: No controller detected\n");
	return;
    }

    Spinlock_Lock(&primary.lock);
    IDE_Reset(&primary);
    IDE_Identify(&primary, 0);
    IDE_Identify(&primary, 1);
    Spinlock_Unlock(&primary.lock);
}

int
IDEWaitForBusy(IDE *ide, bool wait)
{
    uint8_t status;

    ASSERT(Spinlock_IsHeld(&ide->lock));

    if (wait) {
	inb(ide->base + IDE_STATUS);
	inb(ide->base + IDE_STATUS);
	inb(ide->base + IDE_STATUS);
	inb(ide->base + IDE_STATUS);
    }

    while (1) {
	status = inb(ide->base + IDE_STATUS);
	if ((status & IDE_STATUS_BSY) == 0)
	    return status;
    }

    return 0xFF;
}

bool
IDE_HasController(IDE *ide)
{
    outb(ide->base + IDE_LBALOW, 0x41);
    outb(ide->base + IDE_LBAMID, 0x4D);

    if (inb(ide->base + IDE_LBALOW) != 0x41)
	return false;
    if (inb(ide->base + IDE_LBAMID) != 0x4D)
	return false;

    return true;
}

void
IDE_Reset(IDE *ide)
{
    uint8_t status;

    // Select the master drive, otherwise if the slave drive is active but not
    // connected, we will just read 0 from the status register.
    outb(ide->base + IDE_DRIVE, 0xA0);
    IDEWaitForBusy(ide, true);

    outb(ide->devctl, IDE_CONTROL_SRST);
    outb(ide->devctl, 0);

    status = IDEWaitForBusy(ide, true);
    if ((status & IDE_STATUS_RDY) != IDE_STATUS_RDY) {
	Log(ide, "Controller not ready!\n");
	return;
    }
}

void
IDE_SwapAndTruncateString(char *str, int len)
{
    int i;

    ASSERT(len % 2 == 0);

    for (i = 0; i < len/2; i++)
    {
	char tmp = str[2*i];
	str[2*i] = str[2*i+1];
	str[2*i+1] = tmp;
    }

    for (i = len - 1; i > 0; i--) {
	if (str[i] == ' ' || str[i] == '\0')
	    str[i] = '\0';
	else
	    break;
    }
}

void
IDE_Identify(IDE *ide, int drive)
{
    uint8_t driveCode;
    uint8_t status;
    ATAIdentifyDevice ident;

    ASSERT(drive == 0 || drive == 1);

    if (drive == 0)
	driveCode = 0xA0;
    else
	driveCode = 0xB0;

    outb(ide->base + IDE_DRIVE, driveCode);

    status = IDEWaitForBusy(ide, true);
    if ((status & IDE_STATUS_ERR) != 0) {
	Log(ide, "Error selecting drive %d\n", drive);
	return;
    }
    ide->lastDriveCode = driveCode;

    outb(ide->base + IDE_SECTORCOUNT, 0x00);
    outb(ide->base + IDE_LBALOW, 0x00);
    outb(ide->base + IDE_LBAMID, 0x00);
    outb(ide->base + IDE_LBAHIGH, 0x00);
    outb(ide->base + IDE_COMMAND, IDE_CMD_IDENTIFY);

    status = inb(ide->base + IDE_STATUS);
    if (status == 0) {
	Log(ide, "Drive %d not present\n", drive);
	return;
    }

    // XXX: Need timeout
    while (1) {
	if ((status & IDE_STATUS_BSY) == 0)
	    break;
	status = inb(ide->base + IDE_STATUS);
    }

    if ((status & IDE_STATUS_ERR) != 0) {
	Log(ide, "Error trying to identify drive %d\n", drive);
	return;
    }

    insw(ide->base, (void *)&ident, 256);

    // Cleanup model and serial for printing
    char model[41];
    char serial[21];

    memcpy(&model[0], &ident.model[0], 40);
    model[40] = '\0';
    IDE_SwapAndTruncateString(&model[0], 40);

    memcpy(&serial[0], &ident.serial[0], 20);
    serial[20] = '\0';
    IDE_SwapAndTruncateString(&serial[0], 20);

    Log(ide, "Drive %d Model: %s Serial: %s\n", drive, model, serial);
    Log(ide, "Drive %d %llu Sectors (%llu MBs)\n",
	    drive, ident.lbaSectors, ident.lbaSectors / 2048ULL);

    primaryDrives[drive].ide = &primary;
    primaryDrives[drive].drive = drive;
    primaryDrives[drive].lba48 = (ident.lbaSectors > (1 << 24));
    primaryDrives[drive].size = ident.lbaSectors;

    // Register Disk
    Disk *disk = PAlloc_AllocPage();
    if (!disk) {
	Panic("IDE: No memory!\n");
    }

    disk->handle = &primaryDrives[drive];
    disk->ctrlNo = 0;
    disk->diskNo = drive;
    disk->sectorSize = IDE_SECTOR_SIZE;
    disk->sectorCount = ident.lbaSectors;
    disk->diskSize = IDE_SECTOR_SIZE * ident.lbaSectors;
    disk->read = IDE_Read;
    disk->write = IDE_Write;
    disk->flush = IDE_Flush;

    Disk_AddDisk(disk);
}

int
IDE_Read(Disk *disk, void *buf, SGArray *sga, DiskCB cb, void *arg)
{
    int i;
    int status;
    IDEDrive *idedrive;

    idedrive = disk->handle;

    for (i = 0; i < sga->len; i++) {
	status = IDE_ReadOne(idedrive,
			     buf,
			     sga->entries[i].offset / 512,
			     sga->entries[i].length / 512);
	buf += sga->entries[i].length;
	if (status < 0)
	    return status;
    }

    return 0;
}

int
IDE_Write(Disk *disk, void *buf, SGArray *sga, DiskCB cb, void *arg)
{
    int i;
    int status;
    IDEDrive *idedrive;

    idedrive = disk->handle;

    for (i = 0; i < sga->len; i++) {
	status = IDE_WriteOne(idedrive,
			      buf,
			      sga->entries[i].offset / 512,
			      sga->entries[i].length / 512);
	buf += sga->entries[i].length;
	if (status < 0)
	    return status;
    }

    return 0;
}

int
IDE_Flush(Disk *disk, void *buf, SGArray *sga, DiskCB cb, void *arg)
{
    uint8_t driveCode;
    IDE *ide;
    IDEDrive *idedrive;

    idedrive = disk->handle;
    ide = idedrive->ide;

    if (idedrive->drive == 0)
	driveCode = 0xA0;
    else
	driveCode = 0xB0;

    Spinlock_Lock(&ide->lock);
    outb(ide->base + IDE_DRIVE, driveCode);
    outb(ide->base + IDE_COMMAND, IDE_CMD_FLUSH);

    IDEWaitForBusy(ide, false);
    Spinlock_Unlock(&ide->lock);

    return 0;
}

int
IDE_ReadOne(IDEDrive *drive, void *buf, uint64_t off, uint64_t len)
{
    bool lba48 = false;
    uint8_t driveCode;
    uint8_t status;
    IDE *ide = drive->ide;

    DLOG(ide, "read %llx %llx\n", off, len);

    ASSERT(drive->drive == 0 || drive->drive == 1);

    if (drive->drive == 0)
	driveCode = lba48 ? 0x40 : 0xE0;
    else
	driveCode = lba48 ? 0x50 : 0xF0;

    ASSERT(len < 0x10000);

    Spinlock_Lock(&ide->lock);
    if (driveCode != ide->lastDriveCode) {
	outb(ide->base + IDE_DRIVE, driveCode);

	// Need to wait for select to complete
	status = IDEWaitForBusy(ide, true);
	if ((status & IDE_STATUS_ERR) != 0) {
	    Spinlock_Unlock(&ide->lock);
	    Log(ide, "Error selecting drive %d\n", drive->drive);
	    return -1;
	}
	ide->lastDriveCode = driveCode;
    }

    if (lba48) {
	outb(ide->base + IDE_SECTORCOUNT, len >> 8);
	outb(ide->base + IDE_LBALOW, off >> 24);
	outb(ide->base + IDE_LBAMID, off >> 32);
	outb(ide->base + IDE_LBAHIGH, off >> 40);
    }
    outb(ide->base + IDE_SECTORCOUNT, len);
    outb(ide->base + IDE_LBALOW, off & 0xff);
    outb(ide->base + IDE_LBAMID, (off >> 8) & 0xff);
    outb(ide->base + IDE_LBAHIGH, (off >> 16) & 0xff);

    if (lba48)
	outb(ide->base + IDE_COMMAND, IDE_CMD_READ_EXT);
    else
	outb(ide->base + IDE_COMMAND, IDE_CMD_READ);

    status = IDEWaitForBusy(ide, false);
    if ((status & IDE_STATUS_ERR) != 0) {
	Spinlock_Unlock(&ide->lock);
	Log(ide, "Error trying read from drive %d\n", drive->drive);
	return -1;
    }

    int sectors;
    for (sectors = 0; sectors < len; sectors++)
    {
	uint8_t *b = buf + sectors * IDE_SECTOR_SIZE;
	insw(ide->base + IDE_DATAPORT, b, 256);

	status = IDEWaitForBusy(ide, true);
	if ((status & IDE_STATUS_ERR) != 0) {
	    Spinlock_Unlock(&ide->lock);
	    Log(ide, "Error reading from drive %d\n", drive->drive);
	    return -1;
	}
    }
    Spinlock_Unlock(&ide->lock);

    return 0;
}

int
IDE_WriteOne(IDEDrive *drive, void *buf, uint64_t off, uint64_t len)
{
    bool lba48 = false;
    uint8_t driveCode;
    uint8_t status;
    IDE *ide = drive->ide;

    DLOG(ide, "read %llx %llx\n", off, len);

    ASSERT(drive->drive == 0 || drive->drive == 1);

    if (drive->drive == 0)
	driveCode = lba48 ? 0x40 : 0xE0;
    else
	driveCode = lba48 ? 0x50 : 0xF0;

    ASSERT(len < 0x10000);

    Spinlock_Lock(&ide->lock);
    if (driveCode != ide->lastDriveCode) {
	outb(ide->base + IDE_DRIVE, driveCode);

	// Need to wait for select to complete
	status = IDEWaitForBusy(ide, true);
	if ((status & IDE_STATUS_ERR) != 0) {
	    Spinlock_Unlock(&ide->lock);
	    Log(ide, "Error selecting drive %d\n", drive->drive);
	    return -1;
	}
	ide->lastDriveCode = driveCode;
    }

    if (lba48) {
	outb(ide->base + IDE_SECTORCOUNT, len >> 8);
	outb(ide->base + IDE_LBALOW, off >> 24);
	outb(ide->base + IDE_LBAMID, off >> 32);
	outb(ide->base + IDE_LBAHIGH, off >> 40);
    }
    outb(ide->base + IDE_SECTORCOUNT, len);
    outb(ide->base + IDE_LBALOW, off & 0xff);
    outb(ide->base + IDE_LBAMID, (off >> 8) & 0xff);
    outb(ide->base + IDE_LBAHIGH, (off >> 16) & 0xff);

    if (lba48)
	outb(ide->base + IDE_COMMAND, IDE_CMD_WRITE_EXT);
    else
	outb(ide->base + IDE_COMMAND, IDE_CMD_WRITE);

    status = IDEWaitForBusy(ide, false);
    if ((status & IDE_STATUS_ERR) != 0) {
	Spinlock_Unlock(&ide->lock);
	Log(ide, "Error trying read from drive %d\n", drive->drive);
	return -1;
    }

    int sectors;
    for (sectors = 0; sectors < len; sectors++)
    {
	uint8_t *b = buf + sectors * IDE_SECTOR_SIZE;
	outsw(ide->base + IDE_DATAPORT, b, 256);

	status = IDEWaitForBusy(ide, true);
	if ((status & IDE_STATUS_ERR) != 0) {
	    Spinlock_Unlock(&ide->lock);
	    Log(ide, "Error reading from drive %d\n", drive->drive);
	    return -1;
	}
    }
    Spinlock_Unlock(&ide->lock);

    // XXX: Flush cache ...

    return 0;
}

