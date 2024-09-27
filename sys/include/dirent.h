
#ifndef __SYS_DIRENT_H__
#define __SYS_DIRENT_H__

#include <stdint.h>

#define NAME_MAX 256

struct dirent {
    ino_t	d_ino;
    uint16_t	d_reclen;
    uint8_t	d_type;
    uint8_t	d_namlen;
    char	d_name[NAME_MAX];
};

#define DT_UNKNOWN		0
#define DT_REG			1
#define DT_DIR			2
#define DT_LNK			3
#define DT_FIFO			4

#endif /* __SYS_DIRENT_H__ */

