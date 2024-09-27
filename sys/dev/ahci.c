
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <sys/kassert.h>
#include <sys/kdebug.h>
#include <sys/kmem.h>
#include <sys/pci.h>
#include <sys/sga.h>

#include "ata.h"
#include "sata.h"

/*
 * AHCI Definitions
 */

typedef struct AHCIDevice
{
    uint32_t device;
    const char *name;
    uint32_t flags;
} AHCIDevice;

static AHCIDevice deviceList[] =
{
    { 0x80862922, "ICH9", 0 },
    { 0, "", 0 },
};

typedef struct AHCIHostControl
{
    uint32_t	cap;		// Host Capabilities
    uint32_t	ghc;		// Global Host Control
    uint32_t	is;		// Interrupt Status
    uint32_t	pi;		// Ports Implemented
    uint32_t	vs;		// Version
} AHCIHostControl;

#define AHCI_CAP_S64A		0x80000000  /* Supports 64-bit Addressing */
#define AHCI_CAP_SNCQ		0x40000000  /* Supports NCQ */

#define AHCI_GHC_AE		0x80000000
#define AHCI_GHC_IE		0x00000002
#define AHCI_GHC_HR		0x00000001

typedef struct AHCIPort
{
    uint64_t	clba;		// Command List Base Address
    uint64_t	fb;		// FIS Base Address
    uint32_t	is;		// Interrupt Status
    uint32_t	ie;		// Interrupt Enable
    uint32_t	cmd;		// Command
    uint32_t	_rsvd;		// *Reserved*
    uint32_t	tfd;		// Task File Data
    uint32_t	sig;		// Signature
    uint32_t	ssts;		// SATA Status
    uint32_t	sctl;		// SATA Control
    uint32_t	serr;		// SATA Error
    uint32_t	sact;		// SATA Active
    uint32_t	ci;		// Command Issue
    uint32_t	_rsvd2;		// *Reserved*
} AHCIPort;

#define AHCIPORT_CMD_ICCMASK	0xF0000000 /* Interface Communication Control */
#define AHCIPORT_CMD_ICCSLUMBER	0x60000000 /* ICC Slumber */
#define AHCIPORT_CMD_ICCPARTIAL	0x20000000 /* ICC Partial */
#define AHCIPORT_CMD_ICCACTIVE	0x10000000 /* ICC Active */
#define AHCIPORT_CMD_ICCIDLE	0x00000000 /* ICC Idle */
#define AHCIPORT_CMD_ASP	0x08000000 /* Aggressive Slumber/Partial */
#define AHCIPORT_CMD_ALPE	0x04000000 /* Aggressive Link PM Enable */
#define AHCIPORT_CMD_DLAE	0x02000000 /* Drive LED on ATAPI Enable */
#define AHCIPORT_CMD_ATAPI	0x01000000 /* Device is ATAPI */
#define AHCIPORT_CMD_CPD	0x00100000 /* Cold Presence Detection */
#define AHCIPORT_CMD_ISP	0x00080000 /* Interlock Switch Attached */
#define AHCIPORT_CMD_HPCP	0x00040000 /* Hot Plug Capable Port */
#define AHCIPORT_CMD_PMA	0x00020000 /* Port Multiplier Attached */
#define AHCIPORT_CMD_CPS	0x00010000 /* Cold Presence State */
#define AHCIPORT_CMD_CR		0x00008000 /* Command List Running */
#define AHCIPORT_CMD_FR		0x00004000 /* FIS Receive Running */
#define AHCIPORT_CMD_ISS	0x00002000 /* Interlock Switch State */
#define AHCIPORT_CMD_FRE	0x00000010 /* FIS Receive Enable */
#define AHCIPORT_CMD_CLO	0x00000008 /* Command List Override */
#define AHCIPORT_CMD_POD	0x00000004 /* Power On Device */
#define AHCIPORT_CMD_SUD	0x00000002 /* Spin-Up Device */
#define AHCIPORT_CMD_ST		0x00000001 /* Start */

#define AHCIPORT_TFD_BSY	0x00000080 /* Port Busy */
#define AHCIPORT_TFD_DRQ	0x00000004 /* Data Transfer Requested */
#define AHCIPORT_TFD_ERR	0x00000001 /* Error during Transfer */

#define AHCIPORT_SSTS_DETMASK	0x0000000F /* Device Detection (DET) Mask */
#define AHCIPORT_SSTS_DETNP	0x00000000 /* DET: Not Present */
#define AHCIPORT_SSTS_DETNOTEST	0x00000001 /* DET: Phy not established */
#define AHCIPORT_SSTS_DETPE	0x00000003 /* DET: Present and Established */
#define AHCIPORT_SSTS_DETNE	0x00000004 /* DET: Not Enabled or in BIST mode */

