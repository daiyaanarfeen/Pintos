Design Document for Project 1: Threads
======================================

## Group Members

* Mohammad Arfeen <daiyaanarfeen@berkeley.edu>
* Xingyu Lu <xingyulu0701@berkeley.edu>
* Claire Liu <liuzmin@berkeley.edu>
* Qiaowei Zhang <qiaowei@berkeley.edu>


## Task 1 Efficient Alarm Clock
### 1. Data structure and functions
- In threads/thread.h:
```
struct thread
{
	…
	/* This struct holds pointers to the next and previous elements in the list */
	struct list_elem elem;
	/* The wake-up time for the current thread */
	int64_t wake_up_tick;
	…
}
```
- In threads/thread.c:
```
/* This function will be modified to initialize the new fields above */
struct void init_thread(struct thread *t, const char *name, int priority);
```
- In devices/timer.c:
```
/* A wait list of sleeping threads to be woken up, ordered by wake_up_tick*/
static struct list wait_list;
/* A lock on the wait_list for synchronization issues. */
static struct lock wait_list_lock;
```

### 2. Algorithm
Our `wait_list` keeps a list of threads that are blocked (sleeping), and it is ordered by `wake_up_tick`. When `timer_sleep()` is called, the thread should calculate the `wake_up_tick`, which is time (in terms of the **number of ticks** since the OS is booted) when it should be woken up. Then, it adds itself (lock issue explained in the next section) to the `wait_list`, using `list_insert_ordered()` so it is placed at the **right place** on the list. Finally, it blocks itself using `thread_block()`.<br/>
When `timer_interrupt()` is called, it simply goes through the `wait_list` and pops off all the threads whose `wake_up_tick` has past. Since the wait_list is ordered by `wake_up_tick`, we can traverse from the front of the list until the latest thread to be woken up. It should also call `thread_unblock()` to actually wake them up.

### 3. Synchronization
- `wait_list`<br/>
This list is shared among all threads and the scheduler. As list manipulation is not thread-safe in pintos, we decided to add a `wait_list_lock` to make sure no two entities are messing with the list at the same time. <br/>
In particular, when a thread calls `timer_sleep()`, it should use `lock_acquire()` to acquire `wait_list_lock`, add itself to the `wait_list`, use `lock_release()` to release `wait_list_lock`, and finally call `thread_block()`. `timer_interrupt()` also needs to check if a lock is held, but it should not try to acquire the lock; instead, it should disable interrupts while accessing the list. <br/>
Lastly, there is an edge case where the thread might be interrupted before it blocks itself, but after adding itself to the list. This is fortunately easy to check by `timer_interrupt()`, which should only unblock a thread if it has been blocked.

### 4. Rationale
Our biggest design decision is to create a variable for indicating **what time** to wake up a thread, which is to use `wake_up_tick`. At first our vanilla solution was to simply store the `tick` variable passed into `timer_sleep` directly in the thread, but it turned out that updating this value for all the threads would be extremely inefficient. Instead, we realized that we could calculate the global tick the thread should reach before it is unblocked, and so we decided to use `wake_up_tick`. This way, we do not have to tediously update each thread’s wait time at every tick.</br>

Another issue we pondered upon was whether the use of `wait_list_lock` would cause `timer_interrupt()` to not wake up threads on time, as it might need to wait for another thread to finish inserting itself into `wait_list`. We realized that this wait time is inevitable, for otherwise `timer_interrupt()` may mess around with the list (e.g. popping off elements off `wait_list`), leaving the thread that is inserting itself into `wait_list` with invalid pointers.<br/>

In terms of conceptualization, this task is not very difficult, and so the idea boils down to inserting threads into a queue to wait for being woken up. In terms of coding, as we do not need to change many functions, it should not be too messy either.<br/>

Lastly, the features implemented for this task is orthogonal to the other two tasks, and by its nature independent of most other parts of the OS, so we expect that it would not be too difficult to accommodate new features, unless those features directly relate to this task.


## Task 2 Priority Scheduler
### 1. Data structure and functions
- In threads/thread.h:
```
struct thread
{
…
/* The effective priority and the original priority */
fixed_point_t effective_priority;
fixed_point_t original_priority; /*We convert the original int value into fixed_point_t*/
 
/* This holds a pointer to the thread it is waiting for to acquire a lock. */
struct thread * blocking_thread;
 
/* The list of threads that are waiting for this thread to release a lock. */
struct list waiting_threads;
…
}
```
- In threads/thread.c:
```
/* This function will be modified to initialize the new fields above */
struct void init_thread(struct thread *t, const char *name, int priority);
 
/* This function will be modified to select the thread with the highest effective priority */
static struct thread *next_thread_to_run(void);
 
/* These functions will be modified to get/set the current thread’s effective priority */
void thread_set_priority(int new priority); /* Should yield the thread when appropriate */
void thread_get_priority(void);
```
- In threads/synch.c:
```
/* These functions will be modified to select the waiting thread with the highest effective priority */
void sema_up(struct semaphore *sema);
void cond_signal(struct condition *cond, struct lock *lock UNUSED);
 
/* This function will be modified to perform priority donation */
void lock_acquire(struct lock *lock);
void lock_try_acquire(struct lock *lock);
 
/* This function will be modified to select the waiting thread with the highest effective priority; additionally, it should revert priority donation associated with the lock */
void lock_release(void);
 ```
