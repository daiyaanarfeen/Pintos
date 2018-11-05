Final Report for Project 2: User Programs
=========================================

## Group Members

* Daiyaan Arfeen <daiyaanarfeen@berkeley.edu>
* Patrick Lu <xingyulu0701@berkeley.edu>
* Claire Liu <liuzmin@berkeley.edu>
* William Zhang <qiaowei@berkeley.edu>

## Student Testing Report

### Test 1: Remove

* Tested features:  
`remove` and `open`. When a process removes a file, others processes should not be able to open that file. When a process fails to open a removed file, it should get a `-1` fd and not exit abnormally because of the failed remove.

* Test description:  
One process first creates a file called `deleteme`, opens it, and then closes it. Then, it executes another child process that attempts to open this file. Our test fails if the second process manages to open the file (get a valid fd value that is not `-1`) or if it exits abnormally. 

* Expected output:  
```
main test starts; 
main test calls child process;
child process exits with value 0;
main test exits with value 0.
```

* Kernel bug 1:  
If remove fails to prevent other processes from opening the file, then the child process would successfully open the `deleteme`, in which case it would get a valid fd value. This will cause the test to fail, and print out the message “You managed to open a removed file”.

* Kernel bug 2:  
If open is unable to handle a removed file and causes the child process to exit, then in the output we would get a line that tells us the child process has exited abnormally; this will cause the test to fail, because in our expected output we specify that the child process should exit normally.

### Test 2: Tell

* Tested features:   
`Tell`. When `tell` is called on a invalid fd, which may be an unallocated fd, stdin, or stdout, it should fail and cause the process to exit (this was not specified in the proj 2 spec, and so we decided that exiting should be the intended behavior; alternatively, tell could return -1, in which case we only need to make minor changes to the test). 

* Test description:   
Our main process creates three child processes, each attempting an invalid tell on unallocated fd, stdin, and stdout respectively. The test checks that all three child processes have exited abnormally, and that the main process is unaffected. 

* Expected output:  
```
main test starts;
main test calls child process 1;
child process 1 exits with value -1;
main test calls child process 2;
child process 2 exits with value -1;
main test calls child process 3;
child process exits with value -1;
main test exits with value 0.
```

* Kernel bug 1:  
If `tell` cannot handle unallocated fd or does not cause the thread to exit immediately with status -1, we would 
  1. get output values that differ from our expected output, or
  2. reach the compulsory fail in the child process after tell, which the child process should never reach.

* Kernel bug 2:  
If tell cannot handle stdin and stdout (which are allocated fd that should not be called tell on), it may lead to kernel panics, or just not exit immediately. In either case, we would detect it in the same fashion as detecting bug 1. 

### Test Writing Reflection
Overall, We feel that by looking through tests and finding out the ones that are not covered, we were able to better understand the intended behavior of the system. It was pretty awesome to write a new test and see if our code can take the challenge.

In terms of feedback, we feel that one thing that could have really helped us is instructions on how to direct input to a program, as we tried to test reading from stdin but could not figure out how to do it. Other than that, we think the current instructions are quite comprehensive, and were sufficient for us to write new tests.

From writing tests, the most important thing we learned is that the correctness of the test is extremely important. We previously had a test that was incorrectly implemented, and so while it was passing it was not actually testing the functionalities correctly. We were fortunate to catch the bug in our test, and we have learned the importance of making sure the test itself is correct.

## Final Report:

### Task 1: 
William(Qiaowei) Zhang mainly worked on Task 1. I mostly followed the design doc for the `load` part. I made some minor modification on `process_execute`. I changed the argument for the `thread_create` so that the thread name is the same as the file’s name instead of `file_name` (which contains both the file’s name and the arguments). 

In terms of reflection, I spent a lot of time trying to figure out how to manage dereference and casting when pushing the arguments. I simply tried different combinations in the beginning and used the test case to check whether I did it correctly. It was pretty inefficient, so I changed the strategy. I inspected each level of dereference and casting carefully and figured out what was going on behind these syntax. I soon passed all the test case quickly. The most important thing I learnt from the project is to not rely too much on the test case, and instead try to have a deep understanding in the code when debugging. 

### Task 2:
Patrick(Xingyu) mainly worked on Task 2. I mostly followed the design doc, with the only change being initializing the process bundles inside `thread_create`, as I needed to have access to the `tid` of the thread to store in process bundle. After I finished implementing the code, however, I realized some flaws in the design: the biggest problem was synchronization, as we evenly split up the work between parent and child threads, and ended up using a lot of locks; the other problem was the overall complexity of the code, as we introduced a lot of moving parts. As a result, we failed to pass the multi-oom test, which already took me more than 10 hours of debugging.

Reflecting upon this project, I realized the need to reduce complexity in design, especially for large projects like this. I do think that most parts of my work went well, however, as I managed to pass most of the tests without too much debugging. For the next project, I hope to emphasize the need to reduce complexity, especially for synchronization purposes. Additionally, I still need to get better at using gdb, especially for debugging cases where multiple processes run concurrently.

### Task 3: 
Daiyaan and Claire mainly worked on task3. We basically followed the design doc: initialize `file_global_lock`, `all_files` in `syscall.c` and declare `fs_bundle` in `syscall.h`; handle each file system syscall in `syscall_handler`. In addition to implementation details in the design doc, we added more edge case in our actual implementations to make our os more robust: for example, writing to stdin or read from stdout will cause the process to exit with status -1. Moreover, we added a helper function `check_valid_ptr`, which checks the validity of pointers that users pass in before we try to use them; in case of bad pointer, the process will also exit with status -1 and release any locks it holds. 

In terms of reflection, we are more familiar with github this time though we still need more practice. In addition, we should learn more about C-particular syntax such as variable sharing across files.

