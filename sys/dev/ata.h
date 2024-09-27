/*
 * ATA Definitions
 */

#ifndef __ATA_H__
#define __ATA_H__

typedef struct ATAIdentifyDevice
{
    uint16_t	_rsvd0[10];	// 0-9
    uint8_t	serial[20];	// 10-19    - Serial
    uint16_t	_rsvd1[3];	// 20-22
    uint8_t	firmware[8];	// 23-26    - Firmware
    uint8_t	model[40];	// 27-46    - Model
    uint16_t	_rsvd2[16];	// 47-62 X
    uint16_t	dmaMode;	// 63	    - DMA Mode
    uint16_t	_rsvd3[11];	// 64-74 X
    uint16_t	queueDepth;	// 75	    - Queue Depth
    uint16_t	sataCap;	// 76	    - SATA Capabilities
    uint16_t	ncqCap;		// 77	    - NCQ Capabilities
    uint16_t	_rsvd4[8];	// 78-85
    uint16_t	deviceFlags;	// 86	    - Device Flags (48-bit Addressing)
    uint16_t	deviceFlags2;	// 87	    - Device Flags 2 (SMART)
    uint16_t	udmaMode;	// 88	    - Ultra DMA Mode
    uint16_t	_rsvd5[11];	// 89-99
    uint64_t	lbaSectors;	// 100-103  - User Addressable Logical Sectors
    uint16_t	_rsvd6[2];	// 104-105
    uint16_t	sectorSize;	// 106	    - Physical Sector Size
    uint16_t	_rsvd7[148];	// 107-254
    uint16_t	chksum;		// 255	    - Checksum
} ATAIdentifyDevice;

#endif /* __ATA_H__ */

