Design Document for Project 2: User Programs
============================================

## Group Members

* Daiyaan Arfeen <daiyaanarfeen@berkeley.edu>
* Patrick Lu <xingyulu0701@berkeley.edu>
* Claire Liu <liuzmin@berkeley.edu>
* William Zhang <qiaowei@berkeley.edu>


## Task 1 Argument Passing
### 1. Data Structure and Functions:

- In userprog/process.c
```
/* This function will be modified to split file_name and extract argc and argv[] */
bool load (const char *file_name, void (**eip) (void), void **esp)

/* This function will be modified to load arguments to the stack. */
static bool setup_stack (void **esp)
```

### 2. Algorithms
In function load, we split `file_name` to get the actual file name, the argument list. We then count the number of arguments. We also make sure to null-terminate each of the arguments.

In load, we push the parsed command line argument strings onto stack, starting from address `PHYS_BASE`. We push the following values in sequence: the char values of each argv, `word_align` of type `uint8\_t`, pointers to each argv, pointers to argv itself, argc, and finally return address. For each value we push, we decrement the stack pointer by the size of value to push and push the value by dereferencing the pointer. We will enforce a suitable limit on the number of arguments, though this may not be necessary.

### 3. Synchronization
 There should not be any synchronization issue, as each process operates in its own memory space, and we use single-threaded processes in Pintos. In short, there are no shared resources.

### 4. Rationale
Conceptually, this task is quite straightforward. Our goal was to parse the argument, and push the relevant items in the right order onto the stack in order for the program to correctly use them; and so we modify the corresponding functions to achieve that. 

In terms of coding difficulty, it might be a little tricky to push items onto the stack in the right order; since all user processes have a lot of dependencies on this task, we need to be extra careful in order to not create sneaky bugs.

## Task 2 Process Control Syscalls

### 1. Data structure and functions
- In threads/thread.h
```
struct thread
	{
	...
	#ifdef USERPROG
	…
	struct process_bundle *parent;	/* A bundle with the parent of the thread */
	struct list children;	     /* a list of child threads of the thread for wait syscall */
	struct lock child_lock       /* a lock for the children list */
	...
	#endif
  	}
```
- In threads/thread.c
```
/* This function will be modified to initialize the process_bundle. */
static void init_thread (struct thread* t, const char *name, int priority);
```
- In userprog/process.h
```
/* We use this bundle to keep track of the information passed between a child process and its parent process. */
struct process_bundle{
	pid_t cid;			/* pid of the child */
	pid_t par;			/* pid of the parent.*/
	int status; 			/* The return status of the child, set to -1 upon initialization */
	struct semaphore sem;		/* A semaphore to check program status */
	struct lock pb_lock;		/* A lock on access to status, child_exit, parent_exit, and loaded /*
	bool child_exit; 		/* A value to check if the child has exited.*/
	bool parent_exit; 		/* A value to check if the parent has exited.*/
	bool loaded;			/* A value to check if the child program has loaded */
	struct list_elem elem;		/* A list elem used for a process’ children list. */
}
```
- In userprog/process.c
```
/* We modify this function to update process_bundle and inform parent process */
static void start_process(void *file_name)

/* This function will be modified to update the process_bundle. */
void process_exit(void)
void process_wait(tid)
```
### 2. Algorithm

- init_thread</br>
We initialize the `process_bundle` by `malloc()`, set the parent as `thread_current()`, and initialize the `process_bundle` semaphore as 0. Additionally, we add the `process_bundle` to the parent’s children list.

- start_process</br>
If the user program is successfully loaded, we set `process_bundle` loaded to true, and `sema_up()` the semaphore.

- process_exit</br>
We check the parent `process_bundle`, and all the `process_bundle` in children list. For each `process_bundle` that exists, we set the `child_exit` (for parent `process_budle`) or parent_exit (for `process_bundles` in children list) to true, and we free that `process_bundle` if both the child and the parent have exited. Additionally, if the parent `process_bundle` is not freed, we call `sema_up()` on the semaphore. 

- Use of `pb_lock` and `child_lock`</br>
For any functions that require access/modification to a `process_bundle` status, `child_exit`, `parent_exit`, or loaded, it needs to acquire `pb_lock` to ensure that there is no race condition. The lock is immediately released after access/modification.
For any access/change to a thread’s children list, acquisition of the `child_lock` is required, and the lock should be released immediately after access/change.

- Practice</br>
We take `arg[1]` as an integer, increment it by 1, and return it by passing it to `$eax`.

- Halt</br>
Call void `shutdown_power_off` in `device/shutdown.c` to shutdown the device and close the main thread.

