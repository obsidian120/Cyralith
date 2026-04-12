#include "storage.h"
#include "io.h"

#define ATA_PRIMARY_IO 0x1F0U
#define ATA_PRIMARY_CTRL 0x3F6U
#define ATA_REG_DATA (ATA_PRIMARY_IO + 0U)
#define ATA_REG_ERROR (ATA_PRIMARY_IO + 1U)
#define ATA_REG_SECCOUNT0 (ATA_PRIMARY_IO + 2U)
#define ATA_REG_LBA0 (ATA_PRIMARY_IO + 3U)
#define ATA_REG_LBA1 (ATA_PRIMARY_IO + 4U)
#define ATA_REG_LBA2 (ATA_PRIMARY_IO + 5U)
#define ATA_REG_HDDEVSEL (ATA_PRIMARY_IO + 6U)
#define ATA_REG_COMMAND (ATA_PRIMARY_IO + 7U)
#define ATA_REG_STATUS (ATA_PRIMARY_IO + 7U)
#define ATA_REG_CONTROL ATA_PRIMARY_CTRL

#define ATA_CMD_READ_PIO 0x20U
#define ATA_CMD_WRITE_PIO 0x30U
#define ATA_CMD_CACHE_FLUSH 0xE7U
#define ATA_CMD_IDENTIFY 0xECU

#define ATA_SR_ERR 0x01U
#define ATA_SR_DRQ 0x08U
#define ATA_SR_DF  0x20U
#define ATA_SR_DRDY 0x40U
#define ATA_SR_BSY 0x80U

static int g_storage_available = 0;
static uint32_t g_total_sectors = 0U;

static int ata_wait_not_busy(unsigned int limit) {
    unsigned int i;
    for (i = 0U; i < limit; ++i) {
        uint8_t status = inb(ATA_REG_STATUS);
        if ((status & ATA_SR_BSY) == 0U) {
            return 0;
        }
    }
    return -1;
}

static int ata_wait_drq(unsigned int limit) {
    unsigned int i;
    for (i = 0U; i < limit; ++i) {
        uint8_t status = inb(ATA_REG_STATUS);
        if ((status & ATA_SR_BSY) == 0U) {
            if ((status & (ATA_SR_ERR | ATA_SR_DF)) != 0U) {
                return -1;
            }
            if ((status & ATA_SR_DRQ) != 0U) {
                return 0;
            }
        }
    }
    return -1;
}

void storage_init(void) {
    uint16_t identify[256];
    unsigned int i;
    uint8_t status;
    g_storage_available = 0;
    g_total_sectors = 0U;

    outb(ATA_REG_CONTROL, 0U);
    outb(ATA_REG_HDDEVSEL, 0xA0U);
    io_wait();
    outb(ATA_REG_SECCOUNT0, 0U);
    outb(ATA_REG_LBA0, 0U);
    outb(ATA_REG_LBA1, 0U);
    outb(ATA_REG_LBA2, 0U);
    outb(ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    io_wait();

    status = inb(ATA_REG_STATUS);
    if (status == 0U || status == 0xFFU) {
        return;
    }

    if (inb(ATA_REG_LBA1) != 0U || inb(ATA_REG_LBA2) != 0U) {
        return;
    }

    if (ata_wait_drq(1000000U) != 0) {
        return;
    }

    for (i = 0U; i < 256U; ++i) {
        identify[i] = inw(ATA_REG_DATA);
    }

    g_total_sectors = ((uint32_t)identify[61] << 16U) | (uint32_t)identify[60];
    if (g_total_sectors < 64U) {
        return;
    }

    g_storage_available = 1;
}

int storage_available(void) {
    return g_storage_available;
}

const char* storage_name(void) {
    return g_storage_available != 0 ? "ATA PIO disk" : "RAM only";
}

uint32_t storage_total_sectors(void) {
    return g_total_sectors;
}

int storage_read_sector(uint32_t lba, uint8_t* buffer) {
    unsigned int i;
    if (g_storage_available == 0 || buffer == (uint8_t*)0) {
        return -1;
    }
    if (lba >= g_total_sectors) {
        return -1;
    }
    if (ata_wait_not_busy(1000000U) != 0) {
        return -1;
    }
    outb(ATA_REG_HDDEVSEL, (uint8_t)(0xE0U | ((lba >> 24U) & 0x0FU)));
    outb(ATA_REG_ERROR, 0U);
    outb(ATA_REG_SECCOUNT0, 1U);
    outb(ATA_REG_LBA0, (uint8_t)(lba & 0xFFU));
    outb(ATA_REG_LBA1, (uint8_t)((lba >> 8U) & 0xFFU));
    outb(ATA_REG_LBA2, (uint8_t)((lba >> 16U) & 0xFFU));
    outb(ATA_REG_COMMAND, ATA_CMD_READ_PIO);
    if (ata_wait_drq(1000000U) != 0) {
        return -1;
    }
    for (i = 0U; i < 256U; ++i) {
        uint16_t value = inw(ATA_REG_DATA);
        buffer[i * 2U] = (uint8_t)(value & 0xFFU);
        buffer[i * 2U + 1U] = (uint8_t)((value >> 8U) & 0xFFU);
    }
    return 0;
}

int storage_write_sector(uint32_t lba, const uint8_t* buffer) {
    unsigned int i;
    if (g_storage_available == 0 || buffer == (const uint8_t*)0) {
        return -1;
    }
    if (lba >= g_total_sectors) {
        return -1;
    }
    if (ata_wait_not_busy(1000000U) != 0) {
        return -1;
    }
    outb(ATA_REG_HDDEVSEL, (uint8_t)(0xE0U | ((lba >> 24U) & 0x0FU)));
    outb(ATA_REG_ERROR, 0U);
    outb(ATA_REG_SECCOUNT0, 1U);
    outb(ATA_REG_LBA0, (uint8_t)(lba & 0xFFU));
    outb(ATA_REG_LBA1, (uint8_t)((lba >> 8U) & 0xFFU));
    outb(ATA_REG_LBA2, (uint8_t)((lba >> 16U) & 0xFFU));
    outb(ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
    if (ata_wait_drq(1000000U) != 0) {
        return -1;
    }
    for (i = 0U; i < 256U; ++i) {
        uint16_t value = (uint16_t)buffer[i * 2U] | ((uint16_t)buffer[i * 2U + 1U] << 8U);
        outw(ATA_REG_DATA, value);
    }
    outb(ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    if (ata_wait_not_busy(1000000U) != 0) {
        return -1;
    }
    return 0;
}
