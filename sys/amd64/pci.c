
#include <stdint.h>

#include <sys/kassert.h>
#include <sys/pci.h>

#include <machine/amd64.h>
#include <machine/amd64op.h>

#define PCI_PORT_ADDR		0xCF8
#define PCI_PORT_DATABASE	0xCFC

static inline uint32_t
PCIGetAddr(uint32_t bus, uint32_t slot, uint32_t func, uint32_t reg)
{
    ASSERT(bus < 256 && slot < 64 && func < 8 && reg < 256);
    return (1 << 31) | (bus << 16) | (slot << 11) | (func << 8) | (reg & 0x00fc);
}

uint8_t
PCICfgRead8(uint32_t bus, uint32_t slot, uint32_t func, uint32_t reg)
{
    uint32_t addr = PCIGetAddr(bus, slot, func, reg);
    uint16_t port = PCI_PORT_DATABASE + (reg & 0x3);

    outl(PCI_PORT_ADDR, addr);
    return inb(port);
}

uint16_t
PCICfgRead16(uint32_t bus, uint32_t slot, uint32_t func, uint32_t reg)
{
    uint32_t addr = PCIGetAddr(bus, slot, func, reg);
    uint16_t port = PCI_PORT_DATABASE + (reg & 0x2);

    ASSERT((reg & 0x1) == 0);

    outl(PCI_PORT_ADDR, addr);
    return inw(port);
}

uint32_t
PCICfgRead32(uint32_t bus, uint32_t slot, uint32_t func, uint32_t reg)
{
    uint32_t addr = PCIGetAddr(bus, slot, func, reg);
    uint16_t port = PCI_PORT_DATABASE;

    ASSERT((reg & 0x3) == 0);

    outl(PCI_PORT_ADDR, addr);
    return inl(port);
}

void
PCICfgWrite8(uint32_t bus, uint32_t slot, uint32_t func, uint32_t reg,
	      uint8_t data)
{
    uint32_t addr = PCIGetAddr(bus, slot, func, reg);
    uint16_t port = PCI_PORT_DATABASE + (reg & 0x3);

    outl(PCI_PORT_ADDR, addr);
    outb(port, data);
}

void
PCICfgWrite16(uint32_t bus, uint32_t slot, uint32_t func, uint32_t reg,
	       uint16_t data)
{
    uint32_t addr = PCIGetAddr(bus, slot, func, reg);
    uint16_t port = PCI_PORT_DATABASE + (reg & 0x3);

    outl(PCI_PORT_ADDR, addr);
    outw(port, data);
}

void
PCICfgWrite32(uint32_t bus, uint32_t slot, uint32_t func, uint32_t reg,
	       uint32_t data)
{
    uint32_t addr = PCIGetAddr(bus, slot, func, reg);
    uint16_t port = PCI_PORT_DATABASE + (reg & 0x3);

    outl(PCI_PORT_ADDR, addr);
    outl(port, data);
}

