Final Report for Project 1: Threads
===================================


## Group Members

* Daiyaan Arfeen <daiyaanarfeen@berkeley.edu>
* Patrick Lu <xingyulu0701@berkeley.edu>
* Claire Liu <liuzmin@berkeley.edu>
* William Zhang <qiaowei@berkeley.edu>

# Changes made and reasons

## Task 1 Efficient Alarm Clock
For task 1, instead of keeping a lock on the list of sleeping threads, we disabled interrupts when adding to the list. This ended up being much simpler and avoided issues we were having with the task 2 bugs affecting task 1. We also added a `list_elem` to the thread struct specific to the list of sleeping threads. </br>

## Task 2 Priority Scheduler
For task 2, we barely strayed at all from our initial design. We did have to add a `list_elem` in the thread struct specifically for adding to the blocking thread’s list of waiting threads. We also called `thread_yield` in `lock_release` and `thread_set_priority` to ensure the highest priority thread was always the one running. Other than these small changes the original design stands as the design we implemented. </br>

## Task 3 MLFQS
For the task 3, we followed our initial design for most part, but we removed the active_thread (which was intended for storing all running threads in the past four ticks but turned out to be unneccessary). Instead, we simply updated the priority of `thread_current` every four ticks in `thread_tick`. Our code was greatly simplified and less prune to bugs.</br>
In addition, we added `list_elem` to thread struct so that we could keep track of threads in `priority_list`, which holds threads ready to run, sorted by thread priority. </br>
To reduce code duplicity, we also added helper functions : `calculate_load_average`, `calculate_recent_cpu`, `calculate_priority` and `add_to_priority_list`, since the same calculations were used in different places several times. By constructing reusable codes, we made our code simpler, improved its readability and made it easier to debug. 


# Reflection on the project

William Zhang and Claire Liu are mostly responsible for task 3. We pair-programed the initial codes, took turn to debug programs and improved the code quality together. The overall cooperation was great. We believe one improvement we should do next time is to push our codes to github more often and maintain organized logs of changes. In this way we can record how we debug different problems. If we see similar bugs again next time, we can track our changes in the past to get some clues on what direction we should head to. 

Daiyaan Arfeen and Patrick Lu worked mostly on task 1 and task 2. Task 1 was completed entirely through pair-programming; for task 2 we pair-programmed to write the initial code and debugged individually for the most part. The cooperation was very efficient and productive and we were able to complete both tasks with almost no serious issues in the code at any point. Daiyaan still has much to learn about Git and version control, but he did his best and will continue to learn from his teammates who seem much more experienced with it. Patrick, on the other hand, needs to familiarize himself more with gdb. Overall, our code was clean, simple, and understandable so we think we did a good job. 

