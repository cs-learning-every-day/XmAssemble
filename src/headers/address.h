#ifndef ADDRESS_H
#define ADDRESS_H

#include <stdint.h>

/*
    S = 2^s, s = 6
    B = 2^b, b = 6
    E is defined in sram.c, 8
    For correctness verification, E can be 1, 2, 8, 1024
 */

#ifndef CACHE_SIMULATION_VERIFICATION
/*  for cache simulator verification
    use the marcos passed in
 */
#define SRAM_CACHE_INDEX_LENGTH (6)
#define SRAM_CACHE_OFFSET_LENGTH (6)
#define SRAM_CACHE_TAG_LENGTH (40)
#endif

#define PHYSICAL_PAGE_OFFSET_LENGTH (12)
#define PHYSICAL_PAGE_NUMBER_LENGTH (40)
#define PHYSICAL_ADDRESS_LENGTH (52)

typedef union
{
    uint64_t address_value;

    // physical address: 52
    struct
    {
        union
        {
            uint64_t paddr_value : PHYSICAL_ADDRESS_LENGTH;
            struct
            {
                uint64_t PPO : PHYSICAL_PAGE_OFFSET_LENGTH;
                uint64_t PPN : PHYSICAL_PAGE_NUMBER_LENGTH;
            };
        };
    };

    // sram cache: 52
    struct
    {
        uint64_t CO : SRAM_CACHE_OFFSET_LENGTH;
        uint64_t CI : SRAM_CACHE_INDEX_LENGTH;
        uint64_t CT : SRAM_CACHE_TAG_LENGTH;
    };
} address_t;

#endif