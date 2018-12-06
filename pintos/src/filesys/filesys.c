#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{

  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  cache_init();
  inode_init ();
  free_map_init ();

  if (format)
    do_format ();

  free_map_open ();
  
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void)
{
  free_map_close ();
  cache_close();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_dir)
{
  if (name == NULL){
    return NULL;
  }
  block_sector_t inode_sector = 0;
  char *file_name = get_file_name_from_path(name);
  struct dir *dir = open_dir_by_path(name);
  bool success = (dir != NULL
                  && file_name != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, is_dir)
                  && dir_add (dir, file_name, inode_sector, is_dir));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  if (name == NULL){
    return NULL;
  }
  char *file_name = get_file_name_from_path(name);
  if (file_name == NULL){
    return NULL;
  }
  struct dir *dir = open_dir_by_path(name);
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, file_name, &inode);
  dir_close (dir);

  return file_open (inode);
}

/* Opens the inode with the given NAME.
   Returns the inode if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct inode *
filesys_open_inode (const char *name)
{
  if (name == NULL){
    return NULL;
  }
  if (strcmp(name, "/") == 0 && strlen(name) == 1){
    struct inode *root = dir_open_root();
    inode_set_is_dir(root, true);
    return root;
  }
  char *file_name = get_file_name_from_path(name);
  if (file_name == NULL){
    return NULL;
  }
  struct dir *dir = open_dir_by_path(name);
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, file_name, &inode);
  dir_close (dir);

  return inode;
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name)
{
  char *file_name = get_file_name_from_path(name);
  struct dir *dir = open_dir_by_path(name);
  bool success = dir != NULL && dir_remove (dir,file_name);
  dir_close (dir);
  return success;
}

/* Changes the current working directory of the process
   to dir, which may be relative or absolute. 
   Returns true if successful, false on failure. */
bool
filesys_chdir (const char *dir)
{
  if (dir == NULL) {
    return false;
  }

  if (strcmp(dir, "/") == 0 && strlen(dir) == 1){
    dir_close(thread_current()->cwd);
    thread_current()->cwd = dir_open_root();
    return true;
  }
  struct thread *cur = thread_current ();
  struct dir *chdir = open_dir_by_path(dir);
  
  char *name = get_file_name_from_path(dir);
  struct inode *inode = NULL;
  if (! dir_lookup(chdir, name, &inode)){
    dir_close(chdir);
    return false;
  }
  dir_close(thread_current()->cwd);
  thread_current()->cwd = dir_open(inode);
  return true;
}

/* Creates the directory named dir, which may be relative
   or absolute. Returns true if successful, false on failure. 
   Fails if dir already exists or if any directory name in dir, 
   besides the last, does not already exist. */
bool
filesys_mkdir (const char *name)
{
  /* borrowed from filesys_create, but need inode_sector later, so copied here */
  block_sector_t inode_sector = 0;
  int initial_size = 0;
  bool is_dir = true;
  char *file_name = get_file_name_from_path(name);
  struct dir *dir = open_dir_by_path(name);
  bool success = (dir != NULL
                  && file_name != NULL
                  && free_map_allocate (1, &inode_sector)
                  // start with 16 entries in dir
                  && dir_create (inode_sector, 16)
                  && dir_add (dir, file_name, inode_sector, is_dir));
  if (!success){
    if (inode_sector != 0){
      free_map_release (inode_sector, 1);
    }
    dir_close (dir);
    return false;
  }
  /*end of borrowing */
  struct dir *new_dir = dir_open(inode_open(inode_sector));
  success = dir_add(new_dir, ".", inode_sector, true)
            && dir_add(new_dir, "..", inode_get_inumber(dir_get_inode(dir)), true);
  dir_close(new_dir);
  if (!success){
    dir_remove(dir, file_name);
  }
  dir_close(dir);
  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  
  struct dir *dir = dir_open_root();
  dir_add (dir,".", ROOT_DIR_SECTOR, true);
  dir_add (dir,"..", ROOT_DIR_SECTOR, true);
  dir_close (dir);
  free_map_close ();
  printf ("done.\n");
}
