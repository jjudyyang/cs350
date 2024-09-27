
#ifndef __SYS_LOADER_H__
#define __SYS_LOADER_H__

#include <sys/elf64.h>

bool Loader_CheckHeader(const Elf64_Ehdr *ehdr);
bool Loader_Load(Thread *thr, VNode *vn, void *buf, uint64_t len);

#endif /* __SYS_LOADER_H__ */