- Exec</br>
Call `process_execute()` in userprog/process.c to initiate another process and obtain a `tid_t` value.  Then, we locate the corresponding `process_bundle` in children list, and call `sema_down()` on its semaphore. Lastly, we check the `process_bundle` loaded value. If loaded is false, we return -1, remove the `process_bundle` from the children list, and free the `process_bundle`; otherwise, we return the `tid_t` value. 

- Exit</br>
We check if the parent `process_bundle` exists, and if it does, we set the status to `arg[1]`. Then, we call `thread_exit()` to exit the process. 

- Wait</br>
We first iterate through children list to check if the `process_bundle` corresponding to the pid value exists, and return -1 if it does not exist.
Then, we call `sema_down` on the `process_bundle`’s semaphore value only if `process_bundle`'s `child_exited` is false. We then remove this `process_bundle` from children list, get its status value, and free the `process_bundle`. Lastly, we return the status value.

- Safely access user address</br>
We verify the validity of a user-provided pointer (as well as the whole region of memory spanning the arguments) before dereferencing it. We first check if it is a null pointer, then check if the pointer to kernel virtual address space using `is_kernel_vaddr`, and finally check if pointer to unmapped virtual memory by checking if `pagedir_get_page` returns `null`.


### 3. Synchronization
For wait and exec, we decided to use the semaphores to track the order in which events should happen. Since we have one semaphore for each parent-child relationship, the problem was able to be solved in a straightforward manner: the parent down the semaphore when it waits for the child, and the child ups the semaphore when it wants to wake up the parent.

We know that a child can up the semaphore at most twice: once after it is successfully loaded (in `start_process`), and once when it exits (in `process_exit`). For exec, this is perfectly fine: it could be woken up by any of them, and is guaranteed to be woken up since it is guaranteed to happen before any wait syscalls. For wait, there is a danger of never being woken up if the process fails loading, or if wait is called multiple times on the child process. As a result, we include an additional check, which is whether the child process has exited, to avoid putting a process into eternal sleep.

Additionally, we want to make sure that there are no race conditions happening in any `process_bundle`, since multiple threads may access and modify it at the same time. As a result, we add a lock in the `process_bundle`, and a thread must acquire the lock before modifying or checking the values. 


### 4. Rationale
We created `process_bundle` as a communication channel between the parent and the child processes. We decided to put this in kernel memory, so that it is not freed erroneously after one of the processes exits. Additionally, we included a lock to prevent race conditions.

`Practise` and `halt` are pretty straightforward, and so we just followed the spec. For `exec`, `exit`, and `wait`, we decided to use a semaphore to allow the parent and child threads to coordinate. our key idea was to make sure that the number of `sema_up()` is always larger than that of `sema_down()`: exec calls `sema_down()` once, and `wait()` calls it an additional time. As a result, we put one `sema_up()` after successful load, and one `sema_down()` in `process_exit()`, so that a parent thread is guaranteed to wake up.

Another area we focused on was when to free the `process_bundle`, and when to add/remove `process_bundle` to a process’ children list. For the former, a `process_bundle` is freed in two cases: 1. It is freed by a child process upon exit if its parent process has exited. 2. It is freed by a parent process when it calls `wait()` on a child process, or when it calls `exec()` and find that the child process failed loading. This way, we make sure that every `process_bundle` is freed exactly once. For managing a process’ children list, we make sure to add it exactly once upon creating the thread, and always remove it if the `process_bundle` is freed (i.e. whenever a parent frees a child `process_bundle`, iit always remove it from the children list first). 

Overall, this task’s design is quite complex, but we hope that by clearly dividing up the work between the parent process and the child process, we should be fine. When we start coding, we may realize that some parts of the design is redundant, and we will remove them as see fit.


## Task 3 File Operation Syscalls
### 1. Data structure and functions
- In userprog/syscall.c
```
/* A lock is needed since multiply processes may mess around with the same file at the same time. */
struct lock file_global_lock;

/* We keep track of all the files that are open */
struct list all_files;
```

- In userprog/syscall.h
```
/* This struct is used to keep track of different level of I/O calls.
struct fs_bundle{
	const char *filename;  /* The file name used to open the file */
	int fd;			/* The underlying file descriptor */
	struct file *file;		/* The underlying file struct */
	list_elem fs_elem; 	/* Element for the files list in thread.c */
	list_elem global_elem; /* Element for the file_global_list in syscall.c */
}
```
- In threads/thread.h
```
struct thread{
	…
	#ifdef USERPROG
	...
	list files;	/* Use this list to keep track of all files*/
	int next_fd;	/* Keep track of the next available file descriptor to use /*
	…
	#endif
	…
}
```
- In userprog/process.c
```
/* The following methods are changed to add the disable write feature */
tid_t process_execute(const char *file_name);
void process_exit(void);
```