### 2. Algorithm
#### 1) Choosing next thread to run
This is performed in `next_thread_to_run()`. Instead of using `pop_list_front()` to choose the next thread from `ready_list`, it should use `list_max()` to select the thread with the highest effective priority.
#### 2) Acquiring a lock
When a thread T acquires a lock, it should first check if the lock is being held by another thread. This check is completed by calling `lock_try_acquire()`.</br>
In the case when the lock is being held by another thread T1, we set T’s `blocking_thread` to T1, and we add T to T1’s `waiting_threads`. Lastly, we perform priority donation, recursively:
- **Base case**: T’s `effective_priority` is not bigger than T1’s `effective_priority`, so we stop;
- **Recursive step**: we set T1’s `effective_priority` to be the max of T’s `effective_priority` and T1’s `effective_priority`; then, if T1 also has a `blocking_thread`, then we recurse on T1 and its `blocking_thread`. </br>
After T (the original thread) acquires the lock, we reset T’s `blocking_thread` to null, and add all of the lock’s waiters (lock->semaphore->waiters) to T’s `waiting_threads`. For each waiter, we also change its `blocking_thread` to T. 
#### 3) Releasing a lock
When a thread releases a lock, it should iterate through the waiters of that lock by calling lock->semaphore->waiters, and it should remove each waiter from its `waiting_threads`. Then, it should iterate through `waiting_threads`, and its priority should be the max of its original priority and the effective priorities of all the threads in `waiting_threads`.
#### 4) Computing `effective_priority`
By default this is set to `original_priority`. Updates to this value only happen in `lock_acquire` and `lock_release`, and so the computations have been detailed in the previous two parts.
#### 5) Priority scheduling for semaphores and locks, and conditional variables
Since scheduling happens in `sema_up()`, we should just use `list_max()` instead of `list_pop_front()` to select the thread with the highest effective priority.
#### 6) Changing thread’s priority
Since we use `effective_priority` to perform the actual scheduling, all on-demand changes to a thread’s priority (and `effective_priority`) can be performed by calling `thread_set_priority`. Note that when part 4) happens, the thread’s `effective_priority` also changes. We make sure to call `thread_yield()` when the thread no longer has the highest priority, by checking the `ready_list` when its priority decreases.

### 3. Synchronization
#### 1) `waiting_threads`
This list is accessed both by the thread owning this list and the threads in the list. In particular, during `lock_release()`, the thread removes elements from its own `waiting_threads`. During `lock_acquire()`, there are two stages: the thread first waits for the lock, so it adds itself the lock holder’s `waiting_threads`; after it actually acquires the lock, the thread removes itself from the lock’s previous holder’s `waiting_threads`, and add the lock’s waiters to its own `waiting_threads`.</br>

The first scenario when two threads might be acting on the same `waiting_threads` is when they both call `lock_acquire` on the same lock at the same time (one gets interrupted in the middle, for instance). We propose two ways to solve this: first is to turn interrupt off for operations that change waiting_threads, as most of these operations are very quick (adding one item to the list); second is to create a lock on each thread’s list, and thread must acquire the lock to change `waiting_threads`. Our concern for the first approach is that it might not be fast enough, and our concern for the second approach is that we are creating locks for acquiring locks, which makes everything very complicated. We decided to go for the first way for now, since that is straightforward to implement and conceptually clean.</br>

The second scenario when there may be concurrency issue is when a thread calls `lock_release` and gets interrupted by another thread. We make sure to turn interrupt off when we set lock’s holder to NULL and call `sema_up()` to make sure two operations always happen consecutively (this should be super fast since `sema_up()` already turns interrupt off). As long as these two operations are tied together, any other interrupting threads that call `lock_acquire` on the same lock will not be confused, since lock->holder always correctly reflects the holder of the lock. </br>

Apart from this, no two threads will ever be mutating the same waiting_threads at the same time, since the lock has to be released first and then acquired, and the lock cannot be simultaneously acquired by multiple threads.</br>
#### 2) waiters
Our algorithm does not make any additional changes to the elements in waiters. Additionally, accesses to waiters is also regulated by the lock-releasing and lock-acquiring, which involves the use of `sema_down()` and `sema_up()` that prevent synchronization issues.

