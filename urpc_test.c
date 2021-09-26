#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>

#include "urpc_lib.h"

void dd_server(void){
        printf("hello world in %s\n",__func__);
        return ;
}

#define TEST_LOOP 100

void dump_results(unsigned long long*client_log, unsigned long long*server_log){
	fprintf(stderr, "[URPC Test] Results:\n");
	fprintf(stderr, "[Formats] Test_ID, call latency (cycles), resp latency (cycles)\n");
	for (int i=0; i<TEST_LOOP; i++){
		fprintf(stderr, "\t%d\t%llu\t%llu\n",
				i, server_log[i*2] - client_log[i*2],
				client_log[i*2+1] - server_log[i*2+1]);
	}
	fprintf(stderr, "[URPC Test] Results done\n");
}

void server_routine(struct urpc_channel* chan, unsigned long long* time_log){
	const char *msg ="Hello, I am server";
	char call_data[CACHE_LINE_SIZE];
	char resp_data[CACHE_LINE_SIZE];
	int ret;

	/* Pin core*/
	cpu_set_t cpuset;
	pthread_t thread = pthread_self();
	CPU_ZERO(&cpuset);
	for (int i=0; i<4; i++)
		CPU_SET(i, &cpuset);
	ret = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
	if (ret != 0){
		fprintf(stderr,"[URPC Test@%s] Set affinity error\n",
				__func__);
		exit(-1);
	}

	for (int i=0; i<TEST_LOOP; i++) {
		wait_for_request(chan, call_data);
		*time_log = rdtsc(); //after call

		time_log++;
		//fprintf(stderr, "%s got a call: %s\n",
		//		__func__, call_data);

		memcpy(resp_data, msg, strlen(msg));
		*time_log = rdtsc(); //before resp
		send_resp_msg(chan, resp_data, strlen(msg));
		time_log++;
	}
}

//uint64_t rdtsc(void);
void client_routine(struct urpc_channel* chan, unsigned long long* time_log){
	const char *msg ="Hello, I am client";
	char call_data[CACHE_LINE_SIZE];
	char resp_data[CACHE_LINE_SIZE];
	int ret;

	/* Pin core*/
	cpu_set_t cpuset;
	pthread_t thread = pthread_self();
	CPU_ZERO(&cpuset);
	for (int i=4; i<8; i++)
		CPU_SET(i, &cpuset);
	ret = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
	if (ret != 0){
		fprintf(stderr,"[URPC Test@%s] Set affinity error\n",
				__func__);
		exit(-1);
	}

	for (int i=0; i<TEST_LOOP; i++) {
		memcpy(call_data, msg, strlen(msg));
		*time_log = rdtsc(); //before call
		send_call_msg(chan, call_data, strlen(msg));

		time_log++;

		wait_for_response(chan, resp_data);
		*time_log = rdtsc(); //after resp

		time_log++;

		resp_data[CACHE_LINE_SIZE-1] = '\0'; //force to break at the end
	//	fprintf(stderr, "%s got a response: %s\n",
	//			__func__, resp_data);
	}
}

int main() {
        int ret,i;
        unsigned long long sptbr;
        unsigned long long t_begin,t_end;
        unsigned long *entry_value;
        unsigned long *cap_base;
        unsigned long *stack_base;
        unsigned long child_sptbr;
        unsigned long child_dd_server_entry;
        pid_t pid;
	struct urpc_channel* chan;

        unsigned long * shm_p;   // for IPC
        unsigned long long* shm_log; // for Logging results

        printf("[URPC Test] The test program to evaluate URPC latency\n");

        shm_p = (unsigned long *) mmap(NULL, sizeof(struct urpc_channel),
			PROT_WRITE | PROT_READ,
			MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (MAP_FAILED == shm_p){
                printf("error during mmap, exit\n");
                return -1;
        } else {
		fprintf(stderr, "[URPC Test] shm created successfully!\n" );
	}

        shm_log = (unsigned long long *) mmap(NULL, 4096,
			PROT_WRITE | PROT_READ,
			MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (MAP_FAILED == shm_log){
                printf("error during mmap, exit\n");
                return -1;
        } else {
		fprintf(stderr,
			"[URPC Test] results buffer created successfully!\n" );
	}


	chan = translate_to_channel(shm_p);
	init_channel(chan);

	fprintf(stderr, "[URPC Test] Init comm channel finished\n");

        pid = fork();
        if (pid == -1){
                printf("error during fork, exit\n");
                return -1;
        }
        if (pid == 0){
		client_routine(chan, shm_log);
		dump_results(shm_log, shm_log+256);
                //while (1){ sleep(1);}
        }else{
		server_routine(chan, shm_log+256);
                //while (1){ sleep(1);}
        }

        printf("[URPC Test] Test finished\n");
        return 0;
}
