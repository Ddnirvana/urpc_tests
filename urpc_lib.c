#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "urpc_lib.h"


#if 0
#define CACHE_LINE_SIZE 64
struct urpc_msg {
	// 0: empty
	// 1: ready
	// others: invalid
	// FIXME(DD): should use enum later
	// This is set to volatile to ensure not be optimized
	volatile char status;
	char data[CACHE_LINE_SIZE-1];
};

#define RING_LEN 64
struct urpc_ring {
	struct msgs[RING_LEN];
};

struct urpc_channel {
	struct urpc_ring call_ring;
	struct urpc_ring resp_ring;
	// We put the header here to ensure that the call ring
	// and the resp_ring is CACHE_LINE_SIZE_ALIGNED
	volatile unsigned int call_header;
	volatile unsigned int resp_header;
	volatile unsigned int call_tail;
	volatile unsigned int resp_tail;
} __attribute__((align(64)));
#endif

/*
 * Simple type translation from any adr to the channel
 * */
struct urpc_channel* translate_to_channel(void* addr) {
	if (addr)
		return (struct urpc_channel*) addr;
	else
		return NULL;
}

int ring_is_full(unsigned int head, unsigned int tail) {
	return (((tail+1) % RING_LEN) == head);
}

int ring_is_empty(unsigned int head, unsigned int tail) {
	return (tail == head);
}

/*
 * Init the fields in the urpc_channel
 * */
int init_channel(struct urpc_channel* chan) {
	if (!chan)
		return -1;

	/*
	 * When header == tail, means ring empty
	 * When (tail+1) % ring_len == head, means ring full
	 * */
	chan->call_header = 0;
	chan->resp_header = 0;
	chan->call_tail = 0;
	chan->resp_tail = 0;

	memset(&chan->call_ring, 0, CACHE_LINE_SIZE * RING_LEN);
	memset(&chan->resp_ring, 0, CACHE_LINE_SIZE * RING_LEN);

	return 0;
}

int wait_for_request(struct urpc_channel* chan, char* data) {
	if (!chan) return -1;

	// Blocking when the ring is empty
	while (ring_is_empty(chan->call_header, chan->call_tail)) {}

	// Blocking when the header entry is not ready
	while (chan->call_ring.msgs[chan->call_header].status == 0) {
	}

	while (chan->call_ring.msgs[chan->call_header].status != 1) {
		fprintf(stderr, "[URPC@%s] unexpected msg status:%d\n",
				__func__,
				chan->call_ring.msgs[chan->call_header].status);
		return -1;
	}

	memcpy(data, (const void*) chan->call_ring.msgs[chan->call_header].data,
			CACHE_LINE_SIZE-1);
	chan->call_ring.msgs[chan->call_header].status = 0; //set to empty
	chan->call_header = (chan->call_header+1) % RING_LEN; //update header

	return 0;
}

int wait_for_response(struct urpc_channel* chan, char* data) {
	if (!chan) return -1;

	// Blocking when the ring is empty
	while (ring_is_empty(chan->resp_header, chan->resp_tail)) {}

	// Blocking when the header entry is not ready
	while (chan->resp_ring.msgs[chan->resp_header].status == 0) {
	}

	while (chan->resp_ring.msgs[chan->resp_header].status != 1) {
		fprintf(stderr, "[URPC@%s] unexpected msg status:%d\n",
				__func__,
				chan->resp_ring.msgs[chan->resp_header].status);
		return -1;
	}

	memcpy(data, (const void*) chan->resp_ring.msgs[chan->resp_header].data,
			CACHE_LINE_SIZE-1);
	chan->resp_ring.msgs[chan->resp_header].status = 0; //set to empty
	chan->resp_header = (chan->resp_header+1) % RING_LEN; //update header

	return 0;
}

int send_call_msg(struct urpc_channel* chan, char* data, int len) {
	if (!chan) return -1;

	if (len > CACHE_LINE_SIZE-1)
		len = CACHE_LINE_SIZE - 1;
	// Blocking when the ring is full
	while (ring_is_full(chan->call_header, chan->call_tail)) {}

	// Blocking when the tail entry is not empty
	while (chan->call_ring.msgs[chan->call_tail].status != 0) {
	}

	memcpy(chan->call_ring.msgs[chan->call_tail].data, (const void*) data,
			len);
	chan->call_ring.msgs[chan->call_tail].status = 1; //set to used
	chan->call_tail = (chan->call_tail + 1) % RING_LEN; //update tail

	return 0;
}

int send_resp_msg(struct urpc_channel* chan, char* data, int len) {
	if (!chan) return -1;

	if (len > CACHE_LINE_SIZE-1)
		len = CACHE_LINE_SIZE - 1;
	// Blocking when the ring is full
	while (ring_is_full(chan->resp_header, chan->resp_tail)) {}

	// Blocking when the tail entry is not empty
	while (chan->resp_ring.msgs[chan->resp_tail].status != 0) {
	}

	memcpy(chan->resp_ring.msgs[chan->resp_tail].data, (const void*) data,
			len);
	chan->resp_ring.msgs[chan->resp_tail].status = 1; //set to used
	chan->resp_tail = (chan->resp_tail + 1) % RING_LEN; //update tail

	return 0;
}

/*
 * Read cycles
 * */
unsigned long long rdtsc(){
	unsigned int lo,hi;
	__asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
	return ((unsigned long long)hi << 32) | lo;
}

