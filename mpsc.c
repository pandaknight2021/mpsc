#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "mpsc.h"
#include "spsc.h"


static block* _new_block(size_t block_size);
static size_t roundup2(size_t v);

/* multi-producer, single consumer queue *
 * Requirements: max must be >= 2 */
mpscq *mpscq_create(size_t block_size)
{
    mpscq* q = calloc(1, sizeof(mpscq));
    if(!q)  return NULL;

	q->in = q->out = 0;
    q->block_size = roundup2(block_size);
    q->tail = q->head = _new_block(q->block_size);
    if(!q->tail){
        free(q);
        return NULL;
    }
	
    q->_nblock_new = 1;
    q->_idle_queue = spscq_new(4096);
    atomic_thread_fence(memory_order_release);
	return q;
}

void mpscq_destroy(mpscq *q)
{
	block *b = q->tail;
    while(b){
        q->tail = q->tail->next;
        free(b);
        b = q->tail;
    }

    spscq_delete(q->_idle_queue);
    free(q);
}


static block* _get_block(mpscq *q,size_t block_size);
bool mpscq_push(mpscq *q, void *obj)
{
    if(!obj) return false;

	block* head = q->head;
	size_t w = atomic_fetch_add_explicit(&head->w, 1,memory_order_acquire);
	if(w > head->mask) {
		/* back off, queue is full */
		atomic_fetch_sub_explicit(&head->w, 1, memory_order_release);
		return false;
	}

	/* increment the head, which gives us 'exclusive' access to that element */
    //assert(head->buffer[w&head->mask] == 0);
    head->buffer[w&head->mask] = obj;
    //atomic_exchange_explicit(&head->buffer[w], obj, memory_order_release);
    atomic_fetch_add_explicit(&q->in, 1, memory_order_release);

    //the end position of buffer, need get a new block
    if(w == head->mask){   
        block* b = _get_block(q,q->block_size);
        //assert(b);
        head->next = b;                                 // old block -> new block
        q->head = b;
        //atomic_exchange_explicit(&q->head, b, memory_order_release);
    }

	return true;
}

static void _del_block(block* b);
static void _update_tail(mpscq *q);
// must be call in a single thread.
void *mpscq_pop(mpscq *q)
{
    if(mpscq_empty(q)) return NULL;
    block* tail = q->tail;
	void *ret = NULL;
    
    
	if(tail->r <= tail->mask)  {
        //ret = atomic_exchange_explicit(&(tail->buffer[tail->r]), NULL, memory_order_acquire);
        ret = tail->buffer[tail->r];
    }else{
        _update_tail(q);
    }    
 
    if(!ret) {
		/* a thread is adding to the queue, but hasn't done the atomic_exchange yet
		 * to actually put the item in. Act as if nothing is in the queue.
		 * Worst case, other producers write content to tail + 1..n and finish, but
		 * the producer that writes to tail doesn't do it in time, and we get here.
		 * But that's okay, because once it DOES finish, we can get at all the data
		 * that has been filled in. */
		return NULL;
	}
	
    ++tail->r;
    _update_tail(q);
    q->out++;

	return ret;
}


bool mpscq_empty(mpscq *q){
    block* tail = q->tail;
    block* head = q->head;
    //assert( tail && head);
    return (head == tail) && (tail->w == tail->r);
}


size_t mpscq_count(mpscq *q)
{
	return atomic_load_explicit(&q->in, memory_order_relaxed) - q->out;
}

static void _update_tail(mpscq *q){
    block* tail = q->tail;
    if(tail->r > tail->mask)
    {
        //block* b = atomic_load_explicit(&tail->next,memory_order_acquire);
        block* b = tail->next;
        if(b){
            //atomic_exchange_explicit(&q->tail, tail->next, memory_order_acquire);
            q->tail = b;

            // give back to queue for reuse
            if(!spscq_push(q->_idle_queue,tail))  // queue is full
            {
                _del_block(tail);
            }
        }
    }
}

//roundup to power of 2 for bitwise modulus: x % n == x & (n - 1).
static size_t roundup2(size_t v){
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	//v |= v >> 32
	v++;
	return v;
}

static block* _new_block(size_t block_size){
    block* b = calloc(1, sizeof(block));
    if(!b)  return NULL;

    b->next = 0;
    b->w = 0;
	b->r = 0;
    b->buffer = calloc(block_size, sizeof(void *));
    b->mask = block_size - 1;
    
    if(!b->buffer){
        free(b);
        b = NULL;
    }

    memset(b->buffer,0,block_size*sizeof(void *));
    return b;
}

static void _del_block(block* b){
    free(b->buffer);
    free(b);  
}

static block* _get_block(mpscq *q,size_t block_size)
{
    if(spscq_empty(q->_idle_queue)){
        q->_nblock_new++;
        return _new_block(block_size);
    }else{
        block* b = spscq_pop(q->_idle_queue);
        if(!b){
            q->_nblock_new++;
            b = _new_block(block_size);
        }
        memset(b->buffer,0,block_size*sizeof(void *));
        b->r = b->w = 0;
        b->next = 0;
        return b;
    }
}

