
#include <stdbool.h>
#include <stdint.h>

#include <sys/kassert.h>
#include <sys/kdebug.h>
#include <sys/pci.h>

static void PCIScan();

// Platform functions
uint8_t PCICfgRead8(uint32_t bus, uint32_t slot, uint32_t func, uint32_t reg);
uint16_t PCICfgRead16(uint32_t bus, uint32_t slot, uint32_t func, uint32_t reg);
uint32_t PCICfgRead32(uint32_t bus, uint32_t slot, uint32_t func, uint32_t reg);
void PCICfgWrite8(uint32_t bus, uint32_t slot, uint32_t func,
                  uint32_t reg, uint8_t data);
void PCICfgWrite16(uint32_t bus, uint32_t slot, uint32_t func,
                  uint32_t reg, uint16_t data);
void PCICfgWrite32(uint32_t bus, uint32_t slot, uint32_t func,
                  uint32_t reg, uint32_t data);

// Supported Devices
void AHCI_Init(uint32_t bus, uint32_t device, uint32_t func);
void E1000_Init(uint32_t bus, uint32_t device, uint32_t func);

void
PCI_Init()
{
    kprintf("PCI: Initializing ...\n");
    PCIScan();
    kprintf("PCI: Initialization Done!\n");
}

uint16_t
PCIGetDeviceID(uint32_t bus, uint32_t device, uint32_t func)
{
    return PCICfgRead16(bus, device, func, PCI_OFFSET_DEVICEID);
}

uint16_t
PCIGetVendorID(uint32_t bus, uint32_t device, uint32_t func)
{
    return PCICfgRead16(bus, device, func, PCI_OFFSET_VENDORID);
}

uint8_t
PCIGetBaseClass(uint32_t bus, uint32_t device, uint32_t func)
{
    return PCICfgRead8(bus, device, func, PCI_OFFSET_CLASS);
}

uint8_t
PCIGetSubClass(uint32_t bus, uint32_t device, uint32_t func)
{
    return PCICfgRead8(bus, device, func, PCI_OFFSET_SUBCLASS);
}

uint8_t
PCIGetHeaderType(uint32_t bus, uint32_t device, uint32_t func)
{
    return PCICfgRead8(bus, device, func, PCI_OFFSET_HEADERTYPE);
}

uint8_t
PCI_CfgRead8(PCIDevice *dev, uint32_t reg)
{
    return PCICfgRead8(dev->bus, dev->slot, dev->func, reg);
}

uint16_t
PCI_CfgRead16(PCIDevice *dev, uint32_t reg)
{
    return PCICfgRead16(dev->bus, dev->slot, dev->func, reg);
}

uint32_t
PCI_CfgRead32(PCIDevice *dev, uint32_t reg)
{
    return PCICfgRead32(dev->bus, dev->slot, dev->func, reg);
}

void
PCI_CfgWrite8(PCIDevice *dev, uint32_t reg, uint8_t data)
{
    return PCICfgWrite8(dev->bus, dev->slot, dev->func, reg, data);
}

void
PCI_CfgWrite16(PCIDevice *dev, uint32_t reg, uint16_t data)
{
    return PCICfgWrite16(dev->bus, dev->slot, dev->func, reg, data);
}

void
PCI_CfgWrite32(PCIDevice *dev, uint32_t reg, uint32_t data)
{
    return PCICfgWrite32(dev->bus, dev->slot, dev->func, reg, data);
}

uint16_t
PCI_GetDeviceID(PCIDevice *dev)
{
    return PCI_CfgRead16(dev, PCI_OFFSET_DEVICEID);
}

uint16_t
PCI_GetVendorID(PCIDevice *dev)
{
    return PCI_CfgRead16(dev, PCI_OFFSET_VENDORID);
}

uint8_t
PCI_GetBaseClass(PCIDevice *dev)
{
    return PCI_CfgRead8(dev, PCI_OFFSET_CLASS);
}

uint8_t
PCI_GetSubClass(PCIDevice *dev)
{
    return PCI_CfgRead8(dev, PCI_OFFSET_SUBCLASS);
}

