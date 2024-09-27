
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <getopt.h>

#include <o2fs.h>

#define ROUND_UP(_a, _b) (((_a) + (_b) - 1)/(_b))

#define MAXBLOCKSIZE (64*1024*1024)

char tempbuf[MAXBLOCKSIZE];
char zerobuf[MAXBLOCKSIZE];

bool verbose = false;
bool hasManifest = false;
uint64_t diskSize = 0;
uint64_t diskOffset = 0;
uint64_t blockSize = 16*1024;
uint64_t bitmapSize;
int diskfd;
struct stat diskstat;

#define TOKEN_EOF	0
#define TOKEN_DIR	1
#define TOKEN_END	2
#define TOKEN_FILE	3
#define TOKEN_STRING	4

char *tokenBuf;
char *tokenCur;
char tokenString[512];

void LoadManifest(const char *manifest)
{
    int fd = open(manifest, O_RDONLY);
    struct stat manifeststat;

    if (fd < 0) {
	perror("Cannot open manifest");
	exit(1);
    }

    fstat(fd, &manifeststat);
    tokenBuf = malloc(manifeststat.st_size + 1);
    read(fd, tokenBuf, manifeststat.st_size);
    tokenBuf[manifeststat.st_size] = '\0';

    tokenCur = tokenBuf;
    tokenString[0] = '\0';
}

int GetToken()
{
    int i;

    while (*tokenCur == ' ' || *tokenCur == '\t' ||
	   *tokenCur == '\n' || *tokenCur == '\r')
	tokenCur++;

    for (i = 0; i < 512; i++)
    {
	tokenString[i] = tokenCur[i];
	if (tokenCur[i] == ' ' || tokenCur[i] == '\t' ||
	    tokenCur[i] == '\n' || tokenCur[i] == '\r' ||
	    tokenCur[i] == '\0')
	{
	    tokenString[i] = '\0';
	    tokenCur += i;
	    break;
	}
    }

    if (strcmp(tokenString, "") == 0)
	return TOKEN_EOF;
    if (strcmp(tokenString, "DIR") == 0)
	return TOKEN_DIR;
    if (strcmp(tokenString, "END") == 0)
	return TOKEN_END;
    if (strcmp(tokenString, "FILE") == 0)
	return TOKEN_FILE;
    return TOKEN_STRING;
}

void
FlushBlock(uint64_t offset, const void *buf, size_t len)
{
    assert(offset % blockSize == 0);
    assert(len <= blockSize);

    pwrite(diskfd, buf, len, offset);
    if (len != blockSize) {
	pwrite(diskfd, zerobuf, blockSize - len, offset + len);
    }
}


uint64_t
AppendBlock(const void *buf, size_t len)
{
    uint64_t offset = lseek(diskfd, 0, SEEK_CUR);

    FlushBlock(offset, buf, len);
    lseek(diskfd, blockSize, SEEK_CUR);

    return offset;
}

ssize_t ReadBlock(int fd, void *buf, size_t size) {
    size_t count = 0;

    while (count < size) {
	ssize_t ret = read(fd, (char *)buf + count, size);
	if (ret < 0) {
	    if (errno == EINTR) {
		continue;
	    } else if (count == 0) {
		return -1;
	    } else {
		break;
	    }
	} else if (ret == 0) {
	    break;
	} else {
	    count += ret;
	}
    }

    return count;
}

uint64_t
AppendEmpty(void)
{
	return AppendBlock(NULL, 0);
}

