
#ifndef __FS_O2FS_H__
#define __FS_O2FS_H__

/*
 * *** Disk Layout ***
 *
 * +---------------------+
 * | Super Blocks (4MBs) |
 * +---------------------+
 * |                     |
 *           .
 *           .
 *           .
 * |                     |
 * +---------------------+
 * | Super Blocks (4MBs) |
 * +---------------------+
 *
 */

#define MAXUSERNAMELEN	32
#define MAXNAMELEN	255

/*
 * *** Version History ***
 * 1.0		Initial release
 */

#define O2FS_VERSION_MAJOR	1
#define O2FS_VERSION_MINOR	0

/*
 * Object ID: Pointer to a block node
 */
typedef struct ObjID {
    uint8_t		hash[32];
    uint64_t		device;
    uint64_t		offset;
} ObjID;

/*
 * Super block
 */
typedef struct SuperBlock
{
    uint8_t		magic[8];	/* Superblock Magic */
    uint16_t		versionMajor;	/* Major Version Number */
    uint16_t		versionMinor;	/* Minor Version Number */
    uint32_t		_rsvd0;
    uint64_t		features;	/* Feature Flags */
    uint64_t		blockCount;	/* Total Blocks */
    uint64_t		blockSize;	/* Block Size in Bytes */
    uint64_t		bitmapSize;	/* Free Bitmap Size in Bytes */
    uint64_t		bitmapOffset;	/* Free Bitmap Offset */

    uint64_t		version;	/* Snapshot version */
    ObjID		root;		/* Root Tree */

    uint8_t		hash[32];
} SuperBlock;

#define SUPERBLOCK_MAGIC	"SUPRBLOK"

/*
 * Block Pointer: Address raw blocks on the disk
 */
typedef struct BPtr {
    uint8_t		hash[32];
    uint64_t		device;
    uint64_t		offset;
    uint64_t		_rsvd0;
    uint64_t		_rsvd1;
} BPtr;


#define O2FS_DIRECT_PTR		(64)

/*
 * Block Nodes: Contain indirect pointers to pieces of a file
 */
typedef struct BNode
{
    uint8_t		magic[8];
    uint16_t		versionMajor;
    uint16_t		versionMinor;
    uint32_t		flags;
    uint64_t		size;

    BPtr		direct[O2FS_DIRECT_PTR];
} BNode;

#define BNODE_MAGIC	"BLOKNODE"

/*
 * Directory entries are exactly 512 bytes
 */
typedef struct BDirEntry
{
    uint8_t		magic[8];
    ObjID		objId;
    uint64_t		size;
    uint64_t		flags;
    uint64_t		ctime;
    uint64_t		mtime;
    uint8_t		user[MAXUSERNAMELEN]; // Not null terminated
    uint8_t		group[MAXUSERNAMELEN];
    uint8_t		padding[104];
    uint8_t		name[MAXNAMELEN+1]; // Null terminated
} BDirEntry;

#define BDIR_MAGIC	"DIRENTRY"

#endif /* __FS_O2FS_H__ */

