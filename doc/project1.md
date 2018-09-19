Design Document for Project 1: Threads
======================================

## Group Members

* Mohammad Arfeen <daiyaanarfeen@berkeley.edu>
* Xingyu Lu <xingyulu0701@berkeley.edu>
* Claire Liu <liuzmin@berkeley.edu>
* Qiaowei Zhang <qiaowei@berkeley.edu>


## Tast 1 Efficient Alarm Clock
### 1. Data structure and functions
In threads/thread.h:
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
In threads/thread.c:
```
/* This function will be modified to initialize the new fields above */
struct void init_thread(struct thread *t, const char *name, int priority);
```
In devices/timer.c:
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

