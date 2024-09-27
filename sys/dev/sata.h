/*
 * SATA Definitions
 */

#ifndef __SATA_H__
#define __SATA_H__

typedef struct SATAFIS_REG_H2D {
    uint8_t	type;		// 0x27
    uint8_t	flag;
    uint8_t	command;
    uint8_t	feature0;
    uint8_t	lba0;
    uint8_t	lba1;
    uint8_t	lba2;
    uint8_t	device;
    uint8_t	lba3;
    uint8_t	lba4;
    uint8_t	lba5;
    uint8_t	feature1;
    uint8_t	count0;
    uint8_t	count1;
    uint8_t	icc;
    uint8_t	control;
    uint8_t	_rsvd[4];
} SATAFIS_REG_H2D;

#define SATAFIS_REG_H2D_FLAG_COMMAND	0x80	/* Command Flag */

#define SATAFIS_TYPE_REG_H2D	0x27

#define SATAFIS_CMD_IDENTIFY	0xEC

#endif /* __SATA_H__ */

