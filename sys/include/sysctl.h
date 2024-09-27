
#ifndef __SYS_SYSCTL_H__
#define __SYS_SYSCTL_H__

#include <stdbool.h>

/*
 * System Control Macros
 * SYSCTL_STR(PATH, FLAGS, DESCRIPTION, DEFAULT)
 * SYSCTL_INT(PATH, FLAGS, DESCRIPTION, DEFAULT)
 * SYSCTL_BOOL(PATH, FLAGS, DESCRIPTION, DEFAULT)
 * SYSCTL_END()
 */

#define SYSCTL_TYPE_INVALID	0
#define SYSCTL_TYPE_STR		1
#define SYSCTL_TYPE_INT		2
#define SYSCTL_TYPE_BOOL	3

#define SYSCTL_FLAG_RO		1
#define SYSCTL_FLAG_RW		2

#define SYSCTL_LIST \
    SYSCTL_STR(kern_ostype, SYSCTL_FLAG_RO, "OS Type", "Castor") \
    SYSCTL_INT(kern_hz, SYSCTL_FLAG_RW, "Tick frequency", 100) \
    SYSCTL_INT(time_tzadj, SYSCTL_FLAG_RW, "Time zone offset in seconds", 0) \
    SYSCTL_INT(log_syscall, SYSCTL_FLAG_RW, "Syscall log level", 1) \
    SYSCTL_INT(log_loader, SYSCTL_FLAG_RW, "Loader log level", 1) \
    SYSCTL_INT(log_vfs, SYSCTL_FLAG_RW, "VFS log level", 1) \
    SYSCTL_INT(log_o2fs, SYSCTL_FLAG_RW, "O2FS log level", 0) \
    SYSCTL_INT(log_ide, SYSCTL_FLAG_RW, "IDE log level", 0)

#define SYSCTL_STR_MAXLENGTH	128

typedef struct SysCtlString {
    char	path[64];
    char	value[SYSCTL_STR_MAXLENGTH];
} SysCtlString;

typedef struct SysCtlInt {
    char	path[64];
    int64_t	value;
} SysCtlInt;

typedef struct SysCtlBool {
    char	path[64];
    bool	value;
} SysCtlBool;

#define SYSCTL_STR(_PATH, _FLAGS, _DESCRIPTION, _DEFAULT) \
extern SysCtlString SYSCTL_##_PATH;
#define SYSCTL_INT(_PATH, _FLAGS, _DESCRIPTION, _DEFAULT) \
extern SysCtlInt SYSCTL_##_PATH;
#define SYSCTL_BOOL(_PATH, _FLAGS, _DESCRIPTION, _DEFAULT) \
extern SysCtlBool SYSCTL_##_PATH;
SYSCTL_LIST
#undef SYSCTL_STR
#undef SYSCTL_INT
#undef SYSCTL_BOOL

#define SYSCTL_GETSTR(_PATH) SYSCTL_##_PATH.value
#define SYSCTL_SETSTR(_PATH, _VALUE) strncpy(SYSCTL_##_PATH.value, _VALUE, SYSCTL_STR_MAXLENGTH);
#define SYSCTL_GETINT(_PATH) SYSCTL_##_PATH.value
#define SYSCTL_SETINT(_PATH, _VALUE) SYSCTL_##_PATH.value = _VALUE
#define SYSCTL_GETBOOL(_PATH) SYSCTL_##_PATH.value
#define SYSCTL_SETBOOL(_PATH, _VALUE) SYSCTL_##_PATH.value = _VALUE

uint64_t SysCtl_GetType(const char *node);
void *SysCtl_GetObject(const char *node);
uint64_t SysCtl_SetObject(const char *node, void *obj);

#endif /* __SYS_SYSCTL_H__ */

