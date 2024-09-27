
#define PCI_OFFSET_VENDORID	0x00
#define PCI_OFFSET_DEVICEID	0x02
#define PCI_OFFSET_COMMAND	0x04
#define PCI_OFFSET_STATUS	0x06
#define PCI_OFFSET_REVISIONID	0x08
#define PCI_OFFSET_PROGIF	0x09
#define PCI_OFFSET_SUBCLASS	0x0A
#define PCI_OFFSET_CLASS	0x0B
#define PCI_OFFSET_CACHELINE	0x0C
#define PCI_OFFSET_LATENCYTMR	0x0D
#define PCI_OFFSET_HEADERTYPE	0x0E
#define PCI_OFFSET_BIST		0x0F

#define PCI_OFFSET_BARFIRST	0x10
#define PCI_OFFSET_BARLAST	0x24

#define PCI_OFFSET_IRQLINE	0x3C

#define PCI_COMMAND_IOENABLE	0x0001
#define PCI_COMMAND_MEMENABLE	0x0002
#define PCI_COMMAND_BUSMASTER	0x0004

#define PCI_MAX_BARS		6

#define PCI_CLASS_STORAGE	0x01
#define PCI_CLASS_NETWORK	0x02
#define PCI_CLASS_GRAPHICS	0x03
#define PCI_CLASS_BRIDGE	0x06
#define PCI_CLASS_BUS		0x0C

#define PCI_SCLASS_STORAGE_IDE	0x01
#define PCI_SCLASS_STORAGE_SATA	0x06

#define PCI_SCLASS_BRIDGE_HOST	0x00
#define PCI_SCLASS_BRIDGE_ISA	0x01
#define PCI_SCLASS_BRIDGE_PCI	0x04
#define PCI_SCLASS_BRIDGE_MISC	0x80

#define PCI_SCLASS_BUS_FW	0x00
#define PCI_SCLASS_BUS_USB	0x03
#define PCI_SCLASS_BUS_SMBUS	0x05

#define PCIBAR_TYPE_NULL	0
#define PCIBAR_TYPE_IO		1
#define PCIBAR_TYPE_MEM		2

typedef struct PCIBAR
{
    uint32_t base;
    uint32_t size;
    uint32_t type;
} PCIBAR;

typedef struct PCIDevice
{
    uint16_t vendor;
    uint16_t device;

    uint8_t bus;
    uint8_t slot;
    uint8_t func;

    uint8_t irq;

    PCIBAR bars[PCI_MAX_BARS];
} PCIDevice;

void PCI_Init();

uint8_t PCI_CfgRead8(PCIDevice *dev, uint32_t reg);
uint16_t PCI_CfgRead16(PCIDevice *dev, uint32_t reg);
uint32_t PCI_CfgRead32(PCIDevice *dev, uint32_t reg);
void PCI_CfgWrite8(PCIDevice *dev, uint32_t reg, uint8_t data);
void PCI_CfgWrite16(PCIDevice *dev, uint32_t reg, uint16_t data);
void PCI_CfgWrite32(PCIDevice *dev, uint32_t reg, uint32_t data);

uint16_t PCI_GetDeviceID(PCIDevice *dev);
uint16_t PCI_GetVendorID(PCIDevice *dev);
uint8_t PCI_GetBaseClass(PCIDevice *dev);
uint8_t PCI_GetSubClass(PCIDevice *dev);
uint8_t PCI_GetHeaderType(PCIDevice *dev);

void PCI_Configure(PCIDevice *dev);

