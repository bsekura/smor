#include "pci.h"
#include "io.h"
#include "cpu.h"
#include "stdio.h"

#define PCI_MAX_BUS     256
#define PCI_MAX_DEV     32
#define PCI_MAX_FUNC    8

#define PCI_MAKE_ID(bus, dev, func) \
    ((bus) << 16) | ((dev) << 11) | ((func) << 8)

#define PCI_CONFIG_ADDR     0xcf8
#define PCI_CONFIG_DATA     0xcfc

// header
#define PCI_TYPE_MULTIFUNC              0x80
#define PCI_TYPE_GENERIC                0x00
#define PCI_TYPE_PCI_BRIDGE             0x01
#define PCI_TYPE_CARDBUS_BRIDGE         0x02

// register config space
#define PCI_CONFIG_VENDOR_ID            0x00
#define PCI_CONFIG_DEVICE_ID            0x02
#define PCI_CONFIG_COMMAND              0x04
#define PCI_CONFIG_STATUS               0x06
#define PCI_CONFIG_REVISION_ID          0x08
#define PCI_CONFIG_PROG_INTF            0x09
#define PCI_CONFIG_SUBCLASS             0x0a
#define PCI_CONFIG_CLASS_CODE           0x0b
#define PCI_CONFIG_CACHELINE_SIZE       0x0c
#define PCI_CONFIG_LATENCY              0x0d
#define PCI_CONFIG_HEADER_TYPE          0x0e
#define PCI_CONFIG_BIST                 0x0f

// generic type
#define PCI_CONFIG_BAR_0                0x10
#define PCI_CONFIG_BAR_1                0x14
#define PCI_CONFIG_BAR_2                0x18
#define PCI_CONFIG_BAR_3                0x1C
#define PCI_CONFIG_BAR_4                0x20
#define PCI_CONFIG_BAR_5                0x24
#define PCI_CONFIG_CARDBUS_CIS          0x28
#define PCI_CONFIG_SUBSYSTEM_VENDOR_ID  0x2c
#define PCI_CONFIG_SUBSYSTEM_DEVICE_ID  0x2e
#define PCI_CONFIG_EXPANSION_ROM        0x30
#define PCI_CONFIG_CAPABILITIES         0x34
#define PCI_CONFIG_INTERRUPT_LINE       0x3c
#define PCI_CONFIG_INTERRUPT_PIN        0x3d
#define PCI_CONFIG_MIN_GRANT            0x3e
#define PCI_CONFIG_MAX_LATENCY          0x3f

#define PCI_CONFIG_BAR_SIZE             0x04

// bars: base address registers
#define PCI_BAR_IO                      0x1 // 1 == pio, 0 == mmio
#define PCI_BAR_IO_MASK                 0xfffffffc
#define PCI_BAR_MEM_MASK                0xfffffff0
#define PCI_MEMBAR_TYPE                 (3 << 1)
#define PCI_MEMBAR_32_BIT               0x0
#define PCI_MEMBAR_64_BIT               0x4

struct pci_bar_t {
    uint32_t bar;
    uint32_t pio_base;
    uint32_t mmio_size;
    uint32_t mmio_base32;
    uint64_t mmio_base64;
};

#define PCI_MAX_DEVICES     32
#define PCI_MAX_BARS        6
#define PCI_MAX_CAPS        16

struct pci_device_t {
    uint8_t bus;
    uint8_t dev;
    uint8_t func;
    uint8_t header_type;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class;
    uint8_t subclass;
    uint8_t prog_intf;
    uint8_t int_line;
    uint8_t int_pin;
    uint8_t num_bars;
    uint8_t num_caps;
    struct pci_bar_t bars[PCI_MAX_BARS];
    uint8_t caps[PCI_MAX_CAPS];
};

static uint8_t pci_read_8(unsigned int id, unsigned int reg)
{
    uint32_t addr = 0x80000000 | id | (reg & 0xfc);
    int spl = cpu_splhi();
    outl(PCI_CONFIG_ADDR, addr);
    uint8_t val = inb(PCI_CONFIG_DATA + (reg & 0x03));
    cpu_splx(spl);
    return val;
}

