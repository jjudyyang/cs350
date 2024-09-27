
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <sys/kassert.h>
#include <sys/kdebug.h>
#include <sys/sysctl.h>

typedef struct SysCtlEntry {
    char	path[64];
    int		type;
    int		flags;
    char	description[128];
    void	*node;
} SysCtlEntry;

#define SYSCTL_STR(_PATH, _FLAGS, _DESCRIPTION, _DEFAULT) \
    { #_PATH, SYSCTL_TYPE_STR, _FLAGS, _DESCRIPTION, &SYSCTL_##_PATH },
#define SYSCTL_INT(_PATH, _FLAGS, _DESCRIPTION, _DEFAULT) \
    { #_PATH, SYSCTL_TYPE_INT, _FLAGS, _DESCRIPTION, &SYSCTL_##_PATH },
#define SYSCTL_BOOL(_PATH, _FLAGS, _DESCRIPTION, _DEFAULT) \
    { #_PATH, SYSCTL_TYPE_BOOL, _FLAGS, _DESCRIPTION, &SYSCTL_##_PATH },
SysCtlEntry SYSCTLTable[] = {
    SYSCTL_LIST
    { "", 0, 0, "", NULL },
};
#undef SYSCTL_STR
#undef SYSCTL_INT
#undef SYSCTL_BOOL

#define SYSCTL_STR(_PATH, _FLAGS, _DESCRIPTION, _DEFAULT) \
SysCtlString SYSCTL_##_PATH = { #_PATH, _DEFAULT };
#define SYSCTL_INT(_PATH, _FLAGS, _DESCRIPTION, _DEFAULT) \
SysCtlInt SYSCTL_##_PATH = { #_PATH, _DEFAULT };
#define SYSCTL_BOOL(_PATH, _FLAGS, _DESCRIPTION, _DEFAULT) \
SysCtlBool SYSCTL_##_PATH = { #_PATH, _DEFAULT };
SYSCTL_LIST
#undef SYSCTL_STR
#undef SYSCTL_INT
#undef SYSCTL_BOOL

int
SysCtl_Lookup(const char *path)
{
    int i;

    for (i = 0; SYSCTLTable[i].path[0] != '\0'; i++) {
	if (strcmp(path, SYSCTLTable[i].path) == 0)
	    return i;
    }

    return -1;
}

uint64_t
SysCtl_GetType(const char *node)
{
    int i = SysCtl_Lookup(node);
    if (i == -1) {
	return SYSCTL_TYPE_INVALID;
    }

    return SYSCTLTable[i].type;
}

void *
SysCtl_GetObject(const char *node)
{
    int i = SysCtl_Lookup(node);
    if (i == -1) {
	return NULL;
    }

    return SYSCTLTable[i].node;
}

uint64_t
SysCtl_SetObject(const char *node, void *obj)
{
    int i = SysCtl_Lookup(node);
    if (i == -1) {
	return ENOENT;
    }

    if (SYSCTLTable[i].flags == SYSCTL_FLAG_RO) {
	kprintf("Sysctl node is read-only!\n");
	return EACCES;
    }

    switch (SYSCTLTable[i].type) {
	case SYSCTL_TYPE_STR: {
	    SysCtlString *val = (SysCtlString *)SYSCTLTable[i].node;
	    memcpy(val, obj, sizeof(*val));
	}
	case SYSCTL_TYPE_INT: {
	    SysCtlInt *val = (SysCtlInt *)SYSCTLTable[i].node;
	    memcpy(val, obj, sizeof(*val));
	}
	case SYSCTL_TYPE_BOOL: {
	    SysCtlBool *val = (SysCtlBool *)SYSCTLTable[i].node;
	    memcpy(val, obj, sizeof(*val));
	}
    }

    return 0;
}

void
Debug_SysCtl(int argc, const char *argv[])
{
    int i;

    if (argc == 1) {
        kprintf("%-20s %s\n", "Name", "Description");
	for (i = 0; SYSCTLTable[i].path[0] != '\0'; i++) {
	    kprintf("%-20s %s\n", SYSCTLTable[i].path, SYSCTLTable[i].description);
	}

	return;
    }

    if (argc != 2 && argc != 3) {
	kprintf("Usage: sysctl NODE [VALUE]\n");
	return;
    }

    i = SysCtl_Lookup(argv[1]);
    if (i == -1) {
	kprintf("Unknown sysctl node!\n");
	return;
    }

    if (argc == 2) {
	switch (SYSCTLTable[i].type) {
	    case SYSCTL_TYPE_STR: {
		SysCtlString *val = (SysCtlString *)SYSCTLTable[i].node;
		kprintf("%s: %s\n", argv[1], val->value);
		break;
	    }
	    case SYSCTL_TYPE_INT: {
		SysCtlInt *val = (SysCtlInt *)SYSCTLTable[i].node;
		kprintf("%s: %ld\n", argv[1], val->value);
		break;
	    }
	    case SYSCTL_TYPE_BOOL: {
		SysCtlBool *val = (SysCtlBool *)SYSCTLTable[i].node;
		kprintf("%s: %s\n", argv[1], val->value ? "true" : "false");
		break;
	    }
	}

	return;
    }

    if (argc == 3) {
	if (SYSCTLTable[i].flags == SYSCTL_FLAG_RO) {
	    kprintf("Sysctl node is read-only!\n");
	    return;
	}

	switch (SYSCTLTable[i].type) {
	    case SYSCTL_TYPE_STR: {
		SysCtlString *val = (SysCtlString *)SYSCTLTable[i].node;
		strncpy(&val->value[0], argv[2], SYSCTL_STR_MAXLENGTH);
		break;
	    }
	    case SYSCTL_TYPE_INT: {
		SysCtlInt *val = (SysCtlInt *)SYSCTLTable[i].node;
		val->value = Debug_StrToInt(argv[2]);
		break;
	    }
	    case SYSCTL_TYPE_BOOL: {
		SysCtlBool *val = (SysCtlBool *)SYSCTLTable[i].node;
		if (strcmp(argv[2], "0") == 0) {
		    val->value = false;
		} else if (strcmp(argv[2], "1") == 0) {
		    val->value = true;
		} else {
		    kprintf("Invalid value!\n");
		}
		break;
	    }
	}
    }
}

REGISTER_DBGCMD(sysctl, "SYSCTL", Debug_SysCtl);

