#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "queue.h"
#include "sem.h"
#include "thread.h"

//static queue_t waiting;

struct semaphore {
	size_t count;
	queue_t waiting;
};

sem_t sem_create(size_t count)
{
	sem_t sem = malloc(sizeof(struct semaphore));
	if (!sem)
		return NULL;

	sem->count = count;
	sem->waiting = queue_create(); // create waiting queue
	return sem;
}

int sem_destroy(sem_t sem)
{
	if (!sem || queue_length(sem->waiting) > 0)
		return -1;

	free(sem);
	return 0;
}

int sem_down(sem_t sem)
{
	if (!sem)
		return -1;

	enter_critical_section(); // to ensure mutual exclusion

	while (sem->count == 0) { // block thread if count is 0
		pthread_t tid = pthread_self();
		queue_enqueue(sem->waiting, (void*) tid);
		thread_block();
	} 
	sem->count--;
	exit_critical_section();
	return 0;
}

int sem_up(sem_t sem)
{
	if (!sem)
		return -1;

	enter_critical_section(); // to ensure mutual exclusion
	sem->count++;

	if(queue_length(sem->waiting) > 0) { //if waiting queue is not empty
		pthread_t* tid;
		queue_dequeue(sem->waiting, (void**) &tid);
		thread_unblock(*tid);
	}  
	exit_critical_section();
	return 0;
}
