Design Document for Project 3: File System
==========================================

## Group Members

* Daiyaan Arfeen <daiyaanarfeen@berkeley.edu>
* Patrick Lu <xingyulu0701@berkeley.edu>
* Claire Liu <liuzmin@berkeley.edu>
* William Zhang <qiaowei@berkeley.edu>

## Task 1 Buffer Cache
### 1. Data Structure and Functions

- In filesys.h:
```
struct ClockCache{
	size_t hand;		                 /* Clock hand */
	struct CacheEntry[64];                         /* Cache Entries */
	struct lock cache_lock;		   /* A lock for using the cache */
	struct condition cache_queue;		   /* A queue for the cache if all entries are in use */
	int use_count[64];		   /* Counts to track how many process are using each cache entry */
}
struct CacheEntry{
	block_sector_t sector;			 /* The sector this entry maps to */
	char data[BLOCK_SECTOR_SIZE]	 /* A block of data */
	bool ref;				 /* Reference flag for the entry, initialized to true */
	bool dirty;				 /* Dirty flag for the entry */
	
	struct condition wait_queue;		 /* A queue to control access to the entry */
}

/* Retrieves the index cache entry corresponding to the block sector, should it exist; otherwise, it returns -1. */
int cache_lookup(block_sector_t sector);

/* Reads a block of data into the cache. It returns the index of the cache entry. This function may cause the thread to be put to sleep if all cache entries are being used */
int cache_add(block_sector_t sector);

/* Find the cache block to pop out using clock policy */
int cache_pop(void);

/* Reads data from a sector into the buffer, starting at the offset. Returns the number of bytes read.*/
size_t cache_read(void * buf, off_t offset, size_t length);

/* Reads data from a buffer into a sector, starting at the offset. Returns the number of bytes written.*/
size_t cache_write(void * buf, off_t offset, size_t length);
```
- In inode.c:
```
/* The following functions will be modified to use the cache. Alternatively, we may only modify block_read
and block_write*/
off_t inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset);
off_t inode_write_at (struct inode *inode, const void *buffer_, off_t size, off_t offset);
bool inode_create (block_sector_t sector, off_t length);
void inode_close (struct inode *inode);
off_t inode_length (const struct inode *inode);
```

### 2. Algorithms
The cache is declared as a static variable, since its size is fixed. Whenever a disk operation (e.g. in `inode_read_at`) on a particular sector occurs, instead of directly reading the disk, the process should first call `cache_lookup()` to check if the entry is inside the cache; if it is not, it should then call `cache_add()` to read data from the disk into the cache. Finally, it retrieve the data into the buffer using `cache_read()`.

In `cache_lookup()`, we first acquire the `cache_lock` and iterate through all cache entries (there are only 64 entries so the runtime is constant), and return the index of the entry if it maps to the sector we are looking for before releasing the lock; if there is no such entry, we call `cache_add()`  and return the index before releasing the lock.

In `cache_add()`, the current process should already be holding the `cache_lock`. It should first check that there are unused cache entries (by checking if any `use_count` is 0; otherwise, it should be put in the `cache_queue`, thereby atomically releasing the `cache_lock`, to wait for at least one unused entry, at which point it will be woken by `cond_signal`). Then, it should call `cache_pop` to select an entry for replacement. Finally, it should read data from the disk into the selected entry by calling `block_read` and then release the `cache_lock`. 

In `cache_pop()`, the current process should already be holding the `cache_lock`. It should start from the `hand` index and repeat the following: if the current block is in use (`use_count` for the entry is greater than 0), skip it; if the current block is unused without reference bit, set the reference bit and continue; if the current block is unused with reference bit, and if it has dirty bit, write back the block to disk first. Then we pop out the block and return its index.

In `cache_read()`, we first acquire the `cache_lock`. We check the `use_count` of the cache entry, and if it is non-zero, we increment the count by 1 before joining the `wait_queue`; otherwise, we increase count by 1, release the lock, and start copying data into the buffer. After we are done, we acquire `cache_lock` again, decrease count by 1, and call `cond_signal` on `wait_queue` if count is greater than 0; otherwise, we call `cond_signal` on `cache_queue` in case there is a process waiting for an entry to free up.