#define AHCI_ABAR		5
#define AHCI_PORT_OFFSET	0x100
#define AHCI_PORT_LENGTH	0x80
#define AHCI_MAX_PORTS		8
#define AHCI_MAX_CMDS		32

/*
 * Request Structures
 */

typedef struct AHCICommandHeader
{
    uint16_t		flag;
    uint16_t		prdtl;		// PRDT Length
    uint32_t		cmdStatus;
    uint64_t		ctba;
    uint64_t		_rsvd[2];
} AHCICommandHeader;

typedef struct AHCICommandList
{
    AHCICommandHeader	cmds[AHCI_MAX_CMDS];
} AHCICommandList;

/*
 * AHCIPRDT - Physical Region Descriptor Table
 */
typedef struct AHCIPRDT
{
    uint64_t		dba;		// Data Base Address
    uint32_t		_rsvd;
    uint32_t		descInfo;	// Description Information
} AHCIPRDT;

/*
 * AHCICommandTable
 * 	This structure is exactly one machine page in size, we fill up the page 
 * 	with PRDT entries.  AHCI supports up to 64K PRDT entries but on x86 we 
 * 	limit this to 248.
 */
typedef struct AHCICommandTable
{
    uint8_t		cfis[64];	// Command FIS
    uint8_t		acmd[32];	// ATAPI Command
    uint8_t		_rsvd[32];
    AHCIPRDT		prdt[248];	// Physical Region Descriptor Table
} AHCICommandTable;

/*
 * Response Structures
 */

typedef struct AHCIRecvFIS
{
    uint8_t		dsfis[0x1c];
    uint8_t		_rsvd0[0x4];
    uint8_t		psfis[0x14];
    uint8_t		_rsvd1[0xc];
    uint8_t		rfis[0x14];
    uint8_t		_rsvd2[0x4];
    uint8_t		sdbfis[0x8];
    uint8_t		ufis[0x40];
    uint8_t		_rsvd3[0x60];
} AHCIRecvFIS;

/*
 * AHCI
 * 	Exceeds a single page need to use heap
 */
typedef struct AHCI
{
    PCIDevice		dev;
    // Device Space
    AHCIHostControl	*hc;
    AHCIPort		*port[AHCI_MAX_PORTS];
    // Driver Memory
    AHCICommandList	*clst[AHCI_MAX_PORTS];
    AHCICommandTable	*ctbl[AHCI_MAX_PORTS][AHCI_MAX_CMDS];
    AHCIRecvFIS		*rfis[AHCI_MAX_PORTS];
} AHCI;

void AHCI_Configure(PCIDevice dev);

void
AHCI_Init(uint32_t bus, uint32_t slot, uint32_t func)
{
    PCIDevice dev;

    dev.bus = bus;
    dev.slot = slot;
    dev.func = func;
    dev.vendor = PCI_GetVendorID(&dev);
    dev.device = PCI_GetDeviceID(&dev);

    uint32_t device = dev.vendor << 16 | dev.device;

    uint8_t progif = PCI_CfgRead8(&dev, PCI_OFFSET_PROGIF);
    if (progif != 0x01)
    {
	kprintf("Unsupported SATA Controller PROGIF=%02x\n", progif);
	return;
    }

    // XXX: Temporary until we have a slab allocator
#define PGSIZE	4096
    ASSERT(sizeof(AHCI) <= PGSIZE);
    ASSERT(sizeof(AHCICommandList) <= PGSIZE);
    ASSERT(sizeof(AHCIRecvFIS) <= PGSIZE);
    ASSERT(sizeof(ATAIdentifyDevice) == 512);

    int deviceIdx = 0;
    while (deviceList[deviceIdx].device != 0x0) {
	if (deviceList[deviceIdx].device == device) {
	    kprintf("AHCI: Found %s\n", deviceList[deviceIdx].name);
	    // Configure and add disks
	    AHCI_Configure(dev);
	}

	deviceIdx++;
    }
}

void
AHCI_Dump(AHCI *ahci)
{
    volatile AHCIHostControl *hc = ahci->hc;

    kprintf("CAP: 0x%08x\n", hc->cap);
    kprintf("GHC: 0x%08x\n", hc->ghc);
    kprintf("IS: 0x%08x\n", hc->is);
    kprintf("PI: 0x%08x\n", hc->pi);
    kprintf("VS: 0x%08x\n", hc->vs);
}

void
AHCI_DumpPort(AHCI *ahci, int port)
{
    volatile AHCIPort *p = ahci->port[port];

    kprintf("Port %d\n", port);
    kprintf("CLBA: 0x%016llx\n", p->clba);
    kprintf("FB: 0x%016llx\n", p->fb);
    kprintf("IS: 0x%08x\n", p->is);
    kprintf("IE: 0x%08x\n", p->ie);
    kprintf("CMD: 0x%08x\n", p->cmd);
    kprintf("TFD: 0x%08x\n", p->tfd);
    kprintf("SIG: 0x%08x\n", p->sig);
    kprintf("SSTS: 0x%08x\n", p->ssts);
    kprintf("SCTL: 0x%08x\n", p->sctl);
    kprintf("SERR: 0x%08x\n", p->serr);
    kprintf("SACT: 0x%08x\n", p->sact);
    kprintf("CI: 0x%08x\n", p->ci);
}