### 4. Rationale
Our central idea is to add an `"effective_priority”` attribute to the thread struct. When we schedule, we use `effective priority` instead of the base priority, and change the value when priority donation happens or reverts.</br>
To keep track of the priority donations, we considered a number of implementations. The first implementation we were considering doing consisted of adding new fields in the thread struct for “wanted_lock” and "acquired_locks", which would contain lock pointers consistent with their names. After discussing this implementation with a TA, we were told that issues related to deallocation of locks could cause kernel panics; so we completely re-thought our implementation. </br>
As a result, we designed an algorithm that would have the least dependency on the locks themselves. The only way to keep track of all the  donations without using the locks as intermediaries was to directly keep track of thread-to-thread relations. Since a thread can only donate to one other thread at a time, we added a `blocking_thread` attribute to the thread struct; this would ensure that we could keep track of the donation relationship, and update it when appropriate. We added a `waiting_threads` attribute to the thread struct so that when a thread releases a lock (and potentially still has other locks) it can correctly compute its new effective priority. </br>
Our implementation is complicated, in a sense, because it has multiple moving parts; this might cause issues that are difficult to debug but will ensure that the thread’s do not fail for any reason due to the locks. Our implementation also has high overhead for `lock_release` and `lock_acquire` as it has to delete threads from the original holder’s `waiting_threads` as well as add threads to the new holder’s `waiting_threads`. We are currently planning to turn off interrupt to prevent synchronization errors, but if this turns out to be non-ideal we will look into locking shared resources using semaphores. 


## Task 3 MLFQS
### 1. Data structure and functions
- In threads/thread.h
```
/* We add two additional fields to thread */
struct thread{
	….
	/* The niceness value*/
	fixed_point_t niceness;
	/* The recent_cpu value */
	fixed_point_t recent_cpu;
	…
   	 }
```
- In threads/thread.c
```
/* The priority lists for holding threads ready to run, sorted by their priority */
#define LIST_SIZE 64
static struct list priority_list[LIST_SIZE];
 
/* This index keeps track of the first non-empty list in priority_list */
static int priority_index;
 
/* The number of threads in the ready queue */
static int total_ready_threads;
 
/* Load average */
static fixed_point_t load_average;
 
/* This function will be modified to initialize the new fields in thread */
struct void init_thread(struct thread *t, const char *name, int priority);
 
/* This function is modified to initialize load_average, priority_index, total_ready_threads, and the lists in priority_list;
void thread_init(void);

/* The list of treads that have run in the past 4 ticks */
static struct list active_threads;

/* The following functions are slightly modified to increment/decrement total_ready_threads when appropriate*/
void thread_unblock(struct thread *t);
void thread_block(void);
void thread_exit(void);
 
/* This function is modified to run the threads in the list with the highest priority*/
static struct thread *next_thread_to_run(void);
 
/* This function is modified to update values including recent_cpu, load_aveage, and threads’ effective priorities*/
void thread_tick(void);
 
/* These functions will be implemented to set/get the respective values of a thread */
void thread_set_nice (int nice);
void thread_set_recent_cpu(int recent_cpu);
int thread_get_nice(void);
int thread_get_load_avg(void);
int thread_get_recent_cpu(void);
```


### 2. Algorithm
- Niceness

This is a field initialized with the thread, and can be set again using `thread_set_nice();`

- Finding the next thread to run

Using `priority_index`, we can get the non-empty list of **ready threads with the highest priority**, and treat that list as `ready_list` to run the threads in it. We set `priority_index` to `-1` if there are not any ready threads, in which case we would run idle_thread.

- Updating `recent_cpu`, `load_average`, and thread `effective_priority`

All of these are performed in `thread_tick()`. At **every tick**, we increment the **current thread’s `recent_cpu`**, and add it to `active_threads` if it is not already in it; at **every 4 ticks** (checking if `ticks` is divisible by 4), we recompute **priorities of all threads in active_threads**, clamping them between `PRI_MIN` and `PRI_MAX`, and then reset `active_threads` by popping off all elements in it; at **every `TIMER_FREQ`**, we update the **`load_average`** (global value, updated using `total_ready_threads`), then **`recent_cpu`**, and finally **`effective_priority`** of **each thread**.
Lastly, we move threads around `priority_list` based on their new priority values.

- Updating `priority_index`.

This value is updated in `thread_tick` and `next_thread_to_run`. In `thread_tick`, `priority_index` may be changed if we move threads into new lists in `priority_list` , and this can be updated on-the-fly; in `next_thread_to_run`, if the list pointed to by `priority_index` is empty after we run a thread, we decrement `priority_index` until it finds the next non-empty list or reaches -1.


