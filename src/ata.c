#include "ata.h"
#include "cpu.h"
#include "interrupt.h"
#include "stdio.h"

#define ATA_PRIMARY_IO      0x1f0
#define ATA_SECONDARY_IO    0x170

// register offsets
#define ATA_REG_DATA            0x00
#define ATA_REG_ERROR           0x01    // (r)
#define ATA_REG_FEATURES        0x01    // (w)
#define ATA_REG_SECTOR_COUNT    0x02
#define ATA_REG_LBA_LOW         0x03
#define ATA_REG_LBA_MID         0x04
#define ATA_REG_LBA_HIGH        0x05
#define ATA_REG_DEVICE          0x06
#define ATA_REG_STATUS          0x07    // (r)
#define ATA_REG_COMMAND         0x07    // (w)
// packet and service
#define ATA_REG_INT_REASON      0x02
#define ATA_REG_BYTE_COUNT_LO   0x04
#define ATA_REG_BYTE_COUNT_HI   0x05
#define ATA_REG_DEVICE_SELECT   0x06

// control block registers
#define ATA_REG_ALT_STATUS      0x206   // (r)
#define ATA_REG_DEVICE_CONTROL  0x206   // (w)

// ATA command register
#define ATA_CMD_ATAPI_RESET         0x08
#define ATA_CMD_PIO_READ            0x20
#define ATA_CMD_PIO_READ_EXT        0x24
#define ATA_CMD_PIO_WRITE           0x30
#define ATA_CMD_PIO_WRITE_EXT       0x34
#define ATA_CMD_PACKET              0xa0
#define ATA_CMD_ATAPI_IDENTIFY      0xa1
#define ATA_CMD_READ_MULTI          0xc4
#define ATA_CMD_WRITE_MULTI         0xc5
#define ATA_CMD_SET_MUTLI           0xc6
#define ATA_CMD_READ_DMA            0xc8
#define ATA_CMD_WRITE_DMA           0xca
#define ATA_CMD_ATA_IDENTIFY        0xec
#define ATA_CMD_SET_FEATURES        0xef

// ATA status register
#define ATA_STATUS_ERROR        0x01
#define ATA_STATUS_INDEX        0x02
#define ATA_STATUS_CORRECTED    0x04    // data corrected
#define ATA_STATUS_DRQ          0x08    // data request ready
#define ATA_STATUS_DSC          0x10    // drive seek complete
#define ATA_STATUS_SERVICE      0x10    // drive needs service
#define ATA_STATUS_DWF          0x20    // drive write fault
#define ATA_STATUS_DMA          0x20    // DMA ready
#define ATA_STATUS_READY        0x40
#define ATA_STATUS_BUSY         0x80

// ATA error register
#define ATA_ERR_NO_MEDIA            0x02
#define ATA_ERR_COMMAND_ABORTED     0x04
#define ATA_ERR_MEDIA_CHANGE_REQ    0x08
#define ATA_ERR_ID_NOT_FOUND        0x10
#define ATA_ERR_MEDIA_CHANGE        0x20
#define ATA_ERR_UNCORRECTABLE_DATA  0x40
#define ATA_ERR_UDMA_CRC_ERROR      0x80

// ATA device control register
#define ATA_CTL_INT_DISABLE     0x02
#define ATA_CTL_RESET           0x04
#define ATA_CTL_4_HEAD_BITS     0x08

// ATA features register
#define ATA_FEATURE_DMA         0x01
#define ATA_FEATURE_OVERLAP     0x02

// ATA set features command
#define ATA_CMD_FEATURE_SETXFER         0x03
#define ATA_CMD_FEATURE_ENABLE_RCACHE   0xaa
#define ATA_CMD_FEATURE_ENABLE_WCACHE   0x02

// ATA drive select command
#define ATA_DRIVE_MASTER        0x00
#define ATA_DRIVE_SLAVE         0x10
#define ATA_DRIVE_LBA           0x40
#define ATA_DRIVE_512_SECTOR    0xa0

