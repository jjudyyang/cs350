
#ifndef __SGA_H__
#define __SGA_H__

#define SGARRAY_MAX_ENTRIES	32

typedef struct SGEntry
{
    uint64_t	offset;
    uint64_t	length;
} SGEntry;

typedef struct SGArray
{
    uint32_t	len;
    SGEntry	entries[SGARRAY_MAX_ENTRIES];
} SGArray;

void SGArray_Init(SGArray *sga);
int SGArray_Append(SGArray *sga, uint64_t off, uint64_t len);
void SGArray_Dump(SGArray *sga);

#endif /* __SGA_H__ */

