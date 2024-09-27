
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <sys/kassert.h>
#include <sys/kdebug.h>
#include <sys/kmem.h>
#include <sys/semaphore.h>
#include <sys/pci.h>
#include <sys/irq.h>
#include <sys/mbuf.h>
#include <sys/nic.h>

#include <machine/pmap.h>

typedef struct E1000Device
{
    uint32_t device;
    const char *name;
    uint32_t flags;
} E1000Device;

static E1000Device deviceList[] =
{
    { 0x8086100e, "E1000", 0 }, // EERD is not supported
    // { 0x80861209, "i82551", 0 }, // Doesn't seem to work on my qemu build
    { 0, "", 0 },
};

void E1000_Configure(PCIDevice dev);

/*
 * Hardware Structures
 */

#define E1000_REG_CTRL		0x0000
#define E1000_REG_STATUS	0x0008
#define E1000_REG_EECD		0x0010
#define E1000_REG_EERD		0x0014
#define E1000_REG_ICR		0x00C0
#define E1000_REG_ICS		0x00C8
#define E1000_REG_IMS		0x00D0
#define E1000_REG_IMC		0x00D8
#define E1000_REG_RCTL		0x0100
#define E1000_REG_TCTL		0x0400

#define E1000_REG_RDBAL		0x2800
#define E1000_REG_RDBAH		0x2804
#define E1000_REG_RDLEN		0x2808
#define E1000_REG_RDH		0x2810
#define E1000_REG_RDT		0x2818
#define E1000_REG_RDTR		0x2820

#define E1000_REG_TDBAL		0x3800
#define E1000_REG_TDBAH		0x3804
#define E1000_REG_TDLEN		0x3808
#define E1000_REG_TDH		0x3810
#define E1000_REG_TDT		0x3818
#define E1000_REG_TIDV		0x3820
#define E1000_REG_TADV		0x382C

#define E1000_REG_MTABASE	0x5200

// EEPROM Offsets
#define NVM_MAC_ADDR		0x0000
#define NVM_DEVICE_ID		0x000D
#define NVM_VENDOR_ID		0x000E

// Control Flags
#define CTRL_SLU		(1 << 6)

// Recieve Control Flags
#define RCTL_EN			(1 << 1)
#define RCTL_SBP		(1 << 2)
#define RCTL_UPE		(1 << 3)
#define RCTL_MPE		(1 << 4)
#define RCTL_LPE		(1 << 5)
#define RCTL_BSIZE_1K		(0x1 << 16)
#define RCTL_BSIZE_2K		(0x0 << 16)
#define RCTL_BSIZE_4K		((1 << 25) | (0x3 << 16))
#define RCTL_BSIZE_8K		((1 << 25) | (0x2 << 16))
#define RCTL_BSIZE_16K		((1 << 25) | (0x1 << 16))

// Transmit Control Flags
#define TCTL_EN			(1 << 1)
#define TCTL_PSP		(1 << 3)

// Interrupt Control Register
#define ICR_TXDW		(1 << 0)
#define ICR_LSC			(1 << 2)
#define ICR_RXO			(1 << 6)
#define ICR_RXT0		(1 << 7)

typedef struct PACKED E1000RXDesc {
    volatile uint64_t	addr;		// Address
    volatile uint16_t	len;		// Length
    volatile uint16_t	csum;		// Checksum
    volatile uint8_t	status;		// Status
    volatile uint8_t	errors;		// Errors
    volatile uint16_t	special;
} E1000RXDesc;

#define RDESC_STATUS_EOP	(1 << 1)
#define RDESC_STATUS_DD		(1 << 0)

#define RDESC_ERROR_RDE		(1 << 7) /* Receive Data Error */
#define RDESC_ERROR_CE		(1 << 0) /* CRC Error */

typedef struct PACKED E1000TXDesc {
    volatile uint64_t	addr;		// Address
    volatile uint16_t	len;		// Length
    volatile uint8_t	cso;		// Checksum Offset
    volatile uint8_t	cmd;		// Command
    volatile uint8_t	status;		// Status
    volatile uint8_t	css;		// Checksum start
    volatile uint16_t	special;
} E1000TXDesc;

#define TDESC_CMD_RS		(1 << 3) /* Report Status */
#define TDESC_CMD_IC		(1 << 2) /* Insert Checksum */
#define TDESC_CMD_IFCS		(1 << 1) /* Insert Ethernet FCS/CRC */
#define TDESC_CMD_EOP		(1 << 0) /* End-of-Packet */

/*
 * Driver Configuration and Structures
 */

