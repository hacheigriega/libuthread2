#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "queue.h"
#include "thread.h"
#include "tps.h"

static queue_t queue;

typedef struct mempage { 
	void* address;
	int counter; // keep track of how many TPS share a mempage

} mempage;

typedef struct TPS_t {
	pthread_t tid;
	struct mempage* page;
} TPS_t;

// In header file: #define TPS_SIZE 4096
static void segv_handler(int sig, siginfo_t *si, void *context);

/*
 * tps_init - Initialize TPS
 * @segv - Activate segfault handler
 *
 * Initialize TPS API. This function should only be called once by the client
 * application. If @segv is different than 0, the TPS API should install a
 * page fault handler that is able to recognize TPS protection errors and
 * display the message "TPS protection error!\n" on stderr.
 *
 * Return: -1 if TPS API has already been initialized, or in case of failure
 * during the initialization. 0 if the TPS API was successfully initialized.
 */
int tps_init(int segv)
{
	// -1 if TPS has already been initialized
	queue = queue_create();

	if (segv) {
		struct sigaction sa;
		
		sa.sa_sigaction = segv_handler; 
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_SIGINFO;
		sa.sa_sigaction = segv_handler;
		sigaction(SIGBUS, &sa, NULL);
		sigaction(SIGSEGV, &sa, NULL);
    	}

	return 0;
}

/*Find TPS with fault */
int find_fault(void* data, void* arg) {
	TPS_t *ptr = (TPS_t*) data;
	void* pfault = arg;
	if (ptr->page->address == pfault)
		return 1;

	return 0;
}

static void segv_handler(int sig, siginfo_t *si, void *context)
{
	/*
	* Get the address corresponding to the beginning of the page where the
	* fault occurred
	*/
	void *p_fault = (void*)((uintptr_t)si->si_addr & ~(TPS_SIZE - 1));

	TPS_t *foundTPS = NULL;
	/*
	* Iterate through all the TPS areas and find if p_fault matches one of them
	*/
	queue_iterate(queue, find_fault, p_fault, (void**)&foundTPS);
	if (foundTPS != NULL)
		/* Printf the following error message */
		fprintf(stderr, "TPS protection error!\n");

	/* In any case, restore the default signal handlers */
	signal(SIGSEGV, SIG_DFL);
	signal(SIGBUS, SIG_DFL);
	/* And transmit the signal again in order to cause the program to crash */
	raise(sig);
}

/* Find a TPS based on tid */
int find_TPS(void *data, void *arg) {
	TPS_t *ptr = (TPS_t*)data;
	pthread_t *thdID = (pthread_t*)arg;
	if (ptr->tid == *thdID)
		return 1;
	return 0;
}


/*
 * tps_create - Create TPS
 *
 * Create a TPS area and associate it to the current thread.
 *
 * Return: -1 if current thread already has a TPS, or in case of failure during
 * the creation (e.g. memory allocation). 0 if the TPS area was successfully
 * created.
 */
int tps_create(void)
{
	// Look for current thread in queue
	TPS_t *foundTPS = NULL;
	pthread_t tid = pthread_self();
	queue_iterate(queue, find_TPS, (void*) &tid, (void**)&foundTPS);
	if (foundTPS != NULL) {
		return -1; // current thread already has a TPS
	}

	// Create a TPS for the current thread and enqueue
	TPS_t *tps = malloc(sizeof(TPS_t));
	tps->tid = pthread_self();
	tps->page = malloc(sizeof(struct mempage));
	tps->page->address = mmap(NULL, TPS_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANON, 0, 0);
	tps->page->counter = 1; // one TPS has the mempage
	if (!tps->page->address) { // if mmap error
		perror("mmap allocation error");
		return -1;
	}
	queue_enqueue(queue, tps);
	
	return 0;
}

/*
 * tps_destroy - Destroy TPS
 *
 * Destroy the TPS area associated to the current thread.
 *
 * Return: -1 if current thread doesn't have a TPS. 0 if the TPS area was
 * successfully destroyed.
 */
int tps_destroy(void)
{
	TPS_t *foundTPS = NULL;
	pthread_t tid = pthread_self();
	queue_iterate(queue, find_TPS, (void*) &tid, (void**)&foundTPS);
	if (foundTPS == NULL) {
		return -1; // current thread doesn't have a TPS
	}

	// destroy current thread's TPS
	munmap(foundTPS->page->address, TPS_SIZE);
	free(foundTPS->page);
	queue_delete(queue, foundTPS);
	
	return 0;
}

/*
 * tps_read - Read from TPS
 * @offset: Offset where to read from in the TPS
 * @length: Length of the data to read
 * @buffer: Data buffer receiving the read data
 *
 * Read @length bytes of data from the current thread's TPS at byte offset
 * @offset into data buffer @buffer.
 *
 * Return: -1 if current thread doesn't have a TPS, or if the reading operation
 * is out of bound, or if @buffer is NULL, or in case of internal failure. 0 if
 * the TPS was successfully read from.
 */
