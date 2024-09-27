/*
 * Copyright (c) 2006-2023 Ali Mashtizadeh
 * All rights reserved.
 * Generic Copyin/Copyout routines
 */

#include <stdbool.h>
#include <stdint.h>

#include <errno.h>

#include <sys/kassert.h>
#include <machine/pmap.h>

extern int copy_unsafe(void *to_addr, void *from_addr, uintptr_t len);
extern int copystr_unsafe(void *to_addr, void *from_addr, uintptr_t len);

/**
 * Copy_In --
 *
 * Safely copy memory from userspace.  Prevents userspace pointers from
 * reading kernel memory.
 *
 * Side effects:
 * Kernel page fault may have occurred.
 *
 * @param [in] fromuser User address to copy from.
 * @param [in] tokernel Kernel address to copy to.
 * @param [in] len Length of the data to copy.
 *
 * @retval EFAULT if the address is invalid or causes a fault.
 */
int
Copy_In(uintptr_t fromuser, void *tokernel, uintptr_t len)
{
    if (len == 0)
	return 0;

    // Kernel space
    if (fromuser >= MEM_USERSPACE_TOP) {
	kprintf("Copy_In: address exceeds userspace top\n");
	return EFAULT;
    }

    // Wrap around
    if (len > (MEM_USERSPACE_TOP - fromuser)) {
	kprintf("Copy_In: length exceeds userspace top\n");
	return EFAULT;
    }

    return copy_unsafe(tokernel, (void *)fromuser, len);
}

/**
 * Copy_Out --
 *
 * Safely copy memory to userspace.  Prevents userspace pointers from
 * writing kernel memory.
 *
 * Side effects:
 * Kernel page fault may have occurred.
 *
 * @param [in] fromkernel Kernel address to copy from.
 * @param [in] touser User address to copy to.
 * @param [in] len Length of the data to copy.
 *
 * @retval EFAULT if the address is invalid or causes a fault.
 */
int
Copy_Out(void *fromkernel, uintptr_t touser, uintptr_t len)
{
    if (len == 0)
	return 0;

    // Kernel space
    if (touser >= MEM_USERSPACE_TOP) {
	kprintf("Copy_Out: address exceeds userspace top\n");
	return EFAULT;
    }

    // Wrap around
    if (len > (MEM_USERSPACE_TOP - touser)) {
	kprintf("Copy_Out: length exceeds userspace top\n");
	return EFAULT;
    }

    return copy_unsafe((void *)touser, fromkernel, len);
}

/**
 * Copy_StrIn --
 *
 * Safely copy a string from userspace.  Prevents userspace pointers from
 * reading kernel memory.
 *
 * Side effects:
 * Kernel page fault may have occurred.
 *
 * @param [in] fromuser User address to copy from.
 * @param [in] tokernel Kernel address to copy to.
 * @param [in] len Maximum string length.
 *
 * @retval EFAULT if the address is invalid or causes a fault.
 */
int
Copy_StrIn(uintptr_t fromuser, void *tokernel, uintptr_t len)
{
    if (len == 0)
	return 0;

    // Kernel space
    if (fromuser >= MEM_USERSPACE_TOP) {
	kprintf("Copy_StrIn: address exceeds userspace top\n");
	return EFAULT;
    }

    // Wrap around
    if (len > (MEM_USERSPACE_TOP - fromuser)) {
	kprintf("Copy_StrIn: length exceeds userspace top\n");
	return EFAULT;
    }

    return copystr_unsafe(tokernel, (void *)fromuser, len);
}

/**
 * Copy_StrOut --
 *
 * Safely copy a string to userspace.  Prevents userspace pointers from
 * writing kernel memory.
 *
 * Side effects:
 * Kernel page fault may have occurred.
 *
 * @param [in] fromkernel Kernel address to copy from.
 * @param [in] touser User address to copy to.
 * @param [in] len Maximum string length.
 *
 * @retval EFAULT if the address is invalid or causes a fault.
 */
int
Copy_StrOut(void *fromkernel, uintptr_t touser, uintptr_t len)
{
    if (len == 0)
	return 0;

    // Kernel space
    if (touser >= MEM_USERSPACE_TOP) {
	kprintf("Copy_StrOut: address exceeds userspace top\n");
	return EFAULT;
    }

    // Wrap around
    if (len > (MEM_USERSPACE_TOP - touser)) {
	kprintf("Copy_StrOut: length exceeds userspace top\n");
	return EFAULT;
    }

    return copystr_unsafe((void *)touser, fromkernel, len);
}

