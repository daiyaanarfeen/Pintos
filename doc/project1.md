Design Document for Project 1: Threads
======================================

## Group Members

* Mohammad Arfeen <daiyaanarfeen@berkeley.edu>
* Xingyu Lu <xingyulu0701@berkeley.edu>
* Claire Liu <liuzmin@berkeley.edu>
* Qiaowei Zhang <qiaowei@berkeley.edu>


## Tast 1 Efficient Alarm Clock
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
Our `wait_list` keeps a list of threads that are blocked (sleeping), and it is ordered by `wake_up_tick`. When `timer_sleep()` is called, the thread should calculates the `wake_up_tick`, which is time (in terms of the number of ticks since the OS is booted) when it should be woken up. Then, it adds itself (lock issue explained in the next section) to the `wait_list`, using `list_insert_ordered()` so it is placed at the appropriate place on the list. Finally, it blocks itself using `thread_block()`.
When `timer_interrupt()` is called, it simply goes through the `wait_list` and pop off all the threads whose `wake_up_tick` has past. Since the wait_list is ordered by `wake_up_tick`, we can traverse from the front of the list until the latest thread to be woken up. It should also call `thread_unblock()` to actually wake them up.

### 3. Synchronization
- wait_list
This list is shared among all threads and the scheduler. As list manipulation is not thread-safe in pintos, we decided to add a `wait_list_lock` to make sure no two entities are messing with the list at the same time.
In particular, when a thread calls `timer_sleep()`, it should use `lock_acquire()` to acquire `wait_list_lock`, add itself to the `wait_list`, use `lock_release()` to release `wait_list_lock`, and finally call `thread_block()`. `timer_interrupt()` also needs to check if a lock is held, but it should not try to acquire the lock; instead, it should disable interrupts while accessing the list.
Lastly, there is an edge case where the thread might be interrupted before it blocks itself, but after adding itself to the list. This is fortunately easy to check by `timer_interrupt()`, which should only unblock a thread if it has been blocked.

### 4. Rationale
Our biggest design decision is to create a variable for indicating when to wake up a thread. At first our vanilla solution was to simply store the `tick` variable passed into `timer_sleep` directly in the thread, but it turned out that updating this value for all the threads would be extremely inefficient. Instead, we realized that we could calculate the global tick the thread should reach before it is unblocked, and so we decided to use `wake_up_tick`. This way, we do not have to tediously update each thread’s wait time at every tick.
Another issue we pondered upon was whether the use of `wait_list_lock` would cause `timer_interrupt()` to not sleep on time, for that it might need to wait for another thread to finish inserting itself to `wait_list`. This wait time turned out to be inevitable, since multiple threads absolutely should not mess with the same list, especially that it is sorted.
In terms of conceptualization, this task is not very difficult, and so the idea boils down to inserting threads into a queue to wait for being woken up. In terms of coding, as we do not need to change many functions, it should not be too messy either.
Lastly, the features implemented for this task is orthogonal to the other two tasks, and by its nature independent of most other parts of the OS, so we expect that it would not be too difficult to accommodate new features, unless those features directly relate to this task.

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
/* The number of priority queues we have */
#define QUEUE_SIZE 64

 
/* The priority queues for holding threads ready to run */
static struct list priority_queue[QUEUE_SIZE];
 
/* This index keeps track of the first non-empty list in priority_queue */
static int queue_first_index;
 
/* The number of threads in the ready queue */
static int ready_threads;
 
/* Load average */
static fixed_point_t load_average;
 
/* This function will be modified to initialize the new fields in thread */
struct void init_thread(struct thread *t, const char *name, int priority);
 
/* This function is modified to initialize load_average, queue_index, ready_threads, and the lists in priority_queue;
void thread_init(void);
 
/* The following functions are slightly modified to increment/decrement ready_threads when appropriate*/
void thread_unblock(struct thread *t);
void thread_block(void);
void thread_exit(void);
 
/* This function is modified to run the threads in the list with the highest priority*/
static struct thread *next_thread_to_run(void);
 
/* This function is modified to update values including recent_cpu, load_aveage, and threads’ effective priorities*/
void thread_tick(void);
 
/* These functions will be implements to set/get the respective values of a thread */
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

Using `queue_first_index`, we can get the non-empty list of ready threads with the highest priority, and treat that list as `ready_list` to run the threads in it. We set `queue_first_index` to `-1` if there are not any ready threads, in which case we would run idle_thread.

- Updating `recent_cpu`, `load_average`, and thread `effective_priority`

All of these are performed in `thread_tick()`. At every tick, we increment the current thread’s `recent_cpu`; at every 4 ticks (checking if ticks is divisible by 4), we recompute the current thread’s priority, clamping it between `PRI_MIN` and `PRI_MAX`; at every `TIMER_FREQ`, we update the `load_average` (global value, updated using `ready_threads`), then `recent_cpu`, and finally `effective_priority` of each thread.
Lastly, we move threads around `priority_queue` based on their new priority values.

- Updating `queue_first_index`.

This value is updated in `thread_tick` and `next_thread_to_run`. In `thread_tick`, `queue_first_index` may be changed if we move threads into new `priority_queue` lists, and this can be updated on-the-fly; in `next_thread_to_run`, if the list pointed to by `queue_first_index` is empty after we run a thread, we decrement `queue_first_index` until it finds the next non-empty list or reaches -1.


### 3. Synchronization
Everything happens inside `timer_interrupt()`, which runs in an external interrupt context, so there will not be synchronization issues.


### 4. Rationale
Our biggest decision was where to put all the updates. Putting it inside `timer_interrupt()` ended up being the most natural choice, as it is run in an external interrupt context and it is where we keep track of the ticks. We are concerned that this may make `timer_interrupt` too slow, so we moved all the computation to `thread_tick()`, which takes place after we unblock threads (task 1).
Seeing that `recent_cpu` only changes every second for ready threads, we decided to update their priorities every second, after re-computations of `load_average` and `recent_cpu`.


## Additional Question
#### 1. Test for Bugs

#### 2. MLFQS Scheduler
timer ticks | R(A) | R(B) | R(C) | P(A) | P(B) | P(C) | thread to run
------------|------|------|------|------|------|------|--------------
 0          |      |      |      |      |      |      |
 4          |      |      |      |      |      |      |
 8          |      |      |      |      |      |      |
12          |      |      |      |      |      |      |
16          |      |      |      |      |      |      |
20          |      |      |      |      |      |      |
24          |      |      |      |      |      |      |
28          |      |      |      |      |      |      |
32          |      |      |      |      |      |      |
36          |      |      |      |      |      |      |


#### 3. Resolve Ambiguities

