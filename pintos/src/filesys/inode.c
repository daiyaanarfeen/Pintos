#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include <stdio.h>

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    bool is_dir;
    block_sector_t direct[123];
    block_sector_t indirect;
    block_sector_t doubly_indirect;
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    char unused[3];
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
    bool is_dir;

    struct lock deny_lock;
    struct condition write_allowed;
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{
  ASSERT (inode != NULL);
  struct inode_disk disk_inode;
  block_read(fs_device, inode->sector, &disk_inode); 
  if (pos < disk_inode.length) {
    size_t sector = bytes_to_sectors(pos);
    if (sector <= 123)
      return disk_inode.direct[sector - 1];
    if (sector <= 251) {
      block_sector_t indirect[128];
      block_read(fs_device, disk_inode.indirect, indirect);
      return indirect[sector - 124];
    }
    if (sector > 251) {
      block_sector_t doubly_indirect[128];
      block_read(fs_device, disk_inode.doubly_indirect, doubly_indirect);
      block_sector_t middle_man = doubly_indirect[DIV_ROUND_UP(DIV_ROUND_UP(sector - 251, 128), 128) - 1];
      block_sector_t indirect_block[128];
      block_read(fs_device, middle_man, indirect_block);
      return indirect_block[(sector - 251 - 1) % 128];
    }
  }
  else
    return -1;
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
    block_write(fs_device, alloced[i], zeros);
    i += 1;
  }
  if (strict && i < sectors) {
    for (i--; i > -1; i--)
      free_map_release(alloced + i, 1);
  }
  return i;
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      size_t total_to_alloc = sectors;
      if (sectors > 123)
        total_to_alloc++; //indirect block
      if (sectors > 251) {
        total_to_alloc++; //doubly indirect block
        total_to_alloc += DIV_ROUND_UP(sectors - 251, 128); //doubly indirect data blocks
        total_to_alloc += DIV_ROUND_UP(DIV_ROUND_UP(sectors - 251, 128), 128); //middle-men
      }
      block_sector_t alloced[total_to_alloc];
      if (allocate_sectors(total_to_alloc, alloced, true))
        {
          char block_buf[BLOCK_SECTOR_SIZE] = "";
          if (sectors <= 123) 
            memcpy(disk_inode->direct, alloced, sectors * sizeof(block_sector_t));
          if (sectors > 123 && sectors <= 251) {
            memcpy(disk_inode->direct, alloced, 123 * sizeof(block_sector_t));
            disk_inode->indirect = alloced[123];
            memcpy(block_buf, alloced + 124, (total_to_alloc - 124) * sizeof(block_sector_t));
            block_write(fs_device, disk_inode->indirect, block_buf);
          }
          if (sectors > 251) {
            memcpy(disk_inode->direct, alloced, 123 * sizeof(block_sector_t));
            disk_inode->indirect = alloced[123];
            memcpy(block_buf, alloced + 124, 128 * sizeof(block_sector_t));
            block_write(fs_device, disk_inode->indirect, block_buf);
            disk_inode->doubly_indirect = alloced[252];
            size_t num_middle_men = DIV_ROUND_UP(DIV_ROUND_UP(sectors - 251, 128), 128);
            memset(block_buf, 0, BLOCK_SECTOR_SIZE);
            memcpy(block_buf, alloced + 253, num_middle_men * sizeof(block_sector_t));
            block_write(fs_device, disk_inode->doubly_indirect, block_buf);
            size_t remaining_blocks = sectors - 251;
            for (int i = 0; i < num_middle_men; i++) {
              block_sector_t middle_man = alloced[253 + i];
              memset(block_buf, 0, BLOCK_SECTOR_SIZE);
              if (remaining_blocks => 128)
                memcpy(block_buf, alloced + 253 + num_middle_men + i * 128, 128 * sizeof(block_sector_t));
                block_write(fs_device, middle_man, block_buf);
                remaining_blocks -= 128;
              if (remaining_blocks < 128)
                memcpy(block_buf, alloced + 253 + num_middle_men + i * 128, remaining_blocks * sizeof(block_sector_t));
                block_write(fs_device, middle_man, block_buf);
                remaining_blocks -= remaining_blocks;
            }
          }

          block_write (fs_device, sector, disk_inode);
          /* if (sectors > 0)
            {
              static char zeros[BLOCK_SECTOR_SIZE];
              size_t i;

              for (i = 0; i < sectors; i++)
                block_write (fs_device, disk_inode->start + i, zeros);
            } */
          success = true;
        }
      free (disk_inode);
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
  block_read (fs_device, inode->sector, &disk_inode);
  inode->is_dir = disk_inode->is_dir;
  lock_init(&inode->lock);
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
          free_map_release (inode->sector, 1);
          free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length));
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
  uint8_t *bounce = NULL;

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

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL)
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

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
  uint8_t *bounce = NULL;

  lock_acquire(&inode->deny_lock);
  if (inode->deny_write_cnt)
    cond_wait(&inode->write_allowed, &inode->deny_lock);
  lock_release(&inode->deny_lock);

  struct inode_disk disk_inode;

  if (size + offset > inode_length(inode)) {
    lock_acquire(inode->lock);
    size_t num_sectors = bytes_to_sectors(inode_length(inode));
    size_t sectors_needed = bytes_to_sectors(size + offset);

    while (sectors_needed > 0) {
      if (num_sectors < 123) {

      }
      if (num_sectors >= 123 && num_sectors < 251) {

      }
      if (num_sectors >= 251) {

      }
    }
  }

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

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else
        {
          /* We need a bounce buffer. */
          if (bounce == NULL)
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left)
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  inode->deny_write_cnt++;
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
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  struct inode_disk disk_inode;
  block_read(fs_device, inode->sector, &disk_inode);
  return disk_inode->length;
}