static uint16_t pci_read_16(unsigned int id, unsigned int reg)
{
    uint32_t addr = 0x80000000 | id | (reg & 0xfc);
    int spl = cpu_splhi();
    outl(PCI_CONFIG_ADDR, addr);
    uint16_t val = inw(PCI_CONFIG_DATA + (reg & 0x02));
    cpu_splx(spl);
    return val;
}

static uint32_t pci_read_32(unsigned int id, unsigned int reg)
{
    uint32_t addr = 0x80000000 | id | (reg & 0xfc);
    int spl = cpu_splhi();
    outl(PCI_CONFIG_ADDR, addr);
    uint32_t val = inl(PCI_CONFIG_DATA + reg);
    cpu_splx(spl);
    return val;
}

static void pci_write_32(unsigned int id, unsigned int reg, uint32_t val)
{
    uint32_t addr = 0x80000000 | id | (reg & 0xfc);
    int spl = cpu_splhi();
    outl(PCI_CONFIG_ADDR, addr);
    outl(PCI_CONFIG_DATA, val);
    cpu_splx(spl);
}

#define PCI_DEVICE_ID(dev) PCI_MAKE_ID(dev->bus, dev->dev, dev->func)

static void pci_read_caps(struct pci_device_t* dev)
{
}

static uint32_t pci_bar_size(unsigned int id, unsigned int bar)
{
    unsigned int bar_reg = PCI_CONFIG_BAR_0 + bar * PCI_CONFIG_BAR_SIZE;
    uint32_t cur_val = pci_read_32(id, bar_reg);
    pci_write_32(id, bar_reg, 0xFFFFFFFF);
    uint32_t val = pci_read_32(id, bar_reg) & PCI_BAR_MEM_MASK;
    val = ~val + 1;
    pci_write_32(id, bar_reg, cur_val);
    return val;
}

static void pci_read_bars(struct pci_device_t* dev)
{
    unsigned int id = PCI_DEVICE_ID(dev);
    if (dev->header_type == PCI_TYPE_GENERIC) {
        for (unsigned int i = 0; i < PCI_MAX_BARS; ++i ) {
            uint32_t bar = pci_read_32(id, PCI_CONFIG_BAR_0 + i * PCI_CONFIG_BAR_SIZE);
            if (!bar)
                continue;

            dev->bars[i].bar = bar;
            if (bar & PCI_BAR_IO) {
                dev->bars[i].pio_base = bar & PCI_BAR_IO_MASK;
            } else {
                if ((bar & PCI_MEMBAR_TYPE) == PCI_MEMBAR_32_BIT) {
                    dev->bars[i].mmio_base32 = bar & PCI_BAR_MEM_MASK;
                    dev->bars[i].mmio_size = pci_bar_size(id, i);
                } else if ((bar & PCI_MEMBAR_TYPE) == PCI_MEMBAR_64_BIT) {
                    uint32_t next_bar
                        = pci_read_32(id, PCI_CONFIG_BAR_0 + (i+1) * PCI_CONFIG_BAR_SIZE);
                    dev->bars[i].mmio_base64 = bar & PCI_BAR_MEM_MASK;
                    dev->bars[i].mmio_base64 = (uint64_t)next_bar << 32;
                    dev->bars[i].mmio_size = pci_bar_size(id, i);
                    ++i;
                }
            }
        }
    }
}

static struct pci_device_t pci_devices[PCI_MAX_DEVICES];
static uint32_t pci_num_devices;