uint64_t
AHCI_IssueCommand(AHCI *ahci, int port, SGArray *sga, void *cfis, int len)
{
    // XXX: support multiple commands
    volatile AHCICommandList *cl = ahci->clst[port];
    volatile AHCICommandTable *ct = ahci->ctbl[port][0];
    volatile AHCIPort *p = ahci->port[port];

    // Copy Command FIS
    memcpy((void *)&ct->cfis[0], cfis, len);

    // Convert SGArray into PRDT
    int i;
    for (i = 0; i < sga->len; i++)
    {
	ct->prdt[i].dba = sga->entries[i].offset;
	ct->prdt[i].descInfo = sga->entries[i].length - 1;
	// Must be a multiple of word size
	ASSERT(sga->entries[i].length % 2 == 0);
    }

    // Specify cfis length and prdt entries;
    // XXX: support multiple commands
    cl->cmds[0].prdtl = sga->len;
    cl->cmds[0].flag = len >> 2;

    p->ci = 1;

    return 0;
}

void
AHCI_WaitPort(AHCI *ahci, int port)
{
    volatile AHCIPort *p = ahci->port[port];
    while (1) {
	uint32_t tfd = p->tfd & AHCIPORT_TFD_BSY;
	if ((tfd == 0) && (p->ci == 0)) {
	    return;
	}

	// implement timeouts
    }
}

void
AHCI_IdentifyPort(AHCI *ahci, int port)
{
    volatile AHCIPort *p = ahci->port[port];
    SGArray sga;
    SATAFIS_REG_H2D fis;
    ATAIdentifyDevice ident;

    kprintf("AHCI: Signature %08x\n", p->sig);

    memset(&fis, 0, sizeof(fis));
    fis.type = SATAFIS_TYPE_REG_H2D;
    fis.flag = SATAFIS_REG_H2D_FLAG_COMMAND;
    fis.command = SATAFIS_CMD_IDENTIFY;

    sga.len = 1;
    // VA2PA
    sga.entries[0].offset = (uintptr_t)&ident;
    sga.entries[0].length = 512;

    AHCI_IssueCommand(ahci, port, &sga, &fis, sizeof(fis));
    kprintf("AHCI: Identify Issued Port %d\n", port);
    AHCI_WaitPort(ahci, port);

    kprintf("AHCI: Identify Succeeded Port %d\n", port);
    Debug_PrintHex((const char *)&ident, 512, 0, 512);
    AHCI_DumpPort(ahci, port);
    uint64_t val = (uint64_t)&ident;
    Debug_PrintHex((const char *)&val, 8, 0, 8);

    return;
}

void
AHCI_ResetPort(AHCI *ahci, int port)
{
    volatile AHCIPort *p = ahci->port[port];
    uint32_t cmd = p->cmd;
    uint32_t cmd_mask = AHCIPORT_CMD_ST | AHCIPORT_CMD_CR |
			AHCIPORT_CMD_FRE | AHCIPORT_CMD_FR;

    // Wait for controller to be idle
    if ((cmd & cmd_mask) != 0) {
	int tries;
	for (tries = 0; tries < 2; tries++) {
	    cmd = cmd & ~(AHCIPORT_CMD_ST | AHCIPORT_CMD_FRE);
	    p->cmd = cmd;
	    // sleep 500ms
	    cmd = p->cmd;
	    if ((cmd & cmd_mask) != 0) {
		kprintf("AHCI: failed to reset port %d\n", port);
	    }
	}
    }

    // Reset interrupts
    p->is = 0xFFFFFFFF;
    p->is = 0x00000000;

    // Reset error
    p->serr = 0xFFFFFFFF;
    p->serr = 0x00000000;

    p->cmd |= AHCIPORT_CMD_FRE | AHCIPORT_CMD_ST | AHCIPORT_CMD_SUD |
	      AHCIPORT_CMD_POD | AHCIPORT_CMD_ICCACTIVE;

    // Check port
    uint32_t ssts = p->ssts;
    if ((ssts & AHCIPORT_SSTS_DETMASK) == AHCIPORT_SSTS_DETNP) {
	kprintf("AHCI: Device not present on port %d\n", port);
	return;
    }
    if ((ssts & AHCIPORT_SSTS_DETMASK) == AHCIPORT_SSTS_DETNOTEST) {
	kprintf("AHCI: Phys communication not established on port %d\n", port);
	return;
    }
    if ((ssts & AHCIPORT_SSTS_DETNE) == AHCIPORT_SSTS_DETNOTEST) {
	kprintf("AHCI: Port %d not enabled\n", port);
	return;
    }

    AHCI_IdentifyPort(ahci, port);
}

