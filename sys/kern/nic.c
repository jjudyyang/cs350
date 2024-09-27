/*
 * Copyright (c) 2013-2023 Ali Mashtizadeh
 * All rights reserved.
 */

#include <stdbool.h>
#include <stdint.h>

#include <sys/kassert.h>
#include <sys/kdebug.h>
#include <sys/queue.h>
#include <sys/mbuf.h>
#include <sys/nic.h>
#include <sys/spinlock.h>

LIST_HEAD(NICList, NIC) nicList = LIST_HEAD_INITIALIZER(nicList);
uint64_t nextNICNo = 0;

void
NIC_AddNIC(NIC *nic)
{
    nic->nicNo = nextNICNo++;
    LIST_INSERT_HEAD(&nicList, nic, entries);
}

void
NIC_RemoveNIC(NIC *nic)
{
    LIST_REMOVE(nic, entries);
}

NIC *
NIC_GetByID(uint64_t nicNo)
{
    NIC *n;

    LIST_FOREACH(n, &nicList, entries) {
	if (n->nicNo == nicNo)
	    return n;
    }

    return NULL;
}

void
Debug_NICs(int argc, const char *argv[])
{
    NIC *n;

    LIST_FOREACH(n, &nicList, entries) {
	kprintf("nic%lld: %02x:%02x:%02x:%02x:%02x:%02x\n", n->nicNo,
		n->mac[0], n->mac[1], n->mac[2], n->mac[3], n->mac[4], n->mac[5]);
    }
}

REGISTER_DBGCMD(nics, "List NICs", Debug_NICs);