#if 0
struct ata_identify_t {
    uint16_t flags;
    uint16_t unused1[9];
    char     serial[20];
    uint16_t unused2[3];
    char     firmware[8];
    char     model[40];
    uint16_t sectors_per_int;
    uint16_t unused3;
    uint16_t capabilities[2];
    uint16_t unused4[2];
    uint16_t valid_ext_data;
    uint16_t unused5[5];
    uint16_t size_of_rw_mult;
    uint32_t sectors_28;
    uint16_t unused6[38];
    uint64_t sectors_48;
    uint16_t unused7[152];
} PACKED;
#endif

#define ATA_ID_CONFIG           0
#define ATA_ID_CYLS             1
#define ATA_ID_HEADS            3
#define ATA_ID_SECTORS          6
#define ATA_ID_MAX_MULTSECT     47  // max blocks per transfer
#define ATA_ID_CAPABILITY       49
#define ATA_ID_MULTSECT         59  // current multi sect
#define ATA_ID_LBA_CAPACITY     60
#define ATA_ID_COMMAND_SET_2    83
#define ATA_ID_CFS_ENABLE_1     85
#define ATA_ID_CFS_ENABLE_2     86
#define ATA_ID_CSF_DEFAULT      87
#define ATA_ID_LBA_CAPACITY_2   100

#define ata_id_is_ata(id)       (((id)[ATA_ID_CONFIG] & (1 << 15)) == 0)
#define ata_id_removable(id)    ((id)[ATA_ID_CONFIG] & (1 << 7))

#define ata_id_has_lba(id)      ((id)[ATA_ID_CAPABILITY] & (1 << 9))
#define ata_id_has_dma(id)      ((id)[ATA_ID_CAPABILITY] & (1 << 8))

#define ata_id_has_iordy(id)    ((id)[ATA_ID_CAPABILITY] & (1 << 11))
#define ata_id_u32(id,n)        \
         (((uint32_t)(id)[(n) + 1] << 16) | ((uint32_t)(id)[(n)]))
#define ata_id_u64(id,n)        \
         ( ((uint64_t)(id)[(n) + 3] << 48) | \
           ((uint64_t)(id)[(n) + 2] << 32) | \
           ((uint64_t)(id)[(n) + 1] << 16) | \
           ((uint64_t)(id)[(n) + 0]) )


static inline bool ata_id_has_lba48(const uint16_t* id)
{
    if ((id[ATA_ID_COMMAND_SET_2] & 0xc000) != 0x4000)
        return false;
    if (!ata_id_u64(id, ATA_ID_LBA_CAPACITY_2))
        return false;
    return id[ATA_ID_COMMAND_SET_2] & (1 << 10);
}
 
static inline bool ata_id_lba48_enabled(const uint16_t* id)
{
    if (ata_id_has_lba48(id) == 0)
        return false;
    if ((id[ATA_ID_CSF_DEFAULT] & 0xc000) != 0x4000)
        return false;
    return id[ATA_ID_CFS_ENABLE_2] & (1 << 10);
}

static int int_skip;

static void ata_interrupt()
{
    unsigned int io_base = ATA_PRIMARY_IO;
    uint8_t status = inb(io_base + ATA_REG_STATUS);
    //printf("ata_interrupt(): %x\n", status);

    if (status & ATA_STATUS_ERROR) {
        uint8_t err = inb(io_base + ATA_REG_ERROR);
        printf("ata_interrupt(): error %x\n", err);
        return;
    }

    uint8_t mask = ATA_STATUS_READY
                 | ATA_STATUS_DRQ
                 | ATA_STATUS_DSC;
    if (!(status & ATA_STATUS_BUSY) && ((status & mask) == mask)) {
        printf("ata_interrupt(): got data\n");
        if (int_skip)
            return;

        uint16_t buf[256];
        for (int i = 0; i < 256; ++i)
            buf[i] = inw(io_base + ATA_REG_DATA);

    } else {
        printf("ata_interrupt(): weird status %x\n", status);
    }
}