### 3. Synchronization
- `recent_cpu`, `load_avg`, `effective_priority` <br/>
The updates to these values happen inside `timer_interrupt()`, which runs in an **external** interrupt context, so there will **not** be synchronization issues. 

- `priority_list`<br/>
This list of lists is accessed by both `timer_interrupt()` and `schedule()`. As we disable interrupts for both functions, there will **not** be syncrhonization issues as each update to `priority_list` will be completeed without interruption.  

### 4. Rationale
Our biggest decision was **where** to put all the updates. Putting it inside `timer_interrupt()` ended up being the most natural choice, as it is run in an external interrupt context and it is where we keep track of the ticks. We were concerned that this may make `timer_interrupt` too slow, so we moved all the computation to `thread_tick()`, which takes place after we unblock threads (task 1).

Our next challenge was to keep track of all the threads between 4 ticks, since their priorities need to be updated every 4 ticks. Originally we would only update the priority of the thread that is running when `tick % 4 == 0`, but that ignored the cases when a thread is switched out for any reason before running for `TIME_SLICE` ticks. 

Seeing that `recent_cpu` only changes every second for ready threads, we decided to update their priorities every second, after re-computations of `load_average` and `recent_cpu`.  This way we could save a lot of computation time and reduce the time spent in `thread_tick()`.

Our next design choice was to represent the 64 priority levels. We had two ideas: using a list of lists to represent 64 priority bins, or using two lists to represent the threads with the highest priority (list 1) and other threads (list 2). We considered the first idea better, as all operations to the list require constant time, including moving a thread to a new priority bin, finding the next thread to schedule. We decided to include an integer `priority_index` that points to the highest priority bin to achieve this constant runtime. 

Conceptual wise, this task is also quite straightforward: we make updates to `recent_cpu`, `load_avg`, and `effective_priority` at the appropriate ticks, and shift the threads around priority bins. Making sure `priority_index` is always correct may require some carefulness, but the rest of coding should not be too challenging.


## Additional Question
#### 1. Test for Bugs
- **Test Setup**: Suppose there are four threads: T1 with base priority 1, T2 with base priority 2, T3 with base priority 3, T4 with base priority 4 and there are two locks L1, L2. Now T1 `lock_acquire` L1, T2 `lock_acquie` L2; thus, T1 has L1, T2 has L2. Then, T2 `lock_acquire` L1, T3 `lock_acquire` L1, and T4 `lock_acquire` L2. In other words, T2 and T3 are both waiting for L1 which is held by T1 and T3 is waiting for L2 which is held by T1. Then, T1 `lock_release` L1. Now, suppose T2 and T3 do not require any more locks, and when each thread finishes running it should print out its name. Ignoring T1's output, do we expect to see "T2" printed first, or "T3"?</br>
- **Expected Output**: Since T4 is waiting for L2 which T2 holds, T4 would donate priority to T2. As a result, T2 has base priority 2 but effective priority 4. When T1 `lock_release` L1, which calls `sema_up`, among L1's waitlist T2 has higher effective priority(4) than T3(3). T2 should acquire L1 and **T2** will be printed first.</br>
- **Actual Output/ Proof of Bug**: Since `sema_up` looks at base priority rather than effective priority and T3 has higher base priority than T2, T3 actually acquires L1 and **T3** will be printed.</br>

#### 2. MLFQS Scheduler
timer ticks | R(A) | R(B) | R(C) | P(A) | P(B) | P(C) | thread to run
------------|------|------|------|------|------|------|--------------
 0          |  0   |  0   |  0   |  63  |  61  |  59  |     A
 4          |  4   |  0   |  0   |  62  |  61  |  59  |     A
 8          |  8   |  0   |  0   |  61  |  61  |  59  |     A
12          |  12  |  0   |  0   |  60  |  61  |  59  |     B
16          |  12  |  4   |  0   |  60  |  60  |  59  |     A
20          |  16  |  4   |  0   |  59  |  60  |  59  |     B
24          |  16  |  8   |  0   |  59  |  59  |  59  |     A
28          |  20  |  8   |  0   |  58  |  59  |  59  |     B
32          |  20  |  12  |  0   |  58  |  58  |  59  |     C
36          |  20  |  12  |  4   |  58  |  58  |  58  |     A


#### 3. Resolve Ambiguities

Yes, the ambiguties occurs when there are several threads at same priority. We alphabetically choose the next thread to run to resolve the ambiguity for this problem. In our actual code, we will enforce a FIFO rule, since all of our priority bins are lists that behave like queues. 