### 2. Algorithm
- Initization</br>
We initialize the files list and set `next_fd` to 2 in thread.c, since 0 and 1 are reserved for the console. We also initialize the `file_global_lock` in `syscall_init`. </br>

- Disable writes on currently running programs’ executables</br>
We keep track of all the files (in the form of `fs_bundle`) in opened by all processes in all_files. Whenever a process executes, it iterates through all_files, and disable writes on files that point to its executable; similarly, when a process exits, it iterates through all the files, and enable writes on files that point to its executable.</br>
Additionally, when a file is opened, it iterates through `all_files`, and disable write if any of the processes is executing the file (this is checked through thread’s name).</br>

- create</br>
We acquire the `file_global_lock`, and then use `filesys_create` with the provided arguments to create the file. Finally, we release `file_global_lock`.</br>

- remove</br>
We acquire the `file_global_lock`, and then use `filesys_remove` with the provided arguments to remove the file. If successful, we also remove the file from the current thread’s files list. Finally, we release `file_global_lock`.</br>

- open</br>
We acquire the `file_global_lock`, then use `filesys_open` with the provided arguments to open the file and obtain a `struct *file` instance. If successful, we bind next_fd to the new instance and add the resulting `fs_bundle` to the current thread’s files list. We add one to `next_fd` so that we can keep track of the next available file descriptor to use. Finally, we release `file_global_lock`.</br>

- filesize</br>
We first acquire the `file_global_lock`. We go through the files list, and get the file struct if it exists. We return the length by `file_length`. Finally, we release `file_global_lock`.</br>

- read</br>
We first acquire the `file_global_lock`. We check that the provided buffer address is valid. Then, we have two cases:
If the fd is 0, we call `input_getc()` repeatedly until we read size characters from stdin.</br>
Otherwise, we go through the files list and get the corresponding `fs_bundle` instance. We get the file instance and call `file_read`. Finally, we release `file_global_lock`.</br>

- write</br>
We first acquire the `file_global_lock`. We check if the address of the buffer is valid. Then, we have two cases:</br>
fd is 1: we use putbuf to write the buffer content to the console. We write in chunks of 1024 bytes, so in most cases we write everything in the buffer to console at once.</br>
otherwise: we go through the files list and get the corresponding `fs_bundle` instance. We get the file instance and call `file_write`, if the file has write permission. </br>
Finally, we release `file_global_lock`.</br>

- seek</br>
We first acquire the `file_global_lock`. we go through the files list and get the corresponding `fs_bundle` instance if successful. We get the file instance and call `file_seek`. Finally, we release `file_global_lock`. </br>

- tell</br>
We first acquire the `file_global_lock`. we go through the files list and get the corresponding `fs_bundle` instance if successful. We get the file instance and call `file_tell`. Finally, we release `file_global_lock`. </br>

- close</br>
We first acquire the `file_global_lock`. we go through the files list and get the corresponding `fs_bundle` instance if successful. We get the file instance and call `file_close`. Additionally, we remove the `fs_bundle` from files list. Finally, we release `file_global_lock`. 


### 3. Synchronization
Since we use a global lock on all syscalls in this task, there should not be any synchronization issues. Specifically, since this task includes all possible operations files, and they all require acquisition of the global lock, there should not be any synchronization issues. Note that this is not the most efficiency way, but it does simply the potential synchronization issues.


### 4. Rationale
Our biggest design choice was how to keep track of all the representations of a file: file name, file struct, and file descriptor. We ended up creating a bundle struct for it, so we can link all three representations.

Our second design choice was how to keep track of all files in a process. We initially thought about using an array of size 128, and index using the file descriptors. This approach, however, has two limitations: first one is that we cannot have more than 128 files open at the same time; second one is that managing the available file descriptors is prone to error. For these reasons, we ended up using a list, which will be slower (since we need to do list scans to find the appropriate `fs_bundle`) but easier to manager and less prone to bugs.

In terms of coding difficulty, we feel that since most of the fundamental operations have already been provided to us, this task should not be too bad. The trick is to make the operations secure: checking buffer address, file descriptor, and length of the file names etc. This is likely an area that could be more extensively tested.

Lastly, this task should be independent from the other two tasks in nature.



## Additional Questions
### 1. Test name:`sc-bad-sp.c`
Explanation: In line 18 we moved address -(64*1024*1024) to the register esp, which is stack pointer. Then we do a system call. Since the address lies approximately 64mb below the code segment, It’s a invalid stack pointer. 