typedef struct E1000Dev {
    NIC			nic;		// Must be first
    PCIDevice		dev;
    IRQHandler		irqHandle;
    uint8_t		*mmiobase;
    E1000RXDesc		*rxDesc;
    E1000TXDesc		*txDesc;
    uint32_t		rxTail;
    uint32_t		txTail;
    Spinlock		lock;
    Semaphore		ioSema;
} E1000Dev;

#define E1000_TX_QLEN		128
#define E1000_RX_QLEN		128

#define E1000_MAX_MTU		9000

// Memory Resources
static bool runOnce = false;
static Slab rxPool;
static Slab rxDescPool;
static Slab txDescPool;
DEFINE_SLAB(E1000RXDesc, &rxDescPool);
DEFINE_SLAB(E1000TXDesc, &txDescPool);

void *
RXPOOL_Alloc()
{
    return Slab_Alloc(&rxPool);
}

void
RXPOOL_Free(void *buf)
{
    Slab_Free(&rxPool, buf);
}

void
E1000_Init(uint32_t bus, uint32_t slot, uint32_t func)
{
    PCIDevice dev;

    dev.bus = bus;
    dev.slot = slot;
    dev.func = func;
    dev.vendor = PCI_GetVendorID(&dev);
    dev.device = PCI_GetDeviceID(&dev);

    uint32_t device = dev.vendor << 16 | dev.device;

    int deviceIdx = 0;
    while (deviceList[deviceIdx].device != 0x0) {
	if (deviceList[deviceIdx].device == device) {
	    kprintf("E1000: Found %s\n", deviceList[deviceIdx].name);
	    // Configure and add disks
	    E1000_Configure(dev);
	}

	deviceIdx++;
    }
}

static inline uint32_t
MMIO_Read32(E1000Dev *dev, uint64_t addr)
{
    return *(uint32_t volatile *)(dev->mmiobase + addr);
}

static inline void
MMIO_Write32(E1000Dev *dev, uint64_t addr, uint32_t val)
{
    *(uint32_t *)(dev->mmiobase + addr) = val;
}

static uint16_t
E1000_EEPROM_Read(E1000Dev *dev, uint8_t addr)
{
    uint16_t val;

    // Write Address
    MMIO_Write32(dev, E1000_REG_EERD, ((uint32_t)addr << 8) | 1);

    // Wait for ready bit
    while (1) {
	val = MMIO_Read32(dev, E1000_REG_EERD);
	if (val & 0x10)
	    break;
    }

    kprintf("%08x\n", val);

    return (uint16_t)((val >> 16) & 0x0000FFFF);
}

void
E1000_TXPoll(E1000Dev *dev)
{
    // Free memory
    kprintf("TXPOLL\n");
}

void
E1000_RXPoll(E1000Dev *dev)
{
    while (dev->rxDesc[dev->rxTail].status & RDESC_STATUS_DD) {
	//void *data = (void *)DMPA2VA(dev->rxDesc[dev->rxTail].addr);
	//uint16_t len = dev->rxDesc[dev->rxTail].len;

	// Look for packet fragments up to EOP set in status
	if ((dev->rxDesc[dev->rxTail].status & RDESC_STATUS_EOP) &&
	    (dev->rxDesc[dev->rxTail].errors == 0)) {
	    // Process packet
	    /*
	     * XXX: Need to create a queue and copyout packets here, because 
	     * I'm seeing multiple interrupts.
	     *
	     * kprintf("E1000: Int\n");
	     */
	    Semaphore_Release(&dev->ioSema);
	}

	if (dev->rxDesc[dev->rxTail].errors) {
	    kprintf("E1000: Error in RX Queue %x\n",
		    dev->rxDesc[dev->rxTail].errors);
	    dev->rxDesc[dev->rxTail].status = 0;
	    dev->rxDesc[dev->rxTail].errors = 0;
	    MMIO_Write32(dev, E1000_REG_RDT, dev->rxTail);
	    dev->rxTail = (dev->rxTail + 1) % E1000_RX_QLEN;
	}

	/*
	dev->rxDesc[dev->rxTail].status = 0;
	dev->rxDesc[dev->rxTail].errors = 0;
	// XXX: Should write this only once
	MMIO_Write32(dev, E1000_REG_RDT, dev->rxTail);
	dev->rxTail = (dev->rxTail + 1) % E1000_RX_QLEN;
	*/
    }
}

