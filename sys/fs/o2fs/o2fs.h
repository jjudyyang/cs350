#ifndef __FS_O2FS_H__
#define __FS_O2FS_H__

#include <stdint.h>

#define MAXUSERNAMELEN     32
#define MAXNAMELEN         255

#define O2FS_VERSION_MAJOR 1
#define O2FS_VERSION_MINOR 0

#define SUPERBLOCK_MAGIC   "SUPRBLOK"
#define BNODE_MAGIC        "BLOKNODE"
#define BDIR_MAGIC         "DIRENTRY"

#define O2FS_DIRECT_PTR    64
#define O2FS_INDIRECT_PTR  64

typedef struct ObjID {
    uint8_t   hash[32];
    uint64_t  device;
    uint64_t  offset;
} ObjID;

typedef struct BPtr {
    uint8_t   hash[32];
    uint64_t  device;
    uint64_t  offset;
    uint64_t  _rsvd0;
    uint64_t  _rsvd1;
} BPtr;

#pragma pack(push, 1)
typedef struct SuperBlock {
    uint8_t   magic[8];
    uint16_t  versionMajor;
    uint16_t  versionMinor;
    uint32_t  _rsvd0;
    uint64_t  features;
    uint64_t  blockCount;
    uint64_t  blockSize;
    uint64_t  bitmapSize;
    uint64_t  bitmapOffset;
    uint64_t  version;
    ObjID     root;
    uint8_t   hash[32];
} SuperBlock;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct BNode {
    uint8_t   magic[8];
    uint16_t  versionMajor;
    uint16_t  versionMinor;
    uint32_t  flags;
    uint64_t  size;
    BPtr      indirect[O2FS_INDIRECT_PTR];
} BNode;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct BInd {
    BPtr direct[O2FS_DIRECT_PTR];
} BInd;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct BDirEntry {
    uint8_t   magic[8];
    ObjID     objId;
    uint64_t  size;
    uint64_t  flags;
    uint64_t  ctime;
    uint64_t  mtime;
    uint8_t   user[MAXUSERNAMELEN];
    uint8_t   group[MAXUSERNAMELEN];
    uint8_t   padding[104];
    uint8_t   name[MAXNAMELEN + 1];
} BDirEntry;
#pragma pack(pop)

#endif /* __FS_O2FS_H__ */
