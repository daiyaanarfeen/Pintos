#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include <stdbool.h>
#include "devices/block.h"
#include "filesys/off_t.h"

/* Initialize the cache */
void cache_init(void);

/* close the cache */
void cache_close(void);

/* Retrieves the index cache entry corresponding to the block sector, should it exist; otherwise, it returns -1. */
int cache_lookup(block_sector_t sector);

// /* Release the cache and its lock */
// void cache_release(block_sector_t sector, bool dirty);

 /* Create a new cache block and return its index. If all in used, return -1. */
int cache_create(void);

/* Reads data from a sector into the buffer, starting at the offset. Returns the number of bytes read.*/
// void cache_read(block_sector_t sector, void * buf, off_t offset, size_t length);
void cache_read(block_sector_t sector, void * buf);

void cache_read_many(block_sector_t sector, void * buf, off_t buf_ofs, off_t sector_ofs, size_t length);

/* Reads data from a buffer into a sector, starting at the offset. Returns the number of bytes written.*/
void cache_write(block_sector_t sector, const void * buf);

void cache_write_many(block_sector_t sector, const void * buf, off_t buf_ofs, off_t sector_ofs, size_t length);

#endif /* filesys/cache.h */