uint8_t
PCI_GetHeaderType(PCIDevice *dev)
{
    return PCI_CfgRead8(dev, PCI_OFFSET_HEADERTYPE);
}

/*
 * PCICheckFunction --
 *
 * 	Identify device type and initialize known devices.
 */
static void
PCICheckFunction(uint32_t bus, uint32_t device, uint32_t func)
{
    uint8_t baseClass, subClass;
    uint16_t vendorId, deviceId;

    baseClass = PCIGetBaseClass(bus, device, func);
    subClass = PCIGetSubClass(bus, device, func);
    vendorId = PCIGetVendorID(bus, device, func);
    deviceId = PCIGetDeviceID(bus, device, func);

    if (baseClass == PCI_CLASS_BRIDGE) {
        if (subClass == PCI_SCLASS_BRIDGE_HOST) {
            kprintf("PCI: (%d,%d,%d) Host Bridge (%04x:%04x)\n",
                    bus, device, func, vendorId, deviceId);
        } else if (subClass == PCI_SCLASS_BRIDGE_ISA) {
            kprintf("PCI: (%d,%d,%d) ISA Bridge (%04x:%04x)\n",
                    bus, device, func, vendorId, deviceId);
        } else if (subClass == PCI_SCLASS_BRIDGE_PCI) {
            kprintf("PCI: (%d,%d,%d) PCI-PCI Bridge (%04x:%04x)\n",
                    bus, device, func, vendorId, deviceId);
            // Scan sub-bus
        } else if (subClass == PCI_SCLASS_BRIDGE_MISC) {
            kprintf("PCI: (%d,%d,%d) Other Bridge (%04x:%04x)\n",
                    bus, device, func, vendorId, deviceId);
        }
    } else if (baseClass == PCI_CLASS_STORAGE) {
        if (subClass == PCI_SCLASS_STORAGE_SATA) {
            kprintf("PCI: (%d,%d,%d) SATA Controller (%04x:%04x)\n",
                    bus, device, func, vendorId, deviceId);

            AHCI_Init(bus, device, func);
        } else if (subClass == PCI_SCLASS_STORAGE_IDE) {
            kprintf("PCI: (%d,%d,%d) IDE Controller (%04x:%04x)\n",
                    bus, device, func, vendorId, deviceId);
        }
    } else if ((baseClass == PCI_CLASS_NETWORK) && (subClass == 0x00)) {
        kprintf("PCI: (%d,%d,%d) Ethernet (%04x:%04x)\n",
                bus, device, func, vendorId, deviceId);
        E1000_Init(bus, device, func);
    } else if ((baseClass == PCI_CLASS_GRAPHICS) && (subClass == 0x00)) {
        kprintf("PCI: (%d,%d,%d) VGA (%04x:%04x)\n",
                bus, device, func, vendorId, deviceId);
    } else if ((baseClass == PCI_CLASS_BUS) && (subClass == PCI_SCLASS_BUS_SMBUS)) {
        kprintf("PCI: (%d,%d,%d) SMBUS (%04x:%04x)\n",
                bus, device, func, vendorId, deviceId);
    } else {
        kprintf("PCI: (%d,%d,%d) Unsupported (%04x:%04x %02x:%02x)\n",
                bus, device, func, vendorId, deviceId, baseClass, subClass);
    }
}

static void
PCIScanDevice(uint32_t bus, uint32_t device)
{
    uint8_t headerType;
    uint16_t vendorId;

    vendorId = PCIGetVendorID(bus, device, 0);
    if (vendorId == 0xFFFF)
        return;

    PCICheckFunction(bus, device, 0);

    headerType = PCIGetHeaderType(bus, device, 0);
    if ((headerType & 0x80) != 0) {
        uint8_t func;
        for (func = 0; func < 8; func++) {
            if (PCIGetVendorID(bus, device, func) != 0xFFFF) {
                PCICheckFunction(bus, device, func);
            }
        }
    }
}

