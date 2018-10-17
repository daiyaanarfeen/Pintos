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
- In :
```
code
```

### 2. Algorithm


### 3. Synchronization


### 4. Rationale



## Additional Questions
### 1

### 2

### 3

### 4 GDB Questions
#### 1
Name of thread running the function: main
Address of thread running the function: 0xc000e000

(gdb) info threads
  Id   Target Id         Frame
* 1    Thread <main>     process_execute (file_name=file_name@entry=0xc0007d50 "args-none") at ../../userprog/process.c:32

(gdb) dumplist &all_list thread allelem
pintos-debug: dumplist #0: 0xc000e000 {tid = 1, status = THREAD_RUNNING, name = "main", '\000' <repeats 11 times>, stack = 0xc000ee0c "\210", <incomplete sequence \357>, priority = 31, allelem = {prev = 0xc0034b50 <all_list>, next = 0xc0104020}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}
pintos-debug: dumplist #1: 0xc0104000 {tid = 2, status = THREAD_BLOCKED, name = "idle", '\000' <repeats 11 times>, stack = 0xc0104f34 "", priority = 0, allelem = {prev = 0xc000e020, next = 0xc0034b58 <all_list+8>}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}

#### 2
(gdb) backtrace
#0  process_execute (file_name=file_name@entry=0xc0007d50 "args-none") at ../../userprog/process.c:32
#1  0xc002025e in run_task (argv=0xc0034a0c <argv+12>) at ../../threads/init.c:288
#2  0xc00208e4 in run_actions (argv=0xc0034a0c <argv+12>) at ../../threads/init.c:340
#3  main () at ../../threads/init.c:133

/userprog/process.c:32
```{```

/threads/init.c:288
```process_wait (process_execute (task));```

/threads/init.c:340
```a->function (argv);```

/threads/init.c:133
```run_actions (argv);```

#### 3
Name of thread running the function: args-none\000\000\000\000\000\000
Address of thread running the function: 0xc010a000

(gdb) info threads
  Id   Target Id         Frame
* 1    Thread <main>     start_process (file_name_=0xc0109000) at ../../userprog/process.c:55

(gdb) dumplist &all_list thread allelem
pintos-debug: dumplist #0: 0xc000e000 {tid = 1, status = THREAD_BLOCKED, name = "main", '\000' <repeats 11 times>, stack = 0xc000eebc "\001", priority = 31, allelem = {prev = 0xc0034b50 <all_list>, next = 0xc0104020}, elem = {prev = 0xc0036554 <temporary+4>, next = 0xc003655c <temporary+12>},pagedir = 0x0, magic = 3446325067}
pintos-debug: dumplist #1: 0xc0104000 {tid = 2, status = THREAD_BLOCKED, name = "idle", '\000' <repeats 11 times>, stack = 0xc0104f34 "", priority = 0, allelem = {prev = 0xc000e020, next = 0xc010a020}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}
pintos-debug: dumplist #2: 0xc010a000 {tid = 3, status = THREAD_RUNNING, name = "args-none\000\000\000\000\000\000", stack = 0xc010afd4 "", priority = 31, allelem = {prev = 0xc0104020, next = 0xc0034b58 <all_list+8>}, elem = {prev = 0xc0034b60 <ready_list>, next = 0xc0034b68 <ready_list+8>}, pagedir = 0x0, magic = 3446325067}

#### 4
process.c:45
```
tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);
```

#### 5
(gdb) btpagefault
#0  0x0804870c in ?? ()

#### 6
```
#0  _start (argc=<error reading variable: can't compute CFA for this frame>, argv=<error reading variable: can't compute CFA for this frame>) at ../../lib/user/entry.c:9
exit (main (argc, argv));
```

#### 7
All of our Pintos test programs start by printing out their own name (e.g. argv[0]). Since argument passing has not yet been implemented, all of these programs will crash when they access argv[0]. Until we implement argument passing, none of the user programs will work.
