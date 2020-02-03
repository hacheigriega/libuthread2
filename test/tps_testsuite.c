#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tps.h>
#include <sem.h>

void *latest_mmap_addr; // global variable to make the address returned by mmap accessible

void *__real_mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off);
void *__wrap_mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off)
{
	latest_mmap_addr = __real_mmap(addr, len, prot, flags, fildes, off);
	return latest_mmap_addr;
}

void *my_thread(void *arg)
{
	char *tps_addr;

	/* Create TPS */
	tps_create();

	/* Get TPS page address as allocated via mmap() */
	tps_addr = latest_mmap_addr;

	/* Cause an intentional TPS protection error */
	tps_addr[0] = '\0';
	return NULL;

}

int main(int argc, char **argv)
{
	tps_init(1);

	/* check error cases */
	assert(tps_destroy() == -1); // no TPS to destroy
	assert(tps_create() == 0); // should be fine
	assert(tps_create() == -1); // TPS already exists
	assert(tps_destroy() == 0); // should be fine
	assert(tps_create() == 0); // should be fine

	char buffer[10];
	assert(tps_read(TPS_SIZE, 10, buffer) == -1); // out of bounds reading
	assert(tps_read(-1, 10, buffer) == -1); // negative offset

	char to_write[] = "123456789";
	assert(tps_write(TPS_SIZE, 10, to_write) == -1); // out of bounds writing
	assert(tps_write(-1, 10, to_write) == -1); // negative offset

	assert(tps_read(0, TPS_SIZE, NULL) == -1); // passing in NULL buffer
	assert(tps_write(0, TPS_SIZE, NULL) == -1); // passing in NULL buffer

	my_thread(NULL);
	return 0;

}
