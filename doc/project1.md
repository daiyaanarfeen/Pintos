Design Document for Project 1: Threads
======================================

## Group Members

* FirstName LastName <email@domain.example>
* FirstName LastName <email@domain.example>
* FirstName LastName <email@domain.example>
* FirstName LastName <email@domain.example>


## Task 3 MLFQS
### Data structure and functions
1. In threads/thread.h
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
2. In threads/thread.c
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


### Algorithm
1. Niceness
This is a field initialized with the thread, and can be set again using `thread_set_nice();`

2. Finding the next thread to run
Using `queue_first_index`, we can get the non-empty list of ready threads with the highest priority, and treat that list as `ready_list` to run the threads in it. We set `queue_first_index` to `-1` if there are not any ready threads, in which case we would run idle_thread.

3. Updating `recent_cpu`, `load_average`, and thread `effective_priority`
All of these are performed in `thread_tick()`. At every tick, we increment the current thread’s `recent_cpu`; at every 4 ticks (checking if ticks is divisible by 4), we recompute the current thread’s priority, clamping it between `PRI_MIN` and `PRI_MAX`; at every `TIMER_FREQ`, we update the `load_average` (global value, updated using `ready_threads`), then `recent_cpu`, and finally `effective_priority` of each thread.
Lastly, we move threads around `priority_queue` based on their new priority values.

4. Updating `queue_first_index`.
This value is updated in `thread_tick` and `next_thread_to_run`. In `thread_tick`, `queue_first_index` may be changed if we move threads into new `priority_queue` lists, and this can be updated on-the-fly; in `next_thread_to_run`, if the list pointed to by `queue_first_index` is empty after we run a thread, we decrement `queue_first_index` until it finds the next non-empty list or reaches -1.


### Synchronization
Everything happens inside `timer_interrupt()`, which runs in an external interrupt context, so there will not be synchronization issues.


### Rationale
Our biggest decision was where to put all the updates. Putting it inside `timer_interrupt()` ended up being the most natural choice, as it is run in an external interrupt context and it is where we keep track of the ticks. We are concerned that this may make `timer_interrupt` too slow, so we moved all the computation to `thread_tick()`, which takes place after we unblock threads (task 1).
Seeing that `recent_cpu` only changes every second for ready threads, we decided to update their priorities every second, after re-computations of `load_average` and `recent_cpu`.


## Additional Question
### Test for Bugs

### MLFQS Scheduler
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


### Resolve Ambiguities