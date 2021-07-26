/******************************************************************************
implement a lock free mpsc queue, based on  array + list 

why?
mpsc 的实现方式主要有2类
1: unbound node based, 即基于list实现
2: bound array based, 即基于array实现

以上2种方案各有优劣,
方案1: 优点是实现简单, 不限队列长度, 但push时需new node, new操作是内部是上锁的,故性能瓶颈在new
方案2: 预先分配一个固定容量Array, 不需要频繁new, 故可提高性能, 但队列长度固定, 超出容量后需等待

本实现方式: 基于array + list, 兼顾性能和内存使用
            tail ->                     head ->
             |                            | 
Array1----->Array2------->....---------->ArrayN 

既减少了内存分配次数, 又可以动态扩容

*******************************************************************************/
#ifndef __MPSCQ_H__
#define __MPSCQ_H__

#include <stdint.h>
#ifndef __cplusplus
#include <stdatomic.h>
#endif
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif

struct spscq_t;

typedef struct block_t {
    struct block_t* next;
	size_t w;
	size_t r;
	size_t mask;
    void* *buffer;
}block;

typedef struct mpscq_t {
 	block* head;
	block* tail;
    size_t block_size;
    size_t in;
    size_t out;
    struct spscq_t* _idle_queue;
    size_t _nblock_new;
}mpscq;



mpscq* mpscq_create(size_t block_size);

/* push an item into the queue. Returns true on success
 * and false on failure (queue full). This is safe to call
 * from multiple threads */
bool mpscq_push(mpscq* q, void *obj);

/* dequeue an item from the queue and return it.
 * THIS IS NOT SAFE TO CALL FROM MULTIPLE THREADS.
 * Returns NULL on failure, and the item it dequeued
 * on success */
void* mpscq_pop(mpscq* q);

/* get the number of items in the queue currently */
size_t mpscq_count(mpscq* q);

bool mpscq_empty(mpscq* q);

/* destroy a mpscq. Frees the internal buffer, and
 * frees q if it was created by passing NULL to mpscq_create */
void mpscq_destroy(mpscq* q);

#ifdef __cplusplus
}
#endif

#endif