static void
PCIScanBus(uint8_t bus)
{
    uint8_t device;

    for (device = 0; device < 32; device++) {
        PCIScanDevice(bus, device);
    }
}

/*
 * PCIScan --
 *
 * 	Scan all busses and devices.
 */
static void
PCIScan()
{
    uint8_t headerType = PCIGetHeaderType(0, 0, 0);

    if ((headerType & 0x80) == 0) {
        PCIScanBus(0);
    } else {
        uint8_t busNo;

        for (busNo = 0; busNo < 8; busNo++) {
            if (PCIGetVendorID(0, 0, busNo) != 0xFFFF)
                break;
            PCIScanBus(busNo);
        }
    }
}

/*
 * PCI_Configure --
 *
 * 	Configure a PCI device BAR registers.
 */
void
PCI_Configure(PCIDevice *dev)
{
    int bar;

    dev->irq = PCI_CfgRead8(dev, PCI_OFFSET_IRQLINE);

    PCI_CfgWrite16(dev, PCI_OFFSET_COMMAND,
                   PCI_COMMAND_IOENABLE | PCI_COMMAND_MEMENABLE | PCI_COMMAND_BUSMASTER);

    for (bar = 0; bar < PCI_MAX_BARS; bar++)
    {
        dev->bars[bar].base = 0;
        dev->bars[bar].size = 0;
        dev->bars[bar].type = PCIBAR_TYPE_NULL;
    }

    for (bar = 0; bar < PCI_MAX_BARS; bar++)
    {
        uint32_t barReg = PCI_OFFSET_BARFIRST + 4 * bar;
        uint32_t base, size;
        uint32_t origValue = PCI_CfgRead32(dev, barReg);

        PCI_CfgWrite32(dev, barReg, 0xFFFFFFFF);
        size = PCI_CfgRead32(dev, barReg);
        if (size == 0)
            continue;

        PCI_CfgWrite32(dev, barReg, origValue);

        if (origValue & 0x1)
        {
            dev->bars[bar].type = PCIBAR_TYPE_IO;
            base = origValue & 0xFFFFFFFC;
            size = size & 0xFFFFFFFC;
            size = ~size + 1;
        } else {
            dev->bars[bar].type = PCIBAR_TYPE_MEM;
            base = origValue & 0xFFFFFFF0;
            size = size & 0xFFFFFFF0;
            size = ~size + 1;
            // XXX: Support 64-bit
            ASSERT((origValue & 0x06) == 0x00);
        }

        dev->bars[bar].base = base;
        dev->bars[bar].size = size;
    }
}

