
#ifndef __SYS_MBUF_H__
#define __SYS_MBUF_H__

#define MBUF_TYPE_NULL			0
#define MBUF_TYPE_PACKETSTART		1
#define MBUF_TYPE_PACKETCONT		2
#define MBUF_TYPE_PACKETEND		3

#define MBUF_STATUS_NULL		0
#define MBUF_STATUS_READY		1
#define MBUF_STATUS_FAILED		2

typedef struct MBuf {
    uint64_t		vaddr;
    uint64_t		maddr;
    uint32_t		len;
    uint32_t		type;
    uint32_t		flags;
    uint32_t		status;
} MBuf;

#endif /* __SYS_MBUF_H__ */

