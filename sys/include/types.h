
#ifndef __SYS_TYPES_H__
#define __SYS_TYPES_H__

typedef signed char         int8_t;
typedef signed short        int16_t;
typedef signed int          int32_t;
typedef signed long         int64_t;

typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long       uint64_t;

typedef int64_t intptr_t;
typedef uint64_t uintptr_t;
typedef uint64_t size_t;
typedef int64_t ssize_t;

typedef int64_t off_t;
typedef int64_t fpos_t;

typedef uint64_t ino_t;

typedef uint64_t time_t;
typedef uint64_t suseconds_t;

typedef uint16_t pid_t;

#define NULL ((void *)0)

#endif /* __SYS_TYPES_H__ */

