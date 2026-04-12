#ifndef STORAGE_H
#define STORAGE_H

#include <stddef.h>
#include <stdint.h>

#define STORAGE_SECTOR_SIZE 512U

void storage_init(void);
int storage_available(void);
const char* storage_name(void);
uint32_t storage_total_sectors(void);
int storage_read_sector(uint32_t lba, uint8_t* buffer);
int storage_write_sector(uint32_t lba, const uint8_t* buffer);

#endif
