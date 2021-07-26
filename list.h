#ifndef __BSD_LIST_H
#define __BSD_LIST_H


#ifdef __cplusplus
extern "C" {
#endif


struct list_head{
     struct list_head* next, *prev;   
};

static inline void INIT_LIST_HEAD(struct list_head *list) {
    list->next = list;
    list->prev = list;
}

//prev----node ----- next
static inline void __list_add(struct list_head *node, struct list_head *prev,
                  struct list_head *next)
{
    next->prev = node;
    node->next = next;
    node->prev = prev;
    prev->next = node;
}


static inline void list_add(struct list_head *node, struct list_head *head)
{
    __list_add(node, head, head->next);
}


static inline void list_add_tail(struct list_head *node, struct list_head *head)
{
    __list_add(node, head->prev, head);
}

static inline void list_replace(struct list_head *old, struct list_head *node) {
    node->next = old->next;
    node->next->prev = node;
    node->prev = old->prev;
    node->prev->next = node;
}

static inline void list_replace_init(struct list_head *old, struct list_head *node) {
    list_replace(old, node);
    INIT_LIST_HEAD(old);
}

static inline int list_empty(const struct list_head *head) {
    return head->next == head;
}


#define list_entry(ptr, type, member) \
((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))


#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

#define container_of(ptr, type, member) ({                              \
        void *__mptr = (void *)(ptr);                                   \
        ((type *)(__mptr - ((size_t)&((type *)0)->member))); })

/**
 * list_for_each_safe - iterate over elements in a list, but don't dereference
 *                      pos after the body is done (in case it is freed)
 * @pos:	the &struct list_head to use as a loop counter.
 * @pnext:	the &struct list_head to use as a pointer to the next item.
 * @head:	the head for your list (not included in iteration).
 */
#define list_for_each_safe(pos, pnext, head) \
	for (pos = (head)->next, pnext = pos->next; pos != (head); \
	     pos = pnext, pnext = pos->next)


#define list_for_each_entry_safe(pos, n, head, member)\
for (pos = list_entry((head)->next, typeof(*pos), member),\
n = list_entry(pos->member.next, typeof(*pos), member);\
&pos->member != (head);\
pos = n, n = list_entry(n->member.next, typeof(*n), member))


static inline void list_del(struct list_head *node) {
    struct list_head *prev = node->prev;
    struct list_head *next = node->next;
    next->prev = prev;
    prev->next = next;
}


#ifdef __cplusplus
}
#endif

#endif