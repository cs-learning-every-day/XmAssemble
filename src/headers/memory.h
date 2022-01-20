#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include "headers/cpu.h"

#define PHYSICAL_MEMORY_SPACE 65536
#define MAX_INDEX_PHYSICAL_PAGE 15

uint8_t pm[PHYSICAL_MEMORY_SPACE];


uint64_t read64bits_dram(uint64_t paddr);
void write64bits_dram(uint64_t paddr, uint64_t data);
void readinst_dram(uint64_t paddr, char *buf);
void writeinst_dram(uint64_t paddr, const char *str);

#endif