void
AHCI_Reset(AHCI *ahci)
{
    int port;
    volatile AHCIHostControl *hc = ahci->hc;

    AHCI_Dump(ahci);

    // Reset controller
    //uint32_t caps = hc->cap;
    //uint32_t pi = hc->pi;

    hc->ghc |= AHCI_GHC_AE;
    hc->ghc |= AHCI_GHC_HR;
    while (1) {
	if ((hc->ghc & AHCI_GHC_HR) == 0)
	    break;
    }
    hc->ghc |= AHCI_GHC_AE;

    AHCI_Dump(ahci);

    // Reset ports
    for (port = 0; port < AHCI_MAX_PORTS; port++)
    {
	if (ahci->port[port] != 0) {
	    AHCI_ResetPort(ahci, port);
	}
    }

    // XXX: Clear interrupts

    // Enable Interrupts
    hc->ghc = AHCI_GHC_IE;
}

void
AHCI_Configure(PCIDevice dev)
{
    AHCI *ahci = (AHCI *)PAlloc_AllocPage();
    volatile AHCIHostControl *hc;

    PCI_Configure(&dev);

    kprintf("AHCI: IRQ %d\n", dev.irq);

    // MMIO
    int bar;
    for (bar = 0; bar < PCI_MAX_BARS; bar++) {
	if (dev.bars[bar].size == 0)
	    continue;

	kprintf("AHCI: BAR%d base=%08x size=%08x %s\n",
		bar, dev.bars[bar].base, dev.bars[bar].size,
		dev.bars[bar].type == PCIBAR_TYPE_IO ? "IO" : "Mem");
    }

    // Copy PCIDevice structure
    memcpy(&ahci->dev, &dev, sizeof(dev));

    // XXX: Register IRQ

    // Setup
    hc = (volatile AHCIHostControl *)(uintptr_t)dev.bars[AHCI_ABAR].base;
    ahci->hc = (AHCIHostControl *)hc;

    uint32_t caps = hc->cap;
    uint32_t ports = hc->pi;
    uint32_t vers = hc->vs;

    kprintf("AHCI: Version %d.%d, Ports: 0x%08x\n",
	    vers >> 16, vers & 0xFFFF, ports);
    if (ports > ((1 << AHCI_MAX_PORTS) - 1)) 
    {
	kprintf("AHCI: Currently only supports %d ports\n", AHCI_MAX_PORTS);
    }

    if (caps & AHCI_CAP_S64A) {
	kprintf("AHCI: Supports 64-bit Addressing\n");
    } else {
	kprintf("AHCI: Controller does not support 64-bit addressing!\n");
	return;
    }

    if (caps & AHCI_CAP_SNCQ)
	kprintf("AHCI: Supports NCQ\n");

    // Disable Interrupts
    hc->ghc &= ~AHCI_GHC_IE;

    // Enable AHCI Controller
    hc->ghc |= AHCI_GHC_AE;

    int p;
    for (p = 0; p < AHCI_MAX_PORTS; p++)
    {
	if (ports & (1 << p))
	{
	    ahci->port[p] = (AHCIPort *)(uintptr_t)(dev.bars[AHCI_ABAR].base +
				AHCI_PORT_OFFSET + AHCI_PORT_LENGTH * p);
	} else {
	    ahci->port[p] = 0;
	}
    }

    // Allocate memory and setup pointers
    for (p = 0; p < AHCI_MAX_PORTS; p++)
    {
	volatile AHCIPort *port = ahci->port[p];
	if (port != 0) {
	    int c;
	    for (c = 0; c < AHCI_MAX_CMDS; c++)
	    {
		ahci->ctbl[p][c] = (AHCICommandTable *)PAlloc_AllocPage();
		memset(ahci->ctbl[p][c], 0, sizeof(AHCICommandTable));
		// XXX: VA2PA
		ahci->clst[p]->cmds[c].ctba = (uint64_t)ahci->ctbl[p][c];
	    }

	    ahci->clst[p] = (AHCICommandList *)PAlloc_AllocPage();
	    memset(ahci->clst[p], 0, sizeof(AHCICommandList));
	    // XXX: VA2PA
	    port->clba = (uint64_t)ahci->clst[p];

	    ahci->rfis[p] = (AHCIRecvFIS *)PAlloc_AllocPage();
	    memset(ahci->rfis[p], 0, sizeof(AHCIRecvFIS));
	    // XXX: VA2PA
	    port->fb = (uint64_t)ahci->rfis[p];
	}
    }

    // Reset controller and ports
    AHCI_Reset(ahci);
}