int tps_read(size_t offset, size_t length, char *buffer)
{
	if ((int)offset < 0 || (int)length < 0) // negative offset or length
		return -1; 
	if (!buffer) // if buffer is NULL
		return -1; 
	if (offset + length > TPS_SIZE) // if out of bound
		return -1;

	TPS_t *foundTPS = NULL;
	pthread_t tid = pthread_self();
	queue_iterate(queue, find_TPS, (void*) &tid, (void**)&foundTPS);
	if (foundTPS == NULL) {
		return -1; // current thread doesn't have a TPS
	}

	mprotect(foundTPS->page->address, TPS_SIZE, PROT_READ);
	// read into @buffer
	char* TPSmem = foundTPS->page->address; // to do char pointer arithmetic
	memcpy(buffer, TPSmem + offset, length); // memcpy(dest, src, # of bytes)
	mprotect(foundTPS->page->address, TPS_SIZE, PROT_NONE);
	return 0;
}

/*
 * tps_write - Write to TPS
 * @offset: Offset where to write to in the TPS
 * @length: Length of the data to write
 * @buffer: Data buffer holding the data to be written
 *
 * Write @length bytes located in data buffer @buffer into the current thread's
 * TPS at byte offset @offset.
 *
 * If the current thread's TPS shares a memory page with another thread's TPS,
 * this should trigger a copy-on-write operation before the actual write occurs.
 *
 * Return: -1 if current thread doesn't have a TPS, or if the writing operation
 * is out of bound, or if @buffer is NULL, or in case of failure. 0 if the TPS
 * was successfully written to.
 */
int tps_write(size_t offset, size_t length, char *buffer)
{
	if ((int)offset < 0 || (int)length < 0) // negative offset or length
		return -1;
	if (!buffer) // if buffer is NULL
		return -1; 
	if (offset + length > TPS_SIZE) // if out of bound
		return -1;

	TPS_t *foundTPS = NULL;
	pthread_t tid = pthread_self();
	queue_iterate(queue, find_TPS, (void*) &tid, (void**)&foundTPS);
	if (foundTPS == NULL) {
		return -1; // current thread doesn't have a TPS
	}

	// must create a duplicate mempage if more than one TPSs are associated to the mempage
	if(foundTPS->page->counter > 1) {
		// allocate space for new mempage
		mempage* newpage = malloc(sizeof(mempage));
		newpage->address = mmap(NULL, TPS_SIZE, PROT_WRITE, MAP_PRIVATE | MAP_ANON, 0, 0);

		// copy from original mempage
		mprotect(foundTPS->page->address, TPS_SIZE, PROT_READ);
		memcpy(newpage->address, foundTPS->page->address, TPS_SIZE);
		mprotect(foundTPS->page->address, TPS_SIZE, PROT_NONE);

		foundTPS->page->counter--;
		newpage->counter = 1;	
		foundTPS->page = newpage;	
	}
	
	// write from @buffer
	mprotect(foundTPS->page->address, TPS_SIZE, PROT_WRITE);
	char* TPSmem = foundTPS->page->address; // to do char pointer arithmetic
	memcpy(TPSmem + offset, buffer, length); // memcpy(dest, src, # of bytes)
	mprotect(foundTPS->page->address, TPS_SIZE, PROT_NONE);

	return 0;
}

/*
 * tps_clone - Clone TPS
 * @tid: TID of the thread to clone
 *
 * Clone thread @tid's TPS. In the first phase, the cloned TPS's content should
 * copied directly. In the last phase, the new TPS should not copy the cloned
 * TPS's content but should refer to the same memory page.
 *
 * Return: -1 if thread @tid doesn't have a TPS, or if current thread already
 * has a TPS, or in case of failure. 0 is TPS was successfully cloned.
 */
int tps_clone(pthread_t tid)
{
	// find TPS of thread @tid
	TPS_t *srcTPS = NULL;
	queue_iterate(queue, find_TPS, (void*) &tid, (void**)&srcTPS);
	if (srcTPS == NULL) {
		return -1; // thread @tid doesn't have a TPS
	}

	// Look for current thread in queue
	TPS_t *currTPS = NULL;
	pthread_t curr_tid = pthread_self();
	queue_iterate(queue, find_TPS, (void*) &curr_tid, (void**)&currTPS);
	if (currTPS != NULL) {
		return -1; // current thread already has a TPS
	}
	
	// Create TPS and point to same reference page
	TPS_t *tps = malloc(sizeof(TPS_t));
	tps->tid = pthread_self();
	
	tps->page = srcTPS->page;
	tps->page->counter++;
	queue_enqueue(queue, tps);

	return 0;
}

