#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <list.h>
#include "filesys/file.h"
/* This struct is used to keep track of different level of I/O calls. */
struct fs_bundle{
	const char *filename;  /* The file name used to open the file */
	int fd;			/* The underlying file descriptor */
	struct file *file;		/* The underlying file struct */
	struct list_elem fs_elem; 	/* Element for the files list in thread.c */
	struct list_elem global_elem; /* Element for the file_global_list in syscall.c */
};

void syscall_init (void);

#endif /* userprog/syscall.h */
