#include "headers/address.h"
#include "headers/memory.h"
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
    S = 2^s, s = 6
    B = 2^b, b = 6
    E is defined in sram.c, 8
    For correctness verification, E can be 1, 2, 8, 1024
    monitoring:
    cache_hit_count
    cache_miss_count
    cache_evict_count
    dirty_bytes_in_cache_count
    dirty_bytes_evicted_count
 */
#ifdef CACHE_SIMULATION_VERIFICATION
int cache_hit_count = 0;
int cache_miss_count = 0;
int cache_evict_count = 0;
int dirty_bytes_in_cache_count = 0;
int dirty_bytes_evicted_count = 0;

char trace_buf[20];
char *trace_ptr = (char *)&trace_buf;
#else
#define NUM_CACHE_LINE_PER_SET (8)
#endif

// write-back and write-allocate
typedef enum
{
    CACHE_LINE_INVALID,
    CACHE_LINE_CLEAN,
    CACHE_LINE_DIRTY
} sram_cacheline_state_t;

typedef struct
{
    sram_cacheline_state_t state;
    int time; // timer to find LRU line inside one set
    uint64_t tag;
    uint8_t block[1 << SRAM_CACHE_OFFSET_LENGTH];
} sram_cacheline_t;

typedef struct
{
    sram_cacheline_t lines[NUM_CACHE_LINE_PER_SET];
} sram_cacheset_t;

typedef struct
{
    sram_cacheset_t sets[(1 << SRAM_CACHE_INDEX_LENGTH)];
} sram_cache_t;

static sram_cache_t cache;

uint8_t sram_cache_read(uint64_t paddr_value)
{
    address_t paddr = {
        .paddr_value = paddr_value,
    };

    sram_cacheset_t *set = &cache.sets[paddr.CI];

    // update LRU time
    sram_cacheline_t *victim = NULL;
    sram_cacheline_t *invalid = NULL;
    int max_time = -1;

    for (int i = 0; i < NUM_CACHE_LINE_PER_SET; i++)
    {
        sram_cacheline_t *line = &(set->lines[i]);

        line->time++;
        if (max_time < line->time)
        {
            // select this line as victim by LRU policy
            // replace it when all lines are valid
            victim = line;
            max_time = line->time;
        }

        if (line->state == CACHE_LINE_INVALID)
        {
            // exist one invalid line as candidate for cache miss
            invalid = line;
        }
    }

    // try cache hit
    for (int i = 0; i < NUM_CACHE_LINE_PER_SET; i++)
    {
        sram_cacheline_t *line = &(set->lines[i]);
        if (line->state != CACHE_LINE_INVALID && line->tag == paddr.CT)
        {
#ifdef CACHE_SIMULATION_VERIFICATION
            sprintf(trace_buf, "hit");
            cache_hit_count++;
#endif
            // cache hit
            // update LRU
            line->time = 0;

            // find the byte
            return line->block[paddr.CO];
        }
    }

#ifdef CACHE_SIMULATION_VERIFICATION
    // cache miss: load from memory
    sprintf(trace_buf, "miss");
    cache_miss_count++;
#endif

    // try to find one free cache line
    if (invalid != NULL)
    {
#ifndef CACHE_SIMULATION_VERIFICATION
        // load data from DRAM to this invalid cache line
        bus_read_cacheline(paddr.paddr_value, invalid->block);
#endif
        // update cache line state
        invalid->state = CACHE_LINE_CLEAN;

        // update LRU
        invalid->time = 0;

        // update tag
        invalid->tag = paddr.CT;

        return invalid->block[paddr.CO];
    }

    // no free cache line, use LRU policy
    assert(victim != NULL);

    if (victim->state == CACHE_LINE_DIRTY)
    {
#ifndef CACHE_SIMULATION_VERIFICATION
        // write back to dirty line to DRAM
        // TODO 为什么不是victim->block
        bus_write_cacheline(paddr.paddr_value, victim->block);
#else
        dirty_bytes_evicted_count += (1 << SRAM_CACHE_OFFSET_LENGTH);
        dirty_bytes_in_cache_count -= (1 << SRAM_CACHE_OFFSET_LENGTH);
#endif
    }
#ifdef CACHE_SIMULATION_VERIFICATION
    // if CACHE_LINE_CLEAN discard this victim directly
    sprintf(trace_buf, "miss eviction");
    cache_evict_count++;
#endif
    // update state
    victim->state = CACHE_LINE_INVALID;
#ifndef CACHE_SIMULATION_VERIFICATION
    // read from DRAM
    // load data from DRAM to this invalid cache line
    bus_read_cacheline(paddr.paddr_value, &(victim->block));
#endif

    // update cache line state
    victim->state = CACHE_LINE_CLEAN;

    // update LRU
    victim->time = 0;

    // update tag
    victim->tag = paddr.CT;

    return victim->block[paddr.CO];
}