`cache_write()` is similar to `cache_read()`, except we write to the entry and set `dirty` to true.

### 3. Synchronization
This task has two central synchronization issues. The first issue is when a process wants to read a new block of data into the cache, it should not replace entries being used by other processes. As a result, we use `use_count` to keep track of which caches are being used, and use a conditional variable to put a thread to sleep should all entries be busy at the moment. 

The second issue is that multiple process should not be access the same cache entry. We decided to use another conditional variable, which should put a thread to sleep if the `use_count` for that entry is greater than 0, meaning other processes are using that cache entry. This allows us to achieve mutual exclusion without adding additional lock or semaphores, and to keep track of `use_count` correctly.

Lastly, we maintain a `cache_lock` for accessing any meta information of the cache, including `use_count` and the `sector` of each cache entry. 

### 4. Rationale
Our goal for task 1 is to balance efficiency and simplicity. The implementation of a cache contains a lot of moving parts, many of which have potential synchronization issues. As a result, we decided to break down the use of a cache into three simple logical steps: looking up the entry in cache, adding an entry to cache, and reading from/writing to cache. This should make our design conceptually easier, and help us debug the code later. Similarly, instead of maintaining any kind of mapping for fast index lookup, we decided to just iterate through all entries, which is much simpler to code and should not be much slower, since there are only 64 entries to start with.

In terms of synchronization, we wanted to reduce the use of lock as much as possible, which motivated us to use conditional variables. The use of just 1 lock in the cache has several advantages: it is easy to track when the lock is released; it is conceptually simpler, and so we can be confident to prevent race conditions on multiple variables at once; it allows us to use conditional variables, which fit nicely into the concept of waiting for an entry/ a block of data to be free to use.

Overall, we expect our design to be conceptually clean. We might still need to write a lot of code, however, since the addition of a cache to a filesystem is not a trivial problem. Should we find it too slow to iterate through all cache entries (we do not think this is likely), we will look into using a hashmap to map sector numbers to cache entry indices.


## Task 2 Extensible Files
### 1. Data structure and functions

- In filesys/inode.c:
```
struct inode_disk {
	block_sector_t direct[123];          /* First 123 blocks */
	block_sector_t indirect;	           /* Indirect pointer to block pointing to next 128 blocks */
	block_sector_t doubly_indirect;   /* Doubly indirect pointer for up to 128*128 more blocks */
	off_t length;			/* File size in bytes. */
	unsigned magic;			/* Magic number. */
}

struct inode {
	...
	struct lock resizing;		     /* Lock for resizing file */
	...
}
```

### 2. Algorithms
The following functions will be changed:

In `inode_open`, we first check if an in-memory inode already exists. If it does not, when we initialize the in-memory inode we will also initialize the `resizing` lock. We might consider adding additional synchronization measures here, if needed.

In `inode_create`, a block will be allocated for the `inode_disk` object the same way that it is done now. For blocks after that, instead of using `free_map_allocate` to allocate contiguous blocks (which could fail if a long enough contiguous set of blocks does not exist) we will iteratively allocate 1 block at a time using `free_map_allocate` and put them in the ordered array of block sectors `direct`; if more than 123 blocks need to be allocated then we will allocate additional blocks for `indirect` and `doubly_indirect` before continuing to allocate blocks. This will avoid external fragmentation in the disk and ensure that files can be extended (detailed in next part). If at any point `free_map_allocate` fails to allocate an additional needed block, all the blocks allocated up to that point will be `free_map_release` and the call to `inode_create` will return false. 

In `inode_write_at`, we first check if writing `size` bytes starting at `offset` will write past the current EOF. If it will, then `inode_write_at` will call `lock_acquire` on `inode -> resizing` (which may put the thread to sleep if another thread is already resizing this inode). Once the thread acquires this lock, it will again check if writing `size` bytes starting at `offset` to the resized inode will write past EOF. If it still will, then `inode_write_at` will begin iteratively allocating 1 block at a time using `free_map_allocate` until enough blocks have been allocated to “inode” or until `free_map_allocate` cannot allocate anymore additional blocks. At this point the lock will be released and `inode_write_at` will begin `block_write`ing to the blocks it needs to write at (if `inode_write_at` does not need to extend a file it will just skip to this part).


