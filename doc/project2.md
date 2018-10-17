Design Document for Project 2: User Programs
============================================

## Group Members

* Daiyaan Arfeen <daiyaanarfeen@berkeley.edu>
* Patrick Lu <xingyulu0701@berkeley.edu>
* Claire Liu <liuzmin@berkeley.edu>
* William Zhang <qiaowei@berkeley.edu>


## Task 1 Argument Passing



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
We keep track of all the files (in the form of `fs_bundle`) opened by all processes. Whenever a process executes, it iterates through all the files, and disable writes on files that point to its executable; similarly, when a process exits, it iterates through all the files, and enable writes on files that point to its executable.</br>
Additionally, when a file is opened, it iterates through `all_list`, and disable write if any of the processes is executing the file (this is checked through thread’s name).</br>

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
We first acquire the `file_global_lock`. we go through the files list and get the corresponding `fs_bundle` instance if successful. We get the file instance and call `file_close`. Additionally, we remove the `fs_bundle` from files list. Finally, we release `file_global_lock`. </br>


### 3. Synchronization
Since we use a global lock on all syscalls in this task, there should not be any synchronization issues. Specifically, since this task includes all possible operations files, and they all require acquisition of the global lock, there should not be any synchronization issues. Note that this is not the most efficiency way, but it does simply the potential synchronization issues.</br>


### 4. Rationale
Our biggest design choice was how to keep track of all the representations of a file: file name, file struct, and file descriptor. We ended up creating a bundle struct for it, so we can link all three representations.</br>
Our second design choice was how to keep track of all files in a process. We initially thought about using an array of size 128, and index using the file descriptors. This approach, however, has two limitations: first one is that we cannot have more than 128 files open at the same time; second one is that managing the available file descriptors is prone to error. For these reasons, we ended up using a list, which will be slower (since we need to do list scans to find the appropriate `fs_bundle`) but easier to manager and less prone to bugs.</br>
In terms of coding difficulty, we feel that since most of the fundamental operations have already been provided to us, this task should not be too bad. The trick is to make the operations secure: checking buffer address, file descriptor, and length of the file names etc. This is likely an area that could be more extensively tested.</br>
Lastly, this task should be independent from the other two tasks in nature.</br>



## Additional Questions
### 1. Test name: Sc-bad-sp.c
Explanation: In line 18 we moved address -(64*1024*1024) to the register esp, which is stack pointer. Then we do a system call. Since the address lies approximately 64mb below the code segment, It’s a invalid stack pointer. 

### 2. Test name: sc-bad-arg.c
Explanation: In line 14 we first move the address 0xbffffffc to the register stack pointer. Then we move a long type of register zero(which contains the value zero.) to the value of the register stack pointer, which is 0xbffffffc. Since we add a long type of value, the end of the stack pointer is now 0xc0000003. Then we do the syscall. Since our PHYS_BASE is 0xc0000000. The pointer is in kernel virtual address space, which is a invalid pointer. 

### 3 File Operation Syscall: remove
Remove syscall of task 3 is not tested in the provided test cases. Since it is one of the important functionalities of the file system, we should test thoroughly this syscall. Specifically, we should test that when a file is removed, any process which has a file descriptor for that file may continue to use that descriptor. In other words, after being removed, the file will not have a name and no other processes will be able to open it. However, processes that have access to the file prior to its removal should still be able to read and write from the file, until all file descriptors referring to the file are closed or the machine shuts down.


### 4. GDB Questions
1) Name of thread running the function: main</br>
  Address of thread running the function: 0xc000e000</br>
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
3) Name of thread running the function: args-none\000\000\000\000\000\000</br>
Address of thread running the function: 0xc010a000</br>
```
(gdb) info threads
  Id   Target Id         Frame
* 1    Thread <main>     start_process (file_name_=0xc0109000) at ../../userprog/process.c:55

(gdb) dumplist &all_list thread allelem
pintos-debug: dumplist #0: 0xc000e000 {tid = 1, status = THREAD_BLOCKED, name = "main", '\000' <repeats 11 times>, stack = 0xc000eebc "\001", priority = 31, allelem = {prev = 0xc0034b50 <all_list>, next = 0xc0104020}, elem = {prev = 0xc0036554 <temporary+4>, next = 0xc003655c <temporary+12>},pagedir = 0x0, magic = 3446325067}
pintos-debug: dumplist #1: 0xc0104000 {tid = 2, status = THREAD_BLOCKED, name = "idle", '\000' <repeats 11 times>, stack = 0xc0104f34 "", priority = 0, allelem = {prev = 0xc000e020, next = 0xc010a020}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}
pintos-debug: dumplist #2: 0xc010a000 {tid = 3, status = THREAD_RUNNING, name = "args-none\000\000\000\000\000\000", stack = 0xc010afd4 "", priority = 31, allelem = {prev = 0xc0104020, next = 0xc0034b58 <all_list+8>}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}
```
4) process.c:45
```
tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);
```

5) (gdb) btpagefault
```
#0  0x0804870c in ?? ()
```
6)
```
#0  _start (argc=<error reading variable: can't compute CFA for this frame>, argv=<error reading variable: can't compute CFA for this frame>) at ../../lib/user/entry.c:9
exit (main (argc, argv));
```

7) All of our Pintos test programs start by printing out their own name (e.g. argv[0]). Since argument passing has not yet been implemented, all of these programs will crash when they access argv[0]. Until we implement argument passing, none of the user programs will work.