void sram_cache_write(uint64_t paddr_value, uint8_t data)
{
    address_t paddr = {
        .paddr_value = paddr_value,
    };

    sram_cacheset_t *set = &(cache.sets[paddr.CI]);

    // update LRU time
    sram_cacheline_t *victim = NULL;
    sram_cacheline_t *invalid = NULL; // for write-allocate
    int max_time = -1;

    for (int i = 0; i < NUM_CACHE_LINE_PER_SET; i++)
    {
         sram_cacheline_t *line = &(set->lines[i]);
         line->time++;

        if (max_time < line->time)
        {
            // select this line as victim by LRU policy
            // replace it when all lines are valid
            victim = line;
            max_time = line->time;
        }

        if (line->state == CACHE_LINE_INVALID)
        {
            // exist one invalid line as candidate for cache miss
            invalid = &(set->lines[i]);
        }
    }

    // try cache hit
    for (int i = 0; i < NUM_CACHE_LINE_PER_SET; i++)
    {
        sram_cacheline_t *line = &(set->lines[i]);

        if (line->state != CACHE_LINE_INVALID && line->tag == paddr.CT)
        {
#ifdef CACHE_SIMULATION_VERIFICATION
            // cache hit
            sprintf(trace_buf, "hit");
            cache_hit_count++;

            if (line->state == CACHE_LINE_DIRTY) 
            {
                dirty_bytes_in_cache_count += (1 << SRAM_CACHE_OFFSET_LENGTH);
            }
#endif
            // update LRU time
            line->time = 0;

            // find the byte
            line->block[paddr.CO] = data;

            // update state
            line->state = CACHE_LINE_DIRTY;

            return;
        }
    }
#ifdef CACHE_SIMULATION_VERIFICATION
    // cache miss: load from memory
    sprintf(trace_buf, "miss");
    cache_miss_count++;
#endif
    // write-allocate

    // try to find one free cache line
    if (invalid != NULL)
    {
#ifndef CACHE_SIMULATION_VERIFICATION
        // load data from DRAM to this invalid cache line
        bus_read_cacheline(paddr.paddr_value, invalid->block);
#else
        dirty_bytes_in_cache_count += (1 << SRAM_CACHE_OFFSET_LENGTH);
#endif
        // update cache line state
        invalid->state = CACHE_LINE_DIRTY;

        // update LRU
        invalid->time = 0;

        // update tag
        invalid->tag = paddr.CT;

        // write data
        invalid->block[paddr.CO] = data;

        return;
    }

    // no free cache line, use LRU policy
    assert(victim != NULL);

    if (victim->state == CACHE_LINE_DIRTY)
    {
#ifndef CACHE_SIMULATION_VERIFICATION
        // write back the dirty line to dram
        bus_write_cacheline(paddr.paddr_value, victim->block);
#else
        dirty_bytes_evicted_count += (1 << SRAM_CACHE_OFFSET_LENGTH);
        dirty_bytes_in_cache_count -= (1 << SRAM_CACHE_OFFSET_LENGTH);
#endif
    }
#ifdef CACHE_SIMULATION_VERIFICATION
    // if CACHE_LINE_CLEAN discard this victim directly
    sprintf(trace_buf, "miss eviction");
    cache_evict_count++;
    dirty_bytes_in_cache_count += (1 << SRAM_CACHE_OFFSET_LENGTH);
#endif
    // update state
    victim->state = CACHE_LINE_INVALID;
#ifndef CACHE_SIMULATION_VERIFICATION
    // read from dram
    // write-allocate
    // load data from DRAM to this invalid cache line
    bus_read_cacheline(paddr.paddr_value, victim->block);
#endif

    // update cache line state
    victim->state = CACHE_LINE_DIRTY;

    // update LRU
    victim->time = 0;

    // update tag
    victim->tag = paddr.CT;

    victim->block[paddr.CO] = data;
}

#ifdef CACHE_SIMULATION_VERIFICATION
void print_cache()
{
    for (int i = 0; i < (1 << SRAM_CACHE_INDEX_LENGTH); ++ i)
    {
        printf("set %x: [ ", i);

        sram_cacheset_t set = cache.sets[i];

        for (int j = 0; j < NUM_CACHE_LINE_PER_SET; ++ j)
        {
            sram_cacheline_t line = set.lines[j];

            char state;
            switch (line.state)
            {
            case CACHE_LINE_CLEAN:
                state = 'c';
                break;
            case CACHE_LINE_DIRTY:
                state = 'd';
                break;
            case CACHE_LINE_INVALID:
                state = 'i';
                break;            
            default:
                state = 'u';
                break;
            }

            printf("(%lx: %c, %d), ", line.tag, state, line.time);
        }

        printf("\b\b ]\n");
    }
}
#endif