void pci_init()
{
#if 1
    for (unsigned int bus = 0; bus < PCI_MAX_BUS; ++bus) {
        for (unsigned int dev = 0; dev < PCI_MAX_DEV; ++dev) {
            unsigned int base_id = PCI_MAKE_ID(bus, dev, 0);
            uint8_t header_type = pci_read_8(base_id, PCI_CONFIG_HEADER_TYPE);
            unsigned int n_func = header_type & PCI_TYPE_MULTIFUNC 
                                ? PCI_MAX_FUNC : 1;

            for (unsigned int func = 0; func < n_func; ++func) {
                unsigned int id = PCI_MAKE_ID(bus, dev, func);
                uint16_t vendor_id = pci_read_16(id, PCI_CONFIG_VENDOR_ID);
                if (vendor_id == 0xFFFF)
                    continue;

                struct pci_device_t* pci_device = &pci_devices[pci_num_devices++];

                pci_device->bus = bus;
                pci_device->dev = dev;
                pci_device->func = func;
                pci_device->header_type = header_type & 0x7F;
                pci_device->vendor_id = vendor_id;
                pci_device->device_id = pci_read_16(id, PCI_CONFIG_DEVICE_ID);
                pci_device->class = pci_read_8(id, PCI_CONFIG_CLASS_CODE);
                pci_device->subclass = pci_read_8(id, PCI_CONFIG_SUBCLASS);
                pci_device->prog_intf = pci_read_8(id, PCI_CONFIG_PROG_INTF);
                pci_device->int_line = pci_read_8(id, PCI_CONFIG_INTERRUPT_LINE);
                pci_device->int_pin = pci_read_8(id, PCI_CONFIG_INTERRUPT_PIN);

                pci_read_bars(pci_device);
                pci_read_caps(pci_device);
            }
        }
    }
#endif
#if 0
    for (unsigned int bus = 0; bus < 256; ++bus) {
        for (unsigned int dev = 0; dev < 32; ++dev) {
            unsigned int base_id = PCI_MAKE_ID(bus, dev, 0);
            uint8_t header_type = pci_read_8(base_id, PCI_CONFIG_HEADER_TYPE);
            unsigned int func_count = header_type & PCI_TYPE_MULTIFUNC ? 8 : 1;

            for (unsigned int func = 0; func < func_count; ++func) {
                unsigned int id = PCI_MAKE_ID(bus, dev, func);
                uint16_t vendor_id = pci_read_16(id, PCI_CONFIG_VENDOR_ID);
                if (vendor_id == 0xFFFF)
                    continue;

                uint16_t device_id = pci_read_16(id, PCI_CONFIG_DEVICE_ID);
                uint8_t prog_intf = pci_read_8(id, PCI_CONFIG_PROG_INTF);
                uint8_t subclass = pci_read_8(id, PCI_CONFIG_SUBCLASS);
                uint8_t class_code = pci_read_8(id, PCI_CONFIG_CLASS_CODE);

                uint8_t int_line = 0;
                uint8_t int_pin = 0;

                if ((header_type & 0x7F) == 0) {
                    int_line = pci_read_8(id, PCI_CONFIG_INTERRUPT_LINE);
                    int_pin = pci_read_8(id, PCI_CONFIG_INTERRUPT_PIN);
                }
                
                printf("pci (%d,%d,%d): vendor_id %04x, device_id %04x, [%02x:%02x:%02x] (%02x,%02x)\n",
                        bus, dev, func, vendor_id, device_id, 
                        (uint32_t)class_code, (uint32_t)subclass, (uint32_t)prog_intf,
                        (uint32_t)int_line, (uint32_t)int_pin);

                if ((header_type & 0x7F) == 0) {
                    if (pci_read_16(id, PCI_CONFIG_STATUS) & (1<<4)) {
                        uint8_t caps = pci_read_8(id, PCI_CONFIG_CAPABILITIES);
                        printf("pci_caps: %d\n", caps);
                        caps &= ~0x3;
                        while (caps) {
                            uint8_t cap_id = pci_read_8(id, caps);
                            printf("%d\n", cap_id);
                            caps = pci_read_8(id, caps + 1);
                            caps &= ~0x3;
                        }
                    }
                }
            }
        }
    }
#endif
}