static void
DebugPCICheckFunction(uint32_t bus, uint32_t device, uint32_t func)
{
    uint8_t baseClass, subClass;
    uint16_t vendorId, deviceId;

    baseClass = PCIGetBaseClass(bus, device, func);
    subClass = PCIGetSubClass(bus, device, func);
    vendorId = PCIGetVendorID(bus, device, func);
    deviceId = PCIGetDeviceID(bus, device, func);

    if (baseClass == PCI_CLASS_BRIDGE) {
        if (subClass == PCI_SCLASS_BRIDGE_HOST) {
            kprintf("PCI: (%d,%d,%d) Host Bridge (%04x:%04x)\n",
                    bus, device, func, vendorId, deviceId);
        } else if (subClass == PCI_SCLASS_BRIDGE_ISA) {
            kprintf("PCI: (%d,%d,%d) ISA Bridge (%04x:%04x)\n",
                    bus, device, func, vendorId, deviceId);
        } else if (subClass == PCI_SCLASS_BRIDGE_PCI) {
            kprintf("PCI: (%d,%d,%d) PCI-PCI Bridge (%04x:%04x)\n",
                    bus, device, func, vendorId, deviceId);
            // XXX: Scan sub-bus
        } else if (subClass == PCI_SCLASS_BRIDGE_MISC) {
            kprintf("PCI: (%d,%d,%d) Other Bridge (%04x:%04x)\n",
                    bus, device, func, vendorId, deviceId);
        }
    } else if (baseClass == PCI_CLASS_STORAGE) {
        if (subClass == PCI_SCLASS_STORAGE_SATA) {
            kprintf("PCI: (%d,%d,%d) SATA Controller (%04x:%04x)\n",
                    bus, device, func, vendorId, deviceId);
        } else if (subClass == PCI_SCLASS_STORAGE_IDE) {
            kprintf("PCI: (%d,%d,%d) IDE Controller (%04x:%04x)\n",
                    bus, device, func, vendorId, deviceId);
        }
    } else if ((baseClass == PCI_CLASS_NETWORK) && (subClass == 0x00)) {
        kprintf("PCI: (%d,%d,%d) Ethernet (%04x:%04x)\n",
                bus, device, func, vendorId, deviceId);
    } else if ((baseClass == PCI_CLASS_GRAPHICS) && (subClass == 0x00)) {
        kprintf("PCI: (%d,%d,%d) VGA (%04x:%04x)\n",
                bus, device, func, vendorId, deviceId);
    } else if ((baseClass == PCI_CLASS_BUS) && (subClass == PCI_SCLASS_BUS_SMBUS)) {
        kprintf("PCI: (%d,%d,%d) SMBUS (%04x:%04x)\n",
                bus, device, func, vendorId, deviceId);
    } else {
        kprintf("PCI: (%d,%d,%d) Unsupported (%04x:%04x %02x:%02x)\n",
                bus, device, func, vendorId, deviceId, baseClass, subClass);
    }
}

static void
DebugPCIScanBus(uint8_t bus)
{
    uint8_t device;
    uint8_t headerType;
    uint16_t vendorId;

    for (device = 0; device < 32; device++) {
        vendorId = PCIGetVendorID(bus, device, 0);
        if (vendorId == 0xFFFF)
                return;

        DebugPCICheckFunction(bus, device, 0);

        headerType = PCIGetHeaderType(bus, device, 0);
        if ((headerType & 0x80) != 0) {
            uint8_t func;
            for (func = 0; func < 8; func++) {
                if (PCIGetVendorID(bus, device, func) != 0xFFFF) {
                        DebugPCICheckFunction(bus, device, func);
                }
            }
        }
    }
}

void
Debug_PCIList(int argc, const char *argv[])
{
    uint8_t headerType = PCIGetHeaderType(0, 0, 0);

    if ((headerType & 0x80) == 0) {
        DebugPCIScanBus(0);
    } else {
        uint8_t busNo;

        for (busNo = 0; busNo < 8; busNo++) {
            if (PCIGetVendorID(0, 0, busNo) != 0xFFFF)
                break;
            DebugPCIScanBus(busNo);
        }
    }
}

REGISTER_DBGCMD(pcilist, "PCI Device List", Debug_PCIList);

void
Debug_PCIDump(int argc, const char *argv[])
{
    uint32_t bus, device, func;
    uint32_t bar;

    if (argc != 4) {
        kprintf("Usage: pcidump BUS# DEVICE# FUNC#\n");
        return;
    }

    bus = Debug_StrToInt(argv[1]);
    device = Debug_StrToInt(argv[2]);
    func = Debug_StrToInt(argv[3]);

    kprintf("Vendor ID: %04x\n", PCIGetVendorID(bus, device, func));
    kprintf("Device ID: %04x\n", PCIGetDeviceID(bus, device, func));
    kprintf("Class: %d\n", PCIGetBaseClass(bus, device, func));
    kprintf("Subclass: %d\n", PCIGetSubClass(bus, device, func));
    kprintf("Header Type: %d\n", PCIGetHeaderType(bus, device, func));

    for (bar = 0; bar < PCI_MAX_BARS; bar++)
    {
        uint32_t barReg = PCI_OFFSET_BARFIRST + 4 * bar;

        kprintf("BAR%d: %016llx\n", bar, PCICfgRead32(bus, device, func, barReg));
    }
}

REGISTER_DBGCMD(pcidump, "PCI Device Dump", Debug_PCIDump);