void
E1000_Interrupt(void *arg)
{
    E1000Dev *dev = (E1000Dev *)arg;

    kprintf("E1000 (%d:%d) Interrupt\n",
	    dev->dev.bus, dev->dev.slot);

    uint32_t cause = MMIO_Read32(dev, E1000_REG_ICR);

    // Link Status
    if (cause & ICR_LSC) {
	cause &= ~ICR_LSC;
	MMIO_Write32(dev, E1000_REG_CTRL, MMIO_Read32(dev, E1000_REG_CTRL) | CTRL_SLU);
    }

    // Transmit Queue Empty
    if (cause & 3) {
	cause &= ~3;
	E1000_TXPoll(dev);
    }

    // Receive Overrun
    if (cause & ICR_RXO) {
	cause &= ~ICR_RXO;
	kprintf("underrun %u %u\n", MMIO_Read32(dev, E1000_REG_RDH), dev->rxTail);

	E1000_RXPoll(dev);
    }

    // Receive Timer
    if (cause & ICR_RXT0) {
	cause &= ~ICR_RXT0;
	E1000_RXPoll(dev);
    }

    if (cause != 0) {
	kprintf("E1000: Unhandled cause %08x\n", cause);
    }

    MMIO_Read32(dev, E1000_REG_ICR);
}

/*
 * Read packets from NIC
 */
int
E1000_Dequeue(NIC *nic, MBuf *mbuf, NICCB cb, void *arg)
{
    E1000Dev *dev = (E1000Dev *)nic;

retry:
    Semaphore_Acquire(&dev->ioSema);

    Spinlock_Lock(&dev->lock);
    if ((dev->rxDesc[dev->rxTail].status & RDESC_STATUS_EOP) &&
        (dev->rxDesc[dev->rxTail].errors == 0)) {
	void *data = (void *)DMPA2VA(dev->rxDesc[dev->rxTail].addr);
	uint16_t len = dev->rxDesc[dev->rxTail].len;
    
	//Debug_PrintHex(data, len, 0, len);
	// Use CB instead
	memcpy((void *)mbuf->vaddr, data, len);

	dev->rxDesc[dev->rxTail].status = 0;
	dev->rxDesc[dev->rxTail].errors = 0;
	// XXX: Should write this only once
	MMIO_Write32(dev, E1000_REG_RDT, dev->rxTail);
	dev->rxTail = (dev->rxTail + 1) % E1000_RX_QLEN;
    } else {
	/*
	 * XXX: Need to remove this once I do the copyout inside the interrupt 
	 * handler.
	 */
	/*kprintf("Packet not ready %d %x %x\n",
		dev->rxTail,
		dev->rxDesc[dev->rxTail].status,
		dev->rxDesc[dev->rxTail].errors);*/
	Spinlock_Unlock(&dev->lock);
	goto retry;
    }
    Spinlock_Unlock(&dev->lock);

    return 0;
}

/*
 * Transmit packets
 */
int
E1000_Enqueue(NIC *nic, MBuf *mbuf, NICCB cb, void *arg)
{
    E1000Dev *dev = (E1000Dev *)nic;
    void *data = (void *)DMPA2VA(dev->txDesc[dev->txTail].addr);

    // Copy data
    memcpy(data, (void *)mbuf->vaddr, mbuf->len);
    dev->txDesc[dev->txTail].len = mbuf->len;

    dev->txDesc[dev->txTail].cmd = TDESC_CMD_EOP | TDESC_CMD_IFCS | TDESC_CMD_RS;
    MMIO_Write32(dev, E1000_REG_TDT, dev->txTail);
    dev->txTail = (dev->txTail + 1) % E1000_TX_QLEN;

    // Wait for transmit to complete

    return 0;
}

void
E1000_RXInit(E1000Dev *dev)
{
    int i;

    // Zero out Multicast Table Array
    for (i = 0; i < 128; i++) {
	MMIO_Write32(dev, E1000_REG_MTABASE + (i * 4), 0);
    }

    dev->rxDesc = (E1000RXDesc *)E1000RXDesc_Alloc();
    for (i = 0; i < E1000_RX_QLEN; i++) {
	dev->rxDesc[i].addr = VA2PA((uintptr_t)PAlloc_AllocPage()); // LOOKUP IN PMAP
	dev->rxDesc[i].status = 0;
    }

    // Setup base and length of recieve descriptors
    uintptr_t base = VA2PA((uintptr_t)dev->rxDesc);
    MMIO_Write32(dev, E1000_REG_RDBAH, (uint32_t)(base >> 32));
    MMIO_Write32(dev, E1000_REG_RDBAL, (uint32_t)(base & 0xFFFFFFFF));
    MMIO_Write32(dev, E1000_REG_RDLEN, E1000_RX_QLEN * 16);

    MMIO_Write32(dev, E1000_REG_RDH, 0);
    MMIO_Write32(dev, E1000_REG_RDT, E1000_RX_QLEN);
    dev->rxTail = 0;

    MMIO_Write32(dev, E1000_REG_RCTL,
	    (RCTL_EN | RCTL_UPE | RCTL_MPE | RCTL_BSIZE_4K));
}