### 3. Synchronization
The only synchronization issue that could arise is two threads extending the same file, but this case is handled by the `resizing` lock in `struct inode`. Beyond this the usual synchronization issues of writing to the same block need to be handled, and this is discussed in task 1. 

### 4. Rationale
One alternative that was considered was changing `free_map_allocate` to take a `block_sector_t` array as an argument and fill it with non-contiguous block numbers that were iteratively allocated, but due to the complicated nature of malloc’ing a `block_sector_t` array, copying its contents to the inode’s “direct” array, and then freeing the buffer `block_sector_t` array we decided instead to just iteratively call `free_map_allocate` and allocate 1 block at a time. The other details of this task are pretty straightforward and we think our design is as simple and easy to debug as possible. 

## Task 3 Subdirectories
### 1. Data Structure and Functions

- In filesys/inode.c:
```
struct inode{
…
/* indicate whether this inode corresponds to a file or a directory */
struct *inode parent; 	 	/* Used to track the parent directory */
bool is_dir; 
struct lock lock; 		/* This lock is only initialized for directories */
...
}

/* add is_dir */
bool inode_create (block_sector_t, off_t, bool is_dir);
```

- In filesys/directory.c:
```
/* extracts a file name part from *SRCP into PART, and updates *SRCP so that the next call will return the next file name part. Returns 1 if successful, 0 at end of string, -1 for a too-long file name part. */ 
static int get_next_part (char part[NAME_MAX + 1], const char **srcp) 

/* this helper function takes in the directory name, which could be relative or full path, and return the corresponding struct dir if it is valid else NULL */
struct *dir get_dir_by_name(const char *dir)
```

- In threads/thread.h:
```
struct thread{
…
/* a pointer to struct dir corresponding to the current working directory of the process */
struct dir *cwd;
...
}
```

- In threads/thread.c:
```
/* assign cwd of the main thread*/
void thread_init (void)

/* assign cwd of the new thread*/
static void init_thread (struct thread *t, const char *name, int priority)
```

- In useprog/syscall.c:
```
/* add syscalls chdir, mkdir, readdir, and isdir; modify open, read, write, close, exec, remove, and inumber to work with directories */
static void syscall_handler (struct intr_frame *f UNUSED)
```

- In filesys/filesys.c:
```
/* Modify these methods so that we can obtain directory from file name */
/* Add is_dir when creating a file*/
bool filesys_create (const char *name, off_t initial_size, bool is_dir)
struct file * filesys_open (const char *name)
Bool filesys_remove (const char *name)
```

### 2. Algorithms

When we initialize the main thread in `thread_init`, initialize its `cwd` as root directory. When we initialize any other thread in `init_thread`, we will assign its cwd as cwd of `thread_current`.

We also need to construct a helper function `struct *dir get_dir_by_name(const char *dir)`. We first look at the first two of dir: if it starts with ‘/’, it represents a full path. If it is a full path, we first call `dir_open_root` to open the root directory, which returns a dir struct. Then we recursively call the following functions in sequence until we reach the final level of directory: `get_next_part` to get the next directory name, then `dir_look_up` to find the inode of next level directory, finally `dir_open` to open the next level directory if `is_dir`. If at any point `get_next_part` or `dir_look_up` returns false, or `dir_open` returns NULL, we return NULL. Otherwise, We return a pointer to the dir struct of the final directory.

If the first character is not ‘/’, then it represents a relative path. In this case, we first call `get_next_part` once, which will assign next part to char part in its argument: if part becomes ‘.’, we start with cwd of current thread; if we encounter ‘..’, we go to the parent directory. Then we recursively call those three functions in sequence until we reach the final level of directory. If any failure, return NULL. Otherwise, we return a pointer to the dir struct of the final directory. 

Finally, we implement the edge case checking. There are two edge cases we need to handle: Ignore .. on the root directory since the root directory is its own parent; Multiple consecutive slashes are equivalent to a single slash, so this file name is the same as a/b.

