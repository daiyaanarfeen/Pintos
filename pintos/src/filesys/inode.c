#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include <stdio.h>
#include "filesys/cache.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE=512 bytes long. */
struct inode_disk
  {
    block_sector_t direct[123];
    block_sector_t indirect;
    block_sector_t doubly_indirect;
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[124];               /* Not used. */
    char temp[3];
    bool is_dir;                        /* Whether the inode is dir or file. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode
  {
    struct lock lock;                   /* Synch for changing inode values (open_cnt, removed) */
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */

    struct lock deny_lock;
    struct condition write_allowed;
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    bool is_dir;                        /* Whether the inode is dir or file. */
    struct lock dir_lock;               /* Lock for directory */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{
  ASSERT (inode != NULL);
  struct inode_disk* disk_inode = malloc(sizeof(struct inode_disk));;
  cache_read(inode->sector, disk_inode); 
  block_sector_t sect;
  if (pos < disk_inode->length) {
    size_t sector = pos % 512  == 0 ? bytes_to_sectors(pos) + 1 : bytes_to_sectors(pos);
    if (sector <= 123) {
      sect = disk_inode->direct[sector - 1];
    } else if (sector <= 251) {
      block_sector_t indirect[128];
      cache_read(disk_inode->indirect, indirect);
      sect = indirect[sector - 124];
    } else {
      block_sector_t block[128];
      cache_read(disk_inode->doubly_indirect, block);
      block_sector_t middle_man = block[DIV_ROUND_UP(sector - 251, 128) - 1];
      cache_read(middle_man, block);
      sect = block[((sector - 251) % 128) - 1];
    }
    free(disk_inode);
    return sect;
  } else {
    return -1;
  }
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
}

/* Noncontiguous allocation of blocks */
size_t
allocate_sectors(size_t sectors, block_sector_t* alloced, bool strict)
{
  size_t i = 0;
  static char zeros[BLOCK_SECTOR_SIZE];
  while (i < sectors && free_map_allocate(1, alloced + i)) {
    cache_write(alloced[i], zeros);
    i += 1;
  }
  if (strict && i < sectors) {
    int j;
    for (j = i - 1; j >= 0; j--)
      free_map_release(alloced[j], 1);
  }
  return i;
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool is_dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;
  static char zeros[BLOCK_SECTOR_SIZE];
  cache_write(sector, zeros);

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  printf("%d", sizeof *disk_inode);
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      disk_inode->length = 0;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->is_dir = is_dir;
      cache_write(sector, disk_inode);
      free(disk_inode);
      struct inode* inode = inode_open(sector);
      char* buffer;
      buffer = calloc(length, 1);
      int size = inode_write_at(inode, buffer, length, 0);
      if (size == length) {
        success = true;
      } else {
        inode_remove(inode);
      }
      inode_close(inode);
      free(buffer);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector)
        {
          inode_reopen (inode);
          return inode;
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  struct inode_disk disk_inode;
  cache_read (inode->sector, &disk_inode);
  inode->is_dir = disk_inode.is_dir;
  lock_init(&inode->dir_lock);
  lock_init(&inode->lock);
  lock_init(&inode->deny_lock);
  cond_init(&inode->write_allowed);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed)
        {
          int block = 0;
          int total = bytes_to_sectors(inode_length(inode));
          struct inode_disk disk_inode;
          cache_read(inode->sector, &disk_inode);
          while (block < total) {
            if (block < 123) {
              free_map_release(disk_inode.direct[block], 1);
              block += total > 123 ? 123 : total;
            } else if (block >= 123 && block < 251) {
              block_sector_t ind[128];
              cache_read(disk_inode.indirect, ind);
              int i;
              for (i = 0; i < (total - 123 < 128 ? total - 123 : 128); i++)
                free_map_release(ind[i], 1);
              block += (total - 123 < 128 ? total - 123 : 128);
              free_map_release(disk_inode.indirect, 1);
            } else if (block >= 251) {
              block_sector_t dbl_ind[128];
              cache_read(disk_inode.doubly_indirect, dbl_ind);
              int mdl_man;
              for (mdl_man = 0; mdl_man < (DIV_ROUND_UP(total - 251, 128)); mdl_man++) {
                block_sector_t mdl_block[128];
                cache_read(dbl_ind[mdl_man], mdl_block);
                int i;
                for (i = 0; i < (total - block > 128 ? 128 : total - block); i++)
                  free_map_release(mdl_block[i], 1);
                block += (total - block > 128 ? 128 : total - block);
                free_map_release(dbl_ind[mdl_man], 1);
              }
              free_map_release(disk_inode.doubly_indirect, 1);
            }
          }
          free_map_release (inode->sector, 1);
        }

      free (inode);
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;

  while (size > 0)
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      cache_read_many(sector_idx, buffer, bytes_read, sector_ofs, chunk_size);

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  /*free (bounce);*/

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;

  lock_acquire(&inode->deny_lock);
  if (inode->deny_write_cnt)
    cond_wait(&inode->write_allowed, &inode->deny_lock);
  lock_release(&inode->deny_lock);

  lock_acquire(&inode->lock);
  if (size + offset > inode_length(inode)) {
    size_t cur_data_sectors = bytes_to_sectors(inode_length(inode));
    size_t cur_total_sectors = cur_data_sectors <= 123 ? cur_data_sectors : (cur_data_sectors <= 251 ? cur_data_sectors + 1 : cur_data_sectors + 2 + DIV_ROUND_UP(cur_data_sectors - 251, 128));
    size_t need_data_sectors = bytes_to_sectors(size + offset);
    size_t need_total_sectors = need_data_sectors <= 123 ? need_data_sectors : (need_data_sectors <= 251 ? need_data_sectors + 1 : need_data_sectors + 2 + DIV_ROUND_UP(need_data_sectors - 251, 128));
    size_t to_alloc = need_total_sectors - cur_total_sectors;
    block_sector_t* new_blocks = malloc(to_alloc * sizeof(block_sector_t));
    size_t num_new_blocks = allocate_sectors(to_alloc, new_blocks, false);
    
    struct inode_disk* disk_inode = malloc(sizeof(struct inode_disk));;
    cache_read(inode->sector, disk_inode);
    size_t blocks_added = 0;
    while (blocks_added < num_new_blocks) {
      if (cur_data_sectors < 123) {
        memcpy(disk_inode->direct + cur_data_sectors, new_blocks, (num_new_blocks < 123 - cur_data_sectors ? num_new_blocks : 123 - cur_data_sectors) * sizeof(block_sector_t));
        size_t iter_blocks = (num_new_blocks < 123 - cur_data_sectors ? num_new_blocks : 123 - cur_data_sectors);
        cur_data_sectors += iter_blocks;
        blocks_added += iter_blocks;
      } else if (cur_data_sectors >= 123 && cur_data_sectors < 251) {
        if (disk_inode->indirect == 0) {
          if (num_new_blocks - blocks_added > 1) {
            disk_inode->indirect = new_blocks[blocks_added];
            blocks_added += 1;
          } else {
            free_map_release(new_blocks[blocks_added], 1);
            blocks_added = num_new_blocks;
          }
        } 
        block_sector_t indirect_block[128];
        cache_read(disk_inode->indirect, indirect_block);
        size_t space = 128 - (cur_data_sectors - 123);
        memcpy(indirect_block + cur_data_sectors - 123, new_blocks + blocks_added, (num_new_blocks - blocks_added < space ? num_new_blocks - blocks_added : space) * sizeof(block_sector_t));
        cache_write(disk_inode->indirect, indirect_block);
        cur_data_sectors += (num_new_blocks - blocks_added < space ? num_new_blocks - blocks_added : space);
        blocks_added += (num_new_blocks - blocks_added < space ? num_new_blocks - blocks_added : space);
      } else if (cur_data_sectors >= 251) {
        if (disk_inode->doubly_indirect == 0) {
          if (num_new_blocks - blocks_added > 1) {
            disk_inode->doubly_indirect = new_blocks[blocks_added];
            blocks_added += 1;
          } else {
            free_map_release(new_blocks[blocks_added], 1);
            blocks_added = num_new_blocks;
          }
        }
        block_sector_t dbl_ind_block[128];
        cache_read(disk_inode->doubly_indirect, dbl_ind_block);
        size_t mdl_man = DIV_ROUND_UP(cur_data_sectors - 251, 128) == 0 ? 0 : ((cur_data_sectors - 251) % 128 == 0 ? DIV_ROUND_UP(cur_data_sectors - 251, 128) : DIV_ROUND_UP(cur_data_sectors - 251, 128) - 1);
        while (blocks_added < num_new_blocks && mdl_man < 128) {
          block_sector_t middle_man[128];
          if (dbl_ind_block[mdl_man] == 0) {
            if (num_new_blocks - blocks_added > 1) {
              dbl_ind_block[mdl_man] = new_blocks[blocks_added];
              blocks_added += 1;
            } else {
              blocks_added = num_new_blocks;
              break;
            }
          }
          cache_read(dbl_ind_block[mdl_man], middle_man);
          memcpy(middle_man + ((cur_data_sectors - 251) % 128) , new_blocks + blocks_added, (128 - ((cur_data_sectors - 251) % 128)) * sizeof(block_sector_t));
          cache_write(dbl_ind_block[mdl_man], middle_man);
          size_t iter_blocks = (128 - ((cur_data_sectors - 251) % 128));
          cur_data_sectors += iter_blocks;
          blocks_added += iter_blocks;
          mdl_man += 1;
        }
        cache_write(disk_inode->doubly_indirect, dbl_ind_block);
      }
    }
    disk_inode->length = cur_data_sectors * BLOCK_SECTOR_SIZE;
    cache_write(inode->sector, disk_inode);
    free(disk_inode);
    free(new_blocks);
    size = cur_data_sectors < need_data_sectors ? inode_length(inode) - offset : size;
  }
  lock_release(&inode->lock);

  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      cache_write_many(sector_idx, buffer, bytes_written, sector_ofs, chunk_size);

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  lock_acquire(&inode->deny_lock);
  inode->deny_write_cnt++;
  lock_release(&inode->deny_lock);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  lock_acquire(&inode->deny_lock);
  inode->deny_write_cnt--;
  if (inode->deny_write_cnt == 0)
    cond_broadcast(&inode->write_allowed, &inode->deny_lock);
  lock_release(&inode->deny_lock);
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  struct inode_disk disk_inode;
  cache_read(inode->sector, &disk_inode);
  return disk_inode.length;
}

bool
inode_is_dir(struct inode *inode)
{
  return inode->is_dir;
}

void
inode_set_is_dir(struct inode *inode, bool is_dir)
{
  inode->is_dir = is_dir;
}

void
lock_dir(struct inode *inode)
{
  lock_acquire(&inode->dir_lock);
}

void
unlock_dir(struct inode *inode)
{
  lock_release(&inode->dir_lock);
}

int 
inode_open_cnt(struct inode *inode)
{
  return inode->open_cnt;
}
