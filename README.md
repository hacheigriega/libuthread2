# Introduction  
Authors: Hyoung-yoon Kim and Jacqueline Sun   
This is the report for project 3: User-level thread library part 2    

# Phase 1  
We implemented the semamphore API in this phase. Our sempaphore data structure   
contained a count integer to indicate the amount of the resource available. Each     
semaphore has access to a queue of waiting threads, hence the queue_t waiting in    
the struct.  

The function sem_down(sem_t sem) followed the structure of code as given in the     
lecture slides. After checking for the existence of the semaphore, we enter     
the critical section to ensure mutual exclusion, i.e. we don't want other  
threads to be calling sem_down while a thread is currently in the process of   
receiving the resource (as this doesn't happen in an atomic way, ensuring mutual  
exclusion is necessary to avoid a race condition). Before returning, we   
decrement the semaphore count and exit the critical section. For consistency, we  
follow the same enter and exit critical section structure for sem_up.     

The function sem_up(sem_t sem) also followed the structure of code given in the   
lecture slides. We increment the count. If there are threads waiting for the   
resource, i.e. the waiting queue is not empty, we dequeue the waiting thread and   
unblock it.     

# Phase 2
We implemented a naive version of the TPS API in this phase, then added TPS     
protections, and finally implemented copy-on-write cloning. For our naive  
implementation, we had a TPS data structure that contained the thread tid and   
a mempage address. To implement copy-on-write cloning, the mempage address was  
changed to a pointer to a mempage data structure that containted the address and   
a count of how many threads point to the mempage.  

In our naive implementation of tps_create(), we created the memory page using   
mmap() with no protections. Therefore, in calling tps_write(), tps_read(), and   
tps_clone() threads could freely read and write to memory pages.     

In tps_clone(), tps_write(), and tps_read() we used a helper function find_TPS()   
we wrote to find a TPS by tid. This was used with queue_iterate() to find the   
correct thread to read/write/clone from.  

To improve our naive implementation, we added protections to the mempages along  
with dissociating the TPS object with the memory page. So, in tps_create() we  
created the memory page using mmap() with PROT_READ and PROT_Write. Then, when  
we called tps_write() or tps_read(), we temporarily changed the protections  
using mprotect() with the appropriate flag.  

Our function tps_write() changed the most from the naive implementation to  
implement copy-on-write cloning. At a call to tps_write(), we created a new   
mempage structure that was an a copy of the original mempage. We gave the copy  
write access so that it could be written to from the buffer and set the current  
thread to point at the copy.  

We also implemented the seg fault handler in this phase. To detect whether the   
fault originated from an out of bounds TPS access instead of an actual program   
seg fault, we wrote a find_fault() helper function that compares the fault  
address with the addresses of the mempages. If there is a match, we print out   
the specific TPS protection error message, otherwise we raise the segfault  
signal.  

# Testing 
The tester programs provided by the instructor check the basic functionalities    
of our implementations. The given file tps.c confirms that our program handles    
the basic read, write, and clone operations properly. In order to test signal    
handling and corner cases, we created a new program tps_testsuite.c. This  
program includes tests all the error cases we have considered, such as out of    
bounds read or write, negative offset or length, creating a duplicate TPS, and   
missing TPS. In addition, based on the instructor's recommendation, we    
intentionally caused a TPS protection error and verified that our program    
distinguishes this signal from a segmentation fault error.  


 