void
E1000_TXInit(E1000Dev *dev)
{
    int i;

    dev->txDesc = (E1000TXDesc *)E1000RXDesc_Alloc();
    for (i = 0; i < E1000_TX_QLEN; i++) {
	dev->txDesc[i].addr = VA2PA((uintptr_t)PAlloc_AllocPage()); // LOOKUP IN PMAP
	dev->txDesc[i].cmd = 0;
    }

    // Setup base and length of recieve descriptors
    uintptr_t base = VA2PA((uintptr_t)dev->txDesc);
    MMIO_Write32(dev, E1000_REG_TDBAH, (uint32_t)(base >> 32));
    MMIO_Write32(dev, E1000_REG_TDBAL, (uint32_t)(base & 0xFFFFFFFF));
    MMIO_Write32(dev, E1000_REG_TDLEN, E1000_TX_QLEN * 16);

    MMIO_Write32(dev, E1000_REG_TDH, 0);
    MMIO_Write32(dev, E1000_REG_TDT, 0);
    dev->txTail = 0;

    // Interrupt Delay Timers
    MMIO_Write32(dev, E1000_REG_TIDV, 1);
    MMIO_Write32(dev, E1000_REG_TADV, 1);

    MMIO_Write32(dev, E1000_REG_TCTL, TCTL_EN | TCTL_PSP);
}

void
E1000_Configure(PCIDevice dev)
{
    E1000Dev *ethDev = (E1000Dev *)PAlloc_AllocPage();
    PCI_Configure(&dev);

    // Ensure that the NIC structure is the first thing inside E1000Dev
    ASSERT((void *)ethDev == (void *)&ethDev->nic);

    if (!runOnce) {
	runOnce = true;

	//E1000_MAX_MTU + 14 + 4, 4096);
	Slab_Init(&rxPool, "E1000 RX Pool", 4096, 4096);
	Slab_Init(&rxDescPool, "E1000 RX Descriptors", E1000_RX_QLEN*sizeof(E1000RXDesc), 16);
	Slab_Init(&txDescPool, "E1000 TX Descriptors", E1000_TX_QLEN*sizeof(E1000TXDesc), 16);
    }

    // Copy PCIDevice structure
    memcpy(&ethDev->dev, &dev, sizeof(dev));

    // MMIO
    int bar;
    for (bar = 0; bar < PCI_MAX_BARS; bar++) {
	if (dev.bars[bar].size == 0)
	    continue;

	kprintf("E1000: BAR%d base=%08x size=%08x %s\n",
		bar, dev.bars[bar].base, dev.bars[bar].size,
		dev.bars[bar].type == PCIBAR_TYPE_IO ? "IO" : "Mem");
    }

    ethDev->mmiobase = (uint8_t *)DMPA2VA(dev.bars[0].base);

    // Enable Link
    MMIO_Write32(ethDev, E1000_REG_CTRL, MMIO_Read32(ethDev, E1000_REG_CTRL) | CTRL_SLU);

    // Register IRQs
    kprintf("E1000: IRQ %d\n", dev.irq);
    ethDev->irqHandle.irq = dev.irq;
    ethDev->irqHandle.cb = &E1000_Interrupt;
    ethDev->irqHandle.arg = ethDev;
    IRQ_Register(dev.irq, &ethDev->irqHandle);

    kprintf("E1000: MAC XX:XX:XX:XX:XX:XX\n");
    for (int i = 0; i < 3; i++) {
	E1000_EEPROM_Read(ethDev, NVM_MAC_ADDR + 2*i);
    }

    // Device lock
    Spinlock_Init(&ethDev->lock, "E1000 Spinlock", SPINLOCK_TYPE_NORMAL);

    // IO Semaphore
    Semaphore_Init(&ethDev->ioSema, 0, "E1000 Semaphore");
    ethDev->nic.tx = &E1000_Enqueue;
    ethDev->nic.rx = &E1000_Dequeue;

    // Enable interrupts
    //MMIO_Write32(ethDev, E1000_REG_IMC, ~0);
    MMIO_Write32(ethDev, E1000_REG_IMS, 0x1F6DC); //ICR_TXDW | ICR_RXO | ICR_RXT0);
    MMIO_Read32(ethDev, E1000_REG_ICR);  // Clear pending interrupts

    E1000_RXInit(ethDev);
    E1000_TXInit(ethDev);

    ethDev->nic.handle = ethDev;
    // XXX: Fill in callbacks
    NIC_AddNIC(&ethDev->nic);
}

