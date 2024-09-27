
#ifndef __DIRENT_H__
#define __DIRENT_H__

#include <sys/dirent.h>

typedef struct DIR {
    int			fd;
    uint64_t		offset;
    struct dirent	de;
} DIR;

DIR *opendir(const char *);
struct dirent *readdir(DIR *);
void rewinddir(DIR *);
void seekdir(DIR *, long);
long telldir(DIR *);
int closedir(DIR *);

#endif /* __DIRENT_H__ */