We also need to add or modify the following functions:

- Syscall: bool chdir(const char *dir):
We call `get_dir_by_name` with dir. If the helper function returns a dir struct pointer, we assign it to cwd of current thread and return true. Otherwise, return false.

- Syscall: bool mkdir (const char *dir):
We first parse the path of final directory that we want to put our new directory in, then call `get_dir_by_name` with that path to get dir struct of the parent directory. We use lookup to check if the new directory already exists in the parent directory that we want to put it in. If so, return false. Otherwise, we use `free_map_allocate` to get a `inode_sector` number, `dir_create` and `dir_add` to create a new directory in the parent directory. In particular for `dir_create`, we will initialize the directory with 16 entries in the beginning, since task 2 allows inode to grow later.

- Syscall: bool readdir (int fd, char *name):
We will go through the struct list files of the process, find the `fs_bundle` by fd. The `fs_bundle` has file struct, which has inode struct. We then call `dir_open` on that inode to find the dir struct, and call `dir_readdir` on that dir struct to store the null-terminated file name in name and return true.

- Syscall: bool isdir(int fd): 
	We will go through the struct list files of the process, find the `fs_bundle` by fd. The `fs_bundle` has file struct, which has inode struct. Return `is_dir` of the inode.

- Syscall: int inumber(int fd):
Find the inode corresponding to fd, then return sector of the inode.

- Modify open:
In `filesys_open`, if the inode found by `dir_lookup` is a directory, we will use `dir_open`.

- Modify create:
When creating a new file, we will check if the directory where the new file will be placed in is removed by looking at the boolean removed of the inode corresponding to that directory.

- Modify read, write:
Check if the `fs_bundle` found by fd corresponds to a directory by looking at the inode of file of the `fs_bundle`. If yes, fail the syscall
	
- Modify close:
Check if the `fs_bundle` found by fd corresponds to a directory by looking at the inode of file of the `fs_bundle`. If yes, will use `dir_close`.

- Modify exec:
In `init_thread`, the child process created by exec syscall will inherit the parent process’s `cwd`.
 
- Modify remove:
In `dir_remove` called by `filesys_remove`, if the inode found by `inode_open` is a directory, we first check if it do not contain any files or subdirectories (other than . and ..). If it is an empty directory and `in_use` is false, we will call `dir_close` to delete it. 

### 3. Synchronization
Synchronization issue arises in two cases: the first one involves remove when removing the same file or directory or file and directory containing that file, the second one is mkdir. As a result, we decide to put a lock on each inode, which can be a file or a directory. When we attempt to remove a file or directory, we acquire the lock of that file or directory and its parent directory, and release the lock when we are done. Similarly for mkdir, we first acquire the lock of current working directory and release the lock after we are done.

### 4. Rationale 

The biggest design challenge was to how the design of inode struct and dir struct could be incorporated into this task. We decided to continue using `fs_bundle` from task2. `fs_bundle` contains fd, pointer to a file struct, which contains inode struct. We added `is_dir` boolean attribute to inode struct so that we could tell if an inode corresponds to a file or a directory. In this way, we can easily find a directory by fd or inumber via its `fs_bundle`, file and inode.

Another challenge was to support full path and relative path. Since multiple syscall involves such support, we decided to use a helper function, which can take in a full path or a relative path and return the dir struct corresponding to the path. As a result, our code would become cleaner and simpler.
The remaining modifications to syscalls are more straightforward: we simply check if the fd passed in corresponds to a directory. If so, we would respond as the desired behavior.

## Additional question

### Write behind:
We can call the timer interrupt every certain amount of time. During the interrupt, we call the function that writes dirty block back to disk and place a lock on that block. We then continue running the original program. Once the writing process finishes, we remove the dirty bit and release the lock on that block.

### Read ahead:
We can first create a list of all block pointers that we need to read. During each iteration of `inode_read_at`, we pop out the first item of the list. In the `interrupt_handler` in `ide.c`, we load the first block in the list to the cache buffer and mark the block as loading so that it won’t be loaded twice during the `inode_read_at`.

