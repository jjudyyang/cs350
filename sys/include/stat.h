
#ifndef __SYS_STAT_H__
#define __SYS_STAT_H__

struct stat {
    ino_t		st_ino;
    // mode
    // uid
    // gid
    // timespec st_atim, st_mtim, st_ctim
    off_t		st_size;
    size_t		st_blocks;
    size_t		st_blksize;
};

#endif /* __SYS_STAT_H__ */

