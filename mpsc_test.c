/* 2015 Daniel Bittman <danielbittman1@gmail.com>: http://dbittman.github.io/ */
#include <stdio.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include "mpsc.h"

#include <time.h>
#include <unistd.h>

mpscq *queue;
_Atomic int amount_produced = ATOMIC_VAR_INIT(0);
_Atomic int amount_consumed = ATOMIC_VAR_INIT(0);
_Atomic bool done = ATOMIC_VAR_INIT(false);
_Atomic int retries = ATOMIC_VAR_INIT(0);
_Atomic long long total = ATOMIC_VAR_INIT(0);
#define NUM_ITEMS 100000
#define NUM_THREADS 100

struct item {
	_Atomic int sent, recv;
};

struct item items[NUM_THREADS][NUM_ITEMS];

void *producer_main(void *x)
{
	long tid = (long)x;
	struct timespec start, end;
    long t = 0;
    int cnt  = 0;
	for(int i=0;i<NUM_ITEMS;i++) {
		assert(atomic_fetch_add(&items[tid][i].sent, 1) == 0);
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
		bool r = mpscq_push(queue, &items[tid][i]);
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
		if(r) {
			cnt++;
			int ns = end.tv_nsec - start.tv_nsec;
            t += ns < 0 ? ns + 1e9: ns;
		} else {
			items[tid][i].sent = 0;
			i--;
			retries++;
            usleep(10);
		}
	}
	for(int i=0;i<NUM_ITEMS;i++) {
		assert(items[tid][i].sent != 0);
	}
    atomic_fetch_add(&amount_produced, cnt);
    atomic_fetch_add(&total, t);
	atomic_thread_fence(memory_order_seq_cst);
	pthread_exit(0);
}

void *consumer_main(void *x)
{
	(void)x;
	bool doublechecked = false;
	while(true) {
		void *ret = mpscq_pop(queue);
		if(ret) {
			atomic_fetch_add(&amount_consumed, 1);
			struct item *it = ret;
			assert(atomic_fetch_add(&it->sent, 1) == 1);
			assert(atomic_fetch_add(&it->recv, 1) == 0);
			doublechecked = false;
		} else if(done && doublechecked) {
			break;
		} else if(done) {
			doublechecked = true;
            usleep(10);
		}
	}
	assert(!mpscq_pop(queue));
	atomic_thread_fence(memory_order_seq_cst);

	pthread_exit(0);
}


int main(int argc, char **argv)
{
	int num_producers = NUM_THREADS;
	pthread_t producers[num_producers];
	pthread_t consumer;

	struct timespec start, end;

	for(int i=0;i<NUM_THREADS;i++) {
		for(int j=0;j<NUM_ITEMS;j++) {
			items[i][j].sent = 0;
			items[i][j].recv = 0;
		}
	}


	queue = mpscq_create(8000);

	pthread_create(&consumer, NULL, consumer_main, NULL);
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);
	for(long i=0;i<num_producers;i++) {
		pthread_create(&producers[i], NULL, producer_main, (void *)i);
	}

	for(int i=0;i<num_producers;i++) {
		pthread_join(producers[i], NULL);
	}
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end);
	done = true;
	pthread_join(consumer, NULL);

	atomic_thread_fence(memory_order_seq_cst);
	
	for(int i=0;i<num_producers;i++) {
		for(int j=0;j<NUM_ITEMS;j++) {
			if(items[i][j].sent != 2) {
				printf(":(%d %d): %d %d, %d %d\n", i, j, items[i][j].sent,
						items[i][j].recv, amount_produced, amount_consumed);
			}
			assert(items[i][j].sent == 2);
			assert(items[i][j].recv == 1);
		}
	}
	
	long ms = (end.tv_sec - start.tv_sec) * 1000;
	ms += (end.tv_nsec - start.tv_nsec) / 1000000;

	fprintf(stdout, "all: %ums\t ops: %d\n",
			(int)(total/1e6) , amount_produced);

	fprintf(stdout, "retry: \t%d\t all: %ldms\t push/op: %ldus\n",
			retries, ms, (long)(total / amount_produced/1e3));
	assert(amount_produced == amount_consumed);
	exit(amount_produced != amount_consumed);
    
}