ObjID *AddFile(const char *file)
{
    int i = 0;
    int fd;
    ObjID *id = malloc(sizeof(ObjID));
    BNode node;

    memset(id, 0, sizeof(*id));
    memset(&node, 0, sizeof(node));
    memcpy(node.magic, BNODE_MAGIC, 8);
    node.versionMajor = O2FS_VERSION_MAJOR;
    node.versionMinor = O2FS_VERSION_MINOR;

    // Copy file
    fd = open(file, O_RDONLY);
    if (fd < 0) {
	perror("Cannot open file");
	exit(1);
    }
    while (1) {
	int len = ReadBlock(fd, tempbuf, blockSize);
	if (len < 0) {
	    perror("File read error");
	    exit(1);
	}
	if (len == 0) {
	    break;
	}

	node.direct[i].device = 0;
	node.direct[i].offset = AppendBlock(tempbuf, len);
	node.size += (uint64_t)len;
	i += 1;
    }
    close(fd);

    // Construct BNode
    uint64_t offset = AppendBlock(&node, sizeof(node));

    // Construct ObjID
    id->device = 0;
    id->offset = offset;

    return id;
}

ObjID *AddDirectory()
{
    int tok;
    BDirEntry *entries = malloc(128 * sizeof(BDirEntry));
    int entry = 0;
    ObjID *id = malloc(sizeof(ObjID));
    BNode node;

    while (1)
    {
	memset(&entries[entry], 0, sizeof(BDirEntry));
	tok = GetToken();
	if (tok == TOKEN_FILE) {
	    tok = GetToken();
	    printf("FILE %s\n", tokenString);
	    strncpy((char *)entries[entry].name, tokenString, MAXNAMELEN);

	    tok = GetToken();
	    ObjID *fobj = AddFile(tokenString);
	    memcpy(&entries[entry].objId, fobj, sizeof(ObjID));
	    free(fobj);
	} else if (tok == TOKEN_DIR) {
	    tok = GetToken();
	    printf("DIR %s\n", tokenString);
	    strncpy((char *)entries[entry].name, tokenString, MAXNAMELEN);
	    ObjID *dobj = AddDirectory();
	    memcpy(&entries[entry].objId, dobj, sizeof(ObjID));
	    free(dobj);
	} else if (tok == TOKEN_END) {
	    printf("END\n");
	    break;
	} else if (tok == TOKEN_EOF) {
	    printf("Unexpected end of file\n");
	    exit(1);
	} else {
	    printf("Unknown token '%s'\n", tokenString);
	    exit(1);
	}

	memcpy(entries[entry].magic, BDIR_MAGIC, 8);
	// entries[entry].size ...
	entries[entry].flags = 0;
	entries[entry].ctime = (uint64_t)time(NULL);
	entries[entry].mtime = (uint64_t)time(NULL);
	strncpy((char *)entries[entry].user, "root", MAXUSERNAMELEN);
	strncpy((char *)entries[entry].group, "root", MAXUSERNAMELEN);

	entry++;
    }

    // Write Directory

    // Make sure we fit into a single indirect
    // block to simplify the logic below.
    uint64_t size = entry * sizeof(BDirEntry);
    assert(size < blockSize);
    uint64_t offset = AppendBlock(entries, size);

    // Write Inode
    memset(&node, 0, sizeof(node));
    memcpy(node.magic, BNODE_MAGIC, 8);
    node.versionMajor = O2FS_VERSION_MAJOR;
    node.versionMinor = O2FS_VERSION_MINOR;
    node.size = size;
    node.direct[0].device = 0;
    node.direct[0].offset = offset;
    uint64_t nodeoff = AppendBlock(&node, sizeof(node));

    memset(id, 0, sizeof(*id));
    id->device = 0;
    id->offset = nodeoff;

    return id;
}

void BlockBitmap()
{
    off_t off = lseek(diskfd, 0, SEEK_CUR) / blockSize;

    /* Code below only supports using the first 16K blocks */
    assert(off < blockSize);

    memset(tempbuf, 0, MAXBLOCKSIZE);

    /* Mark the blocks in use up to the current offset */
    assert(off > 8);
    for (off_t i = 0; i < (off / 8); i++) {
	tempbuf[i] = 0xFF;
    }
    for (off_t i = 0; i < (off % 8); i++) {
	tempbuf[off / 8] |= 1 << i;
    }

    for (int i = 0; i < bitmapSize; i++)
	FlushBlock(blockSize + (blockSize * i), tempbuf + (blockSize * i), blockSize);
}

