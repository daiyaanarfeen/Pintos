#include "filesys/cache.h"
#include <debug.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "devices/timer.h"
#include "filesys/off_t.h"

#define CACHE_SIZE 64
#define INVALID_SECTOR ((block_sector_t) -1)

static int hand;                          /* Clock hand */
struct cache_entry *entries[CACHE_SIZE];  /* Cache Entries */
static struct lock cache_lock;            /* A lock for using the cache */

struct cache_entry{
  block_sector_t sector;                  /* The sector this entry maps to */
  bool ref;                               /* Reference flag for the entry, initialized to true */
  bool dirty;                             /* Dirty flag for the entry */
  bool up_to_date;
  int use_count;

  struct condition block_cond;            /* control access to the entry */
  struct lock block_lock;
  struct lock data_lock;

  uint8_t data[BLOCK_SECTOR_SIZE];        /* A block of data */
};


/* Initialize the cache */
void cache_init(void){
  hand = 0;
  lock_init(&cache_lock);
  int i;
  for (i = 0; i < CACHE_SIZE; i ++) {
    struct cache_entry *entry = malloc(sizeof(struct cache_entry));
    entry -> sector = INVALID_SECTOR;
    entry -> ref = false;
    entry -> dirty = false;
    entry -> up_to_date = false;
    entry -> use_count = 0;
    
    cond_init(&entry->block_cond);
    lock_init(&entry->block_lock);
    lock_init(&entry->data_lock);

    entries[i] = entry;
  }
}

/* Close the cache */
void cache_close(void){
  int i;
  for (i = 0; i < CACHE_SIZE; i++) {
    /* If there's dirty block when we close the cache, write back. */
    if (entries[i]->dirty && entries[i]->up_to_date) {
      block_write (fs_device, entries[i]->sector, entries[i]->data);
      entries[i]->dirty = false;
    }
    free(entries[i]);
  }
}

/* Retrieves the index cache entry corresponding to the block sector, should it exist; otherwise, it returns -1. */
int cache_lookup(block_sector_t sector){


  if ((&cache_lock)->holder == thread_current ()) {
    ASSERT (false);
  }
  lock_acquire(&cache_lock);


  int i;
  for (i = 0; i < CACHE_SIZE; i++) {
    if (entries[i]->sector == sector) {


      if ((&entries[i]->block_lock)->holder == thread_current ()) {
    ASSERT (false);
  }
      lock_acquire(&entries[i]->block_lock);


      entries[i]->use_count += 1;
      lock_release(&cache_lock);
      return i;
    }
  }
  lock_release(&cache_lock);
  return -1;
}

int cache_create(void) {


  if ((&cache_lock)->holder == thread_current ()) {
    ASSERT (false);
  }
  lock_acquire(&cache_lock);


  /* Find the empty block first. */
  int i;
  for (i = 0; i < CACHE_SIZE; i++) {
    if (entries[i]->sector == INVALID_SECTOR) {


      if ((&entries[i]->block_lock)->holder == thread_current ()) {
    ASSERT (false);
  }
      lock_acquire(&entries[i]->block_lock);


      entries[i]->use_count += 1;
      lock_release(&cache_lock);
      return i;
    }
  }

  /* If there's no empty block, use clock algorithm to evict a block. */
  int base;
  for (base = 0; base < CACHE_SIZE * 2; base++) {
    if (entries[hand]->use_count > 0) {
      continue;
    }
    if (entries[hand]->ref == false) {
      entries[hand]->ref = true;
      hand = (hand + 1) % CACHE_SIZE;
      continue;
    }


    if ((&entries[hand]->block_lock)->holder == thread_current ()) {
    ASSERT (false);
  }
    lock_acquire(&entries[hand]->block_lock);


    entries[hand]->use_count += 1;
    lock_release(&cache_lock);

    /* If we are evicting a dirty block, write it back first. */
    if (entries[hand]->dirty && entries[hand]->up_to_date) {
      block_write (fs_device, entries[hand]->sector, entries[hand]->data);
    }
    return hand;
  }
  lock_release(&cache_lock);
  return -1;
}


void cache_read_many(block_sector_t sector, void * buf, off_t buf_ofs, off_t sector_ofs, size_t length){
  
  /* First find if the block is in the cache. */
  int i = cache_lookup(sector);
  /* If not in cache, create a new one. */
  if (i == -1) {
    i = cache_create();
    entries[i]->up_to_date = false;
  }

  entries[i]->sector = sector;
  entries[i]->ref = false;
  if (entries[i]->up_to_date == false) {
    block_read(fs_device, sector, entries[i]->data);
    entries[i]->dirty = false;
    entries[i]->up_to_date = true;
  }

  memcpy (buf + buf_ofs, entries[i]->data + sector_ofs, length);
  entries[i]->use_count -= 1;
  lock_release(&entries[i]->block_lock);
}

void cache_read(block_sector_t sector, void * buf) {
  cache_read_many(sector, buf, 0, 0, BLOCK_SECTOR_SIZE);
}

void cache_write_many(block_sector_t sector,const void * buf, off_t buf_ofs, off_t sector_ofs, size_t length){
  int i = cache_lookup(sector);
  if (i == -1) {
    i = cache_create();
  }
  entries[i]->sector = sector;
  entries[i]->ref = false;
  entries[i]->dirty = true;
  entries[i]->up_to_date = true;

  memcpy (entries[i]->data + sector_ofs, buf + buf_ofs, length);
  entries[i]->use_count -= 1;
  lock_release(&entries[i]->block_lock);
}

void cache_write(block_sector_t sector, const void * buf) {
  cache_write_many(sector, buf, 0, 0, BLOCK_SECTOR_SIZE);
}
