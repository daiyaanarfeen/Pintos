#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <string.h>
#include <devices/input.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "filesys/directory.h"

/* We keep track of all the files that are open */
struct list all_files;

static void syscall_handler (struct intr_frame *);
void check_valid_ptr(const uint8_t *addr, int range, struct intr_frame *f UNUSED);
// void check_valid_ptr_with_lock(const uint8_t *addr, int range, struct intr_frame *f UNUSED, struct lock *lock);
void syscall_exit(int status, struct intr_frame *f);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  list_init(&all_files);
}

/* In case of bad pointer, other syscalls may need to call exit(-1) */
void syscall_exit(int status, struct intr_frame *f){
  /* Store status into frame */
  f->eax = status;
  /* check parent bundle, set status code */
  struct thread *cur = thread_current ();
  if (cur->parent) 
    {
      struct process_bundle *pb = cur->parent;
      lock_acquire (&pb->pb_lock);
      pb->status = status;
      lock_release (&pb->pb_lock);
    }
  printf("%s: exit(%d)\n", thread_current ()->name, status);
  thread_exit ();
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  int fd, size;
  char* buf;

  uint32_t* args = ((uint32_t*) f->esp);
  /* We use check valid pointer to make sure that every argument is valid */
  check_valid_ptr ((uint8_t*) args, 0, f);
  switch (args[0]){
    case SYS_EXIT:
      check_valid_ptr ((uint8_t*) args, 8, f);
      /* Call syscall exit function */
      syscall_exit (args[1], f);
      break;
    case SYS_HALT:
      check_valid_ptr ((uint8_t*) args, 8, f);
      /* Shutdown system */
      shutdown_power_off ();
      break;
    case SYS_WAIT:
      check_valid_ptr ((uint8_t*) args, 8, f);
      /* Call process wait */
      f->eax = process_wait (args[1]);
      break;
    case SYS_EXEC:
      check_valid_ptr ((uint8_t*) args, 8, f);
      /* Call process exec */
      f->eax = process_execute ((char *)args[1]);
      break;
    case SYS_PRACTICE:
      check_valid_ptr ((uint8_t*) args, 8, f);
      f->eax = args[1] + 1;
      break;
    /* System Call: bool create (const char *file, unsigned initial size) */
    case SYS_CREATE:
      ;
      const char *create_name = (const char *) args[1];
      check_valid_ptr ((uint8_t*) create_name, 0, f);
      if (!create_name)
        {
          f->eax = -1;
        } 
      else 
        {
          off_t initial_size = (off_t) args[2];
          f->eax = filesys_create (create_name, initial_size, false);	
        }
      break;
    /* System Call: bool remove (const char *file)  */
    case SYS_REMOVE:
      ;
      const char *remove_name = (const char *) args[1];
      /* cannot remove root */
      if (strcmp(remove_name, "/")==0 && strlen(remove_name) == 1){
        f->eax = false;
      } else{
        f->eax = filesys_remove (remove_name);
      }
      break;
    /* System Call: int open (const char *file) */
    case SYS_OPEN:
      check_valid_ptr ((uint8_t *) args, 8, f);
      const char *open_name = (const char *) args[1];
      check_valid_ptr ((uint8_t*) open_name, 0, f);
      struct inode* inode = filesys_open_inode (open_name);
      if (!inode){
        f->eax = -1;
      } else{
        struct file *fp = NULL;
        struct dir *dir = NULL;
        /* if we are trying to open a file */
        if (!inode_is_dir(inode)){
          fp = filesys_open(open_name);
        }
        else {
          dir = dir_open(inode);
        }
        if (fp || dir){
          struct thread *t = thread_current ();
          struct fs_bundle * fb = (struct fs_bundle *)malloc(sizeof(struct fs_bundle));
          char* copier = (char*) malloc (strlen(open_name) + 1);
          strlcpy (copier, open_name, strlen (open_name) + 1);
          fb->filename = copier;
          fb->fd = t->next_fd;
          t->next_fd ++;
          if (fp){
            fb->file = fp;
            fb->is_dir = false;
          } else{
            fb->dir = dir;
            fb->is_dir = true;
          }
          list_push_back (&t->files, &fb->fs_elem);
          list_push_back (&all_files, &fb->global_elem);
          f->eax = fb->fd;
        } else{
          syscall_exit(-1, f);
        }
      }
      break;
    /* System Call: int filesize (int fd) */
    case SYS_FILESIZE:
      ;
      struct list_elem *e;
      fd = (int) args[1];
      struct fs_bundle * fb = NULL;
      for (e = list_begin (&thread_current ()->files); e != list_end (&thread_current ()->files); 
           e = list_next (e))
        {
          fb = list_entry(e, struct fs_bundle, fs_elem);
          if (fb->fd == fd){
            break;
          }  
        } 
      size = 0;
      if (fb)
        {
          size = (int) file_length (fb->file);
        }
      f->eax = size;
      break;
    case SYS_READ:    
      fd = (int)args[1];
      buf = (char*)args[2];
      size = (unsigned)args[3];
      check_valid_ptr ((uint8_t*) buf, 0, f);
      if(fd == 0) 
        {
          /* read from stdin */
          int i;
          for (i = 0; i < size; i++) {
            *(buf + i) = input_getc ();
          }
          f->eax = i; 
        } 
      else if (fd == 1) 
        {
          syscall_exit (-1, f);
        } 
      else 
        {
          struct list_elem *e;
          struct fs_bundle * fb = NULL;
          /* find the fs_bundle of the file being read from */
        	for (e = list_begin (&thread_current ()->files); e != list_end (&thread_current ()->files); 
              e = list_next (e))
            {
              fb = list_entry (e, struct fs_bundle, fs_elem);
              if (fb->fd == fd) 
                {
                  break;
                }
            }
          if (e == list_end (&thread_current ()->files))
            {
            }
          /* should not read a directory */
          if (fb->is_dir){
            syscall_exit(-1, f);
          }
          size = file_read (fb->file, buf, size);
          f->eax = size; 
        }
      break;
    case SYS_WRITE: 
      fd = (int)args[1];
      buf = (char*)args[2];
      size = (unsigned)args[3];
      check_valid_ptr ((uint8_t*) buf, 0, f);
      if (fd == 1) 
        {
          /* write to stdout */
          putbuf((char*)args[2], (size_t)args[3]);
        } 
      else if (fd == 0) 
        {
          syscall_exit (-1, f);
        } 
      else 
        {
          /* find the fs_bundle of the file being written to*/
          struct list_elem *e;
          struct fs_bundle * fb = NULL;
          for (e = list_begin (&thread_current ()->files); e != list_end (&thread_current ()->files); 
               e = list_next (e))
            {
              fb = list_entry (e, struct fs_bundle, fs_elem);
              if (fb->fd == fd) 
                {
                  break;
                }
            }
          if (e == list_end(&thread_current ()->files))
            syscall_exit(-1, f);
          /* should not write to directory */
          if (fb->is_dir){
            syscall_exit(-1, f);
          }
          struct thread* t;
          /* rox implementation, check to see the file is not the executable of a running process */
          int deny = 0;
          for (e = list_begin (all_list_ptr); e != list_end (all_list_ptr); e = list_next (e)) 
            {
              t = list_entry (e, struct thread, allelem);
              if (strcmp (t->name, fb->filename) == 0)
                deny = 1;
            }
          if (deny) 
            {
              f->eax = 0;
            } 
          else
            {
              size = file_write (fb->file, buf, size);
              f->eax = size;
            }
        }
      break;
    case SYS_SEEK:
      fd = (int)args[1];
      size = (off_t)args[2];
      struct list_elem * seek_e;
      struct fs_bundle * seek_fb = NULL;
      for (seek_e = list_begin (&thread_current ()->files); seek_e != list_end (&thread_current ()->files); 
        seek_e = list_next (seek_e)) 
        { 
          seek_fb = list_entry (seek_e, struct fs_bundle, fs_elem);
          if (seek_fb->fd == fd) 
            {
              break;
            }
        }
      if (seek_e == list_end (&thread_current ()->files))
        {
          syscall_exit (-1, f);
        }
      file_seek (seek_fb->file, size);
      break;        
    case SYS_TELL: 
      fd = (int)args[1];
      struct list_elem *tell_e;
      struct fs_bundle * tell_fb = NULL;
      for (tell_e = list_begin (&thread_current ()->files); tell_e != list_end (&thread_current ()->files); 
           tell_e = list_next (tell_e))
        {         
          tell_fb = list_entry (tell_e, struct fs_bundle, fs_elem);
          if (tell_fb->fd == fd) 
            {
              break;
            }
        }
      if (tell_e == list_end (&thread_current ()->files))
        {
          syscall_exit(-1, f);
        }
      f->eax = file_tell (tell_fb->file);
      break;          
    case SYS_CLOSE:
      fd = (int)args[1];
      if (fd == 0 || fd == 1) 
        {
          syscall_exit (-1, f);
        } 
      else 
      {
        struct list_elem *e;
        struct fs_bundle * fb = NULL;
        /* find the file being closed */
        for (e = list_begin (&thread_current ()->files); e != list_end (&thread_current ()->files); e = list_next (e))
          {
            fb = list_entry (e, struct fs_bundle, fs_elem);
            if (fb->fd == fd) 
              {
                break;
              }
          }
        if (e == list_end (&thread_current ()->files))
          {
            syscall_exit (-1, f);
          }
        /* close the file, free the fs_bundle */
        file_close (fb->file);
        free ((void*)fb->filename);
        list_remove (e); 
        list_remove (e);
        free (fb);
      }
      break;

    case SYS_CHDIR:
      check_valid_ptr ((uint8_t*) args, 0, f);
      const char *chdir_dir = (const char *) args[1];
      check_valid_ptr ((uint8_t*) args, strlen(chdir_dir), f);
      f->eax = filesys_chdir (chdir_dir);
      break;

    case SYS_MKDIR:
      check_valid_ptr ((uint8_t*) args, 0, f);
      const char *mkdir_dir = (const char *) args[1];
      check_valid_ptr ((uint8_t*) args, strlen(mkdir_dir), f);
      f->eax = filesys_mkdir (mkdir_dir);
      break;

    case SYS_READDIR:
      check_valid_ptr ((uint8_t*) args, 0, f);
      fd = (int) args[1];
      char *readdir_name = (char *) args[2];
      check_valid_ptr ((uint8_t*) args, strlen(readdir_name) + sizeof(int), f);
      struct list_elem *readdir_e;
      struct fs_bundle * readdir_fb = NULL;
      for (readdir_e = list_begin (&thread_current ()->files); readdir_e != list_end (&thread_current ()->files); 
           readdir_e = list_next (readdir_e))
        {         
          readdir_fb = list_entry (readdir_e, struct fs_bundle, fs_elem);
          if (readdir_fb->fd == fd) 
            {
              break;
            }
        }
      if (readdir_e == list_end (&thread_current ()->files))
        {
          // lock_release(&file_global_lock);
          syscall_exit(-1, f);
        }
      if (! readdir_fb->is_dir){
        // lock_release(&file_global_lock);
        syscall_exit(-1, f);
      }
      f->eax = dir_readdir(readdir_fb->dir, readdir_name);
      break;
    case SYS_ISDIR:
      check_valid_ptr ((uint8_t*) args, 0, f);
      fd = (int) args[1];
      struct list_elem *isdir_e;
      struct fs_bundle * isdir_fb = NULL;
      for (isdir_e = list_begin (&thread_current ()->files); isdir_e != list_end (&thread_current ()->files); 
           isdir_e = list_next (isdir_e))
        {         
          isdir_fb = list_entry (isdir_e, struct fs_bundle, fs_elem);
          if (isdir_fb->fd == fd) 
            {
              break;
            }
        }
      if (isdir_e == list_end (&thread_current ()->files))
        {
          // lock_release(&file_global_lock);
          syscall_exit(-1, f);
        }
      f->eax = isdir_fb->is_dir;
      break;
    case SYS_INUMBER:
      check_valid_ptr ((uint8_t*) args, 0, f);
      fd = (int) args[1];
      struct list_elem *inum_e;
      struct fs_bundle * inum_fb = NULL;
      for (inum_e = list_begin (&thread_current ()->files); inum_e != list_end (&thread_current ()->files); 
           inum_e = list_next (inum_e))
        {         
          inum_fb = list_entry (inum_e, struct fs_bundle, fs_elem);
          if (inum_fb->fd == fd) 
            {
              break;
            }
        }
      if (inum_e == list_end (&thread_current ()->files))
        {
          // lock_release(&file_global_lock);
          syscall_exit(-1, f);
        }
      struct inode *inum_inode;
      if (inum_fb->is_dir){
        inum_inode = dir_get_inode(inum_fb->dir);
      } else{
        inum_inode = file_get_inode(inum_fb->file);
      }
      f->eax = inode_get_inumber(inum_inode);
      break;
  }
}
    case SYS_CHDIR:
      check_valid_ptr ((uint8_t*) args, 0, f);
      const char *chdir_dir = (const char *) args[1];
      check_valid_ptr ((uint8_t*) args, strlen(chdir_dir), f);
      f->eax = filesys_chdir (chdir_dir);
      break;

    case SYS_MKDIR:
      check_valid_ptr ((uint8_t*) args, 0, f);
      const char *mkdir_dir = (const char *) args[1];
      check_valid_ptr ((uint8_t*) args, strlen(mkdir_dir), f);
      f->eax = filesys_mkdir (mkdir_dir);
      break;

    case SYS_READDIR:
      check_valid_ptr ((uint8_t*) args, 0, f);
      fd = (int) args[1];
      char *readdir_name = (char *) args[2];
      check_valid_ptr ((uint8_t*) args, strlen(readdir_name) + sizeof(int), f);
      struct list_elem *readdir_e;
      struct fs_bundle * readdir_fb = NULL;
      for (readdir_e = list_begin (&thread_current ()->files); readdir_e != list_end (&thread_current ()->files); 
           readdir_e = list_next (readdir_e))
        {         
          readdir_fb = list_entry (readdir_e, struct fs_bundle, fs_elem);
          if (readdir_fb->fd == fd) 
            {
              break;
            }
        }
      if (readdir_e == list_end (&thread_current ()->files))
        {
          syscall_exit(-1, f);
        }
      if (! readdir_fb->is_dir){
        syscall_exit(-1, f);
      }
      f->eax = dir_readdir(readdir_fb->dir, readdir_name);
      break;
    case SYS_ISDIR:
      check_valid_ptr ((uint8_t*) args, 0, f);
      fd = (int) args[1];
      struct list_elem *isdir_e;
      struct fs_bundle * isdir_fb = NULL;
      for (isdir_e = list_begin (&thread_current ()->files); isdir_e != list_end (&thread_current ()->files); 
           isdir_e = list_next (isdir_e))
        {         
          isdir_fb = list_entry (isdir_e, struct fs_bundle, fs_elem);
          if (isdir_fb->fd == fd) 
            {
              break;
            }
        }
      if (isdir_e == list_end (&thread_current ()->files))
        {
          syscall_exit(-1, f);
        }
      f->eax = isdir_fb->is_dir;
      break;
    case SYS_INUMBER:
      check_valid_ptr ((uint8_t*) args, 0, f);
      fd = (int) args[1];
      struct list_elem *inum_e;
      struct fs_bundle * inum_fb = NULL;
      for (inum_e = list_begin (&thread_current ()->files); inum_e != list_end (&thread_current ()->files); 
           inum_e = list_next (inum_e))
        {         
          inum_fb = list_entry (inum_e, struct fs_bundle, fs_elem);
          if (inum_fb->fd == fd) 
            {
              break;
            }
        }
      if (inum_e == list_end (&thread_current ()->files))
        {
          syscall_exit(-1, f);
        }
      struct inode *inum_inode;
      if (inum_fb->is_dir){
        inum_inode = dir_get_inode(inum_fb->dir);
      } else{
        inum_inode = file_get_inode(inum_fb->file);
      }
      f->eax = inode_get_inumber(inum_inode);
      break;
  }
}

/* checks if the region of memeory from addr to addr+range is mapped and part of user memory, if it is not then it kills the process */ 
void check_valid_ptr (const uint8_t *addr, int range, struct intr_frame *f UNUSED){
  if (!is_user_vaddr (addr) || !is_user_vaddr (addr+range) || 
      !pagedir_get_page (thread_current ()->pagedir, addr))
    {
      syscall_exit (-1, f);
    }
}
