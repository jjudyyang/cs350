
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <syscall.h>

#include <sys/sysctl.h>

typedef struct SysCtlEntry {
    char	path[64];
    int		type;
    int		flags;
    char	description[128];
} SysCtlEntry;

#define SYSCTL_STR(_PATH, _FLAGS, _DESCRIPTION, _DEFAULT) \
    { #_PATH, SYSCTL_TYPE_STR, _FLAGS, _DESCRIPTION },
#define SYSCTL_INT(_PATH, _FLAGS, _DESCRIPTION, _DEFAULT) \
    { #_PATH, SYSCTL_TYPE_INT, _FLAGS, _DESCRIPTION },
#define SYSCTL_BOOL(_PATH, _FLAGS, _DESCRIPTION, _DEFAULT) \
    { #_PATH, SYSCTL_TYPE_BOOL, _FLAGS, _DESCRIPTION },
SysCtlEntry SYSCTLTable[] = {
    SYSCTL_LIST
    { "", 0, 0, "" },
};
#undef SYSCTL_STR
#undef SYSCTL_INT
#undef SYSCTL_BOOL

void
PrintVal(int idx)
{
    switch (SYSCTLTable[idx].type) {
	case SYSCTL_TYPE_STR: {
	    SysCtlString scStr;

	    OSSysCtl(SYSCTLTable[idx].path, &scStr, NULL);
	    printf("%s: %s\n", SYSCTLTable[idx].path, scStr.value);
	    break;
	}
	case SYSCTL_TYPE_INT: {
	    SysCtlInt scInt;

	    OSSysCtl(SYSCTLTable[idx].path, &scInt, NULL);
	    printf("%s: %ld\n", SYSCTLTable[idx].path, scInt.value);
	    break;
	}
	case SYSCTL_TYPE_BOOL: {
	    SysCtlBool scBool;

	    OSSysCtl(SYSCTLTable[idx].path, &scBool, NULL);
	    printf("%s: %s\n", SYSCTLTable[idx].path,
		    scBool.value ? "true" : "false");
	    break;
	}
	default:
	    printf("%s: Unsupported type\n", SYSCTLTable[idx].path);
	    break;
    }
}

void
UpdateVal(int idx, const char *val)
{
    switch (SYSCTLTable[idx].type) {
	case SYSCTL_TYPE_STR: {
	    SysCtlString scStr;

	    strncpy(scStr.value, val, sizeof(scStr.value) - 1);
	    OSSysCtl(SYSCTLTable[idx].path, NULL, &scStr);
	    break;
	}
	case SYSCTL_TYPE_INT: {
	    SysCtlInt scInt;

	    scInt.value = atoi(val);
	    printf("%ld\n", scInt.value);
	    OSSysCtl(SYSCTLTable[idx].path, NULL, &scInt);
	    break;
	}
	case SYSCTL_TYPE_BOOL: {
	    SysCtlBool scBool;

	    if (strcmp(val,"true") == 0)
		scBool.value = true;
	    else if (strcmp(val,"false") == 0)
		scBool.value = false;
	    else {
		printf("Value must be true or false\n");
		exit(1);
	    }
	    OSSysCtl(SYSCTLTable[idx].path, NULL, &scBool);
	    break;
	}
	default:
	    printf("%s: Unsupported type\n", SYSCTLTable[idx].path);
	    break;
    }
}

int
main(int argc, const char *argv[])
{
    if (argc == 2 && strcmp(argv[1],"-h") == 0) {
	printf("Usage: sysctl [NODE] [VALUE]\n");
	return 1;
    }

    if (argc == 2 && strcmp(argv[1],"-d") == 0) {
        printf("%-20s %s\n", "Name", "Description");
	for (int i = 0; SYSCTLTable[i].type != 0; i++) {
	    printf("%-20s %s\n",
		    SYSCTLTable[i].path,
		    SYSCTLTable[i].description);
	}
	return 0;
    }

    if (argc == 2 || argc == 3) {
	for (int i = 0; SYSCTLTable[i].type != 0; i++) {
	    if (strcmp(SYSCTLTable[i].path, argv[1]) == 0) {
		if (argc == 2)
		    PrintVal(i);
		else
		    UpdateVal(i, argv[2]);
	    }
	}

	return 0;
    }

    for (int i = 0; SYSCTLTable[i].type != 0; i++) {
	PrintVal(i);
    }

    return 0;
}