void ata_init()
{
    printf("ata_init()\n");

    intr_register_irq_handler(14, ata_interrupt);
    intr_irq_enable(14, 1 << get_cpu()->apic_id);

    unsigned int io_base = ATA_PRIMARY_IO;

    int_skip = 1;
    outb(io_base + ATA_REG_DEVICE_CONTROL, ATA_CTL_4_HEAD_BITS);

    outb(io_base + ATA_REG_DEVICE, ATA_DRIVE_512_SECTOR | ATA_DRIVE_MASTER);

    outb(io_base + ATA_REG_SECTOR_COUNT, 0);
    outb(io_base + ATA_REG_LBA_LOW, 0);
    outb(io_base + ATA_REG_LBA_MID, 0);
    outb(io_base + ATA_REG_LBA_HIGH, 0);

    outb(io_base + ATA_REG_COMMAND, ATA_CMD_ATA_IDENTIFY);

    uint8_t status = inb(io_base + ATA_REG_STATUS);
    if (status) {
        int err = 0;
        while(1) {
            status = inb(io_base + ATA_REG_STATUS);
            if ((status & ATA_STATUS_ERROR)) {
                err = 1;
                break;
            }
            if (!(status & ATA_STATUS_BUSY) && (status & ATA_STATUS_DRQ)) 
                break;
         }

         if (err)
            return;

        uint16_t buf[256];
        int s = cpu_splhi();
        for (int i = 0; i < 256; ++i)
            buf[i] = inw(io_base + ATA_REG_DATA);
        cpu_splx(s);

        printf("ata_init(): ident read\n");
        printf("config : %04x\n", buf[ATA_ID_CONFIG]);
        printf("cyls   : %04x\n", buf[ATA_ID_CYLS]);
        printf("heads  : %04x\n", buf[ATA_ID_HEADS]);
        printf("sectors: %04x\n", buf[ATA_ID_SECTORS]);
        printf("caps   : %04x\n", buf[ATA_ID_CAPABILITY]);

        printf("%08x %08x\n",
            buf[ATA_ID_LBA_CAPACITY], buf[ATA_ID_LBA_CAPACITY+1]);
        /*
        for (int i = 0; i < 16; ++i) {
            if (buf[ATA_ID_CAPABILITY] & (1<<i))
                printf("%d ", i);
        }
        printf("\n");
        */

        if (ata_id_lba48_enabled(buf)) {
            printf("lba48: %016lx\n", ata_id_u64(buf, ATA_ID_LBA_CAPACITY_2));
        } else if (ata_id_has_lba(buf)) {
            printf("lba: %08x\n", ata_id_u32(buf, ATA_ID_LBA_CAPACITY));
        } else {
            uint32_t cyls = buf[ATA_ID_CYLS];
            uint32_t heads = buf[ATA_ID_HEADS];
            uint32_t sectors = buf[ATA_ID_SECTORS];
            uint32_t size = cyls * heads * sectors;
            printf("chs: %08x\n", size);
        }

        printf("max_multi_sect: %d\n", buf[ATA_ID_MAX_MULTSECT]);
        printf("multisect: %d\n", buf[ATA_ID_MULTSECT] && 0xFF);


        int_skip = 0;
        outb(io_base + ATA_REG_DEVICE, ATA_DRIVE_LBA | ATA_DRIVE_MASTER);
        
        uint32_t lba_addr = 419430400-1;
        uint8_t lba[6] = {
            (lba_addr & 0x000000FF) >> 0,
            (lba_addr & 0x0000FF00) >> 8,
            (lba_addr & 0x00FF0000) >> 16,
            (lba_addr & 0xFF000000) >> 24,
            0, 0
        };

        outb(io_base + ATA_REG_SECTOR_COUNT, 0);
        outb(io_base + ATA_REG_LBA_LOW, lba[3]);
        outb(io_base + ATA_REG_LBA_MID, lba[4]);
        outb(io_base + ATA_REG_LBA_HIGH, lba[5]);

        outb(io_base + ATA_REG_SECTOR_COUNT, 1);
        outb(io_base + ATA_REG_LBA_LOW, lba[0]);
        outb(io_base + ATA_REG_LBA_MID, lba[1]);
        outb(io_base + ATA_REG_LBA_HIGH, lba[2]);

        outb(io_base + ATA_REG_COMMAND, ATA_CMD_PIO_READ_EXT);
        
    }
}