### 2. Test name: `sc-bad-arg.c`
Explanation: In line 14 we first move the address `0xbffffffc` to the stack pointer register, which means the stack is now at that address. Then we move a long type of register zero(which contains the value zero.) to the address of the register stack pointer, which is `0xbffffffc`. Since we add a long type of value, the the variable on the stack  now reaches the address `0xc0000003`. Then we do the syscall. Since our `PHYS_BASE` is `0xc0000000`. The pointer is in kernel virtual address space, which is a invalid pointer. 

### 3 File Operation Syscall: remove
Remove syscall of task 3 is not tested in the provided test cases. Since it is one of the important functionalities of the file system, we should test thoroughly this syscall. Specifically, we should test that when a file is removed, any process which has a file descriptor for that file may continue to use that descriptor. In other words, after being removed, the file will not have a name and no other processes will be able to open it. However, processes that have access to the file prior to its removal should still be able to read and write from the file, until all file descriptors referring to the file are closed or the machine shuts down.


### 4. GDB Questions
1) Name of thread running the function: `main`</br>
  Address of thread running the function: `0xc000e000`</br>
```
(gdb) info threads
  Id   Target Id         Frame
* 1    Thread <main>     process_execute (file_name=file_name@entry=0xc0007d50 "args-none") at ../../userprog/process.c:32

(gdb) dumplist &all_list thread allelem
pintos-debug: dumplist #0: 0xc000e000 {tid = 1, status = THREAD_RUNNING, name = "main", '\000' <repeats 11 times>, stack = 0xc000ee0c "\210", <incomplete sequence \357>, priority = 31, allelem = {prev = 0xc0034b50 <all_list>, next = 0xc0104020}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}
pintos-debug: dumplist #1: 0xc0104000 {tid = 2, status = THREAD_BLOCKED, name = "idle", '\000' <repeats 11 times>, stack = 0xc0104f34 "", priority = 0, allelem = {prev = 0xc000e020, next = 0xc0034b58 <all_list+8>}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}
```
2) backtrace
```
(gdb) backtrace
#0  process_execute (file_name=file_name@entry=0xc0007d50 "args-none") at ../../userprog/process.c:32
#1  0xc002025e in run_task (argv=0xc0034a0c <argv+12>) at ../../threads/init.c:288
#2  0xc00208e4 in run_actions (argv=0xc0034a0c <argv+12>) at ../../threads/init.c:340
#3  main () at ../../threads/init.c:133

/userprog/process.c:32
{

/threads/init.c:288
process_wait (process_execute (task));

/threads/init.c:340
a->function (argv);

/threads/init.c:133
run_actions (argv);
```
3) Name of thread running the function: `args-none\000\000\000\000\000\000`</br>
Address of thread running the function: `0xc010a000`</br>
```
(gdb) info threads
  Id   Target Id         Frame
* 1    Thread <main>     start_process (file_name_=0xc0109000) at ../../userprog/process.c:55

(gdb) dumplist &all_list thread allelem
pintos-debug: dumplist #0: 0xc000e000 {tid = 1, status = THREAD_BLOCKED, name = "main", '\000' <repeats 11 times>, stack = 0xc000eebc "\001", priority = 31, allelem = {prev = 0xc0034b50 <all_list>, next = 0xc0104020}, elem = {prev = 0xc0036554 <temporary+4>, next = 0xc003655c <temporary+12>},pagedir = 0x0, magic = 3446325067}
pintos-debug: dumplist #1: 0xc0104000 {tid = 2, status = THREAD_BLOCKED, name = "idle", '\000' <repeats 11 times>, stack = 0xc0104f34 "", priority = 0, allelem = {prev = 0xc000e020, next = 0xc010a020}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}
pintos-debug: dumplist #2: 0xc010a000 {tid = 3, status = THREAD_RUNNING, name = "args-none\000\000\000\000\000\000", stack = 0xc010afd4 "", priority = 31, allelem = {prev = 0xc0104020, next = 0xc0034b58 <all_list+8>}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}
```
4) `process.c:45`
```
tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);
```

5) `(gdb) btpagefault`
```
#0  0x0804870c in ?? ()
```
6)
```
#0  _start (argc=<error reading variable: can't compute CFA for this frame>, argv=<error reading variable: can't compute CFA for this frame>) at ../../lib/user/entry.c:9
exit (main (argc, argv));
```

7) All of our Pintos test programs start by printing out their own name (e.g. `argv[0]`). Since argument passing has not yet been implemented, all of these programs will crash when they access `argv[0]`. Until we implement argument passing, none of the user programs will work.
