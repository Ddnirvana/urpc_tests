#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>


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
	struct urpc_msg msgs[RING_LEN];
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

struct urpc_channel* translate_to_channel(void* addr);
int init_channel(struct urpc_channel* chan);

int ring_is_full(unsigned int head, unsigned int tail);
int ring_is_empty(unsigned int head, unsigned int tail);

int wait_for_request(struct urpc_channel* chan, char* data);
int wait_for_response(struct urpc_channel* chan, char* data);

int send_call_msg(struct urpc_channel* chan, char* data, int len);
int send_resp_msg(struct urpc_channel* chan, char* data, int len);
unsigned long long rdtsc(void);