void Superblock(ObjID *objid)
{
    SuperBlock sb;

    memset(&sb, 0, sizeof(sb));
    memcpy(sb.magic, SUPERBLOCK_MAGIC, 8);
    sb.versionMajor = O2FS_VERSION_MAJOR;
    sb.versionMinor = O2FS_VERSION_MINOR;
    sb.blockCount = diskSize / blockSize;
    sb.blockSize = blockSize;
    sb.bitmapSize = bitmapSize;
    sb.bitmapOffset = blockSize;

    if (objid)
	memcpy(&sb.root, objid, sizeof(ObjID));

    FlushBlock(0, &sb, sizeof(sb));
}

void usage()
{
    printf("Usage: newfs_o2fs [OPTIONS] special-device\n");
    printf("Options:\n");
    printf("    -m, --manifest  Manifest of files to copy to file system\n");
    printf("    -s, --size      Size in megabytes of device or disk image\n");
    printf("    -v, --verbose   Verbose logging\n");
    printf("    -h, --help      Print help message\n");
}

int main(int argc, char * const *argv)
{
    int ch;
    int status;

    // Sanity check
    assert(sizeof(BDirEntry) == 512);

    struct option longopts[] = {
	{ "manifest",		required_argument,	NULL,	'm' },
	{ "size",		required_argument,	NULL,	's' },
	{ "verbose",		no_argument,		NULL,	'v' },
	{ "help",		no_argument,		NULL,	'h' },
	{ NULL,			0,			NULL,	0   }
    };

    while ((ch = getopt_long(argc, argv, "m:s:vh", longopts, NULL)) != -1)
    {
	switch (ch) {
	    case 'm':
		hasManifest = true;
		LoadManifest(optarg);
		break;
	    case 's':
		diskSize = atol(optarg) * 1024 * 1024;
		break;
	    case 'v':
		verbose = true;
		break;
	    case 'h':
		usage();
		return 0;
	    default:
		usage();
		return 1;
	}
    }

    argc -= optind;
    argv += optind;

    if (argc != 1) {
	usage();
	return 1;
    }

    diskfd = open(argv[0], O_RDWR | O_CREAT, 0660);
    if (diskfd < 0) {
	perror("Cannot open special device or disk image");
	return 1;
    }

    status = fstat(diskfd, &diskstat);
    if (status < 0) {
	perror("Cannot fstat special device or disk image");
	return 1;
    }

    if (diskstat.st_size == 0 && diskSize == 0) {
	printf("Error: Must specify size for disk images\n");
	usage();
	return 1;
    }

    if (diskstat.st_size == 0)
	FlushBlock(diskSize - blockSize, zerobuf, blockSize);

    /* Skip superblock */
    diskOffset = blockSize;
    lseek(diskfd, diskOffset, SEEK_SET);
    memset(zerobuf, 0, MAXBLOCKSIZE);

    /* Zero the bitmap (and skip past it) */
    bitmapSize = ROUND_UP(diskSize / (blockSize * 8), blockSize);
    for (int i = 0; i < bitmapSize; i++)
	AppendBlock(zerobuf, blockSize);

    ObjID *root = NULL;
    if (hasManifest) {
	int tok;

	tok = GetToken();
	if (tok != TOKEN_DIR) {
	    printf("Expected 'DIR' token, but found '%s'\n", tokenString);
	    exit(1);
	}
	tok = GetToken();
	if (tok != TOKEN_STRING || strcmp(tokenString, "/") != 0) {
	    printf("Expected '/' token\n");
	    exit(1);
	}

	root = AddDirectory();
	tok = GetToken();
	if (tok != TOKEN_EOF) {
	    printf("Expected end-of-file, but found '%s'\n", tokenString);
	    exit(1);
	}
    }

    /* Write bitmap */
    BlockBitmap();

    Superblock(root);
    free(root);

    close(diskfd);
}

