#ifndef __BFLD_LIST_H__
#define __BFLD_LIST_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>


/*
 * Linked list implementation.
 */
struct list_head
{
    struct list_head *next;
    struct list_head *prev;
};


/*
 * Inline initializer for an empty linked list.
 */
#define LIST_HEAD_INIT(name) \
    { &(name), &(name) }


/*
 * Initialize an empty linked list.
 */
static inline
void list_head_init(struct list_head *head)
{
    head->next = head;
    head->prev = head;
}


/*
 * Check if the list is empty.
 */
static inline
bool list_empty(const struct list_head *head)
{
    return head->next == head;
}


static inline
bool list_entry_is_head(const struct list_head *head, const struct list_head *entry)
{
    return entry == head;
}


/*
 * Remove entry from list.
 */
static inline
void list_remove_entry(struct list_head *entry)
{
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;

    // Make sure that the removed entry do not refer
    // to any entries still in the list
    list_head_init(entry); 
}


/*
 * Insert entry before the specified head,
 * i.e., at the back/tail of the list.
 *
 * Example: Adding elements to a queue.
 */
static inline
void list_tail_insert_entry(struct list_head *head, struct list_head *new_entry)
{
    struct list_head *tail = head->prev;
    tail->next = new_entry;
    new_entry->prev = tail;
    head->prev = new_entry;
    new_entry->next = head;
}


/*
 * Insert entry after the specified head,
 * i.e., at the front/head of the list.
 *
 * Example: Pushing elements to a stack.
 */
static inline
void list_head_insert_entry(struct list_head *head, struct list_head *new_entry)
{
    struct list_head *first = head->next;
    first->prev = new_entry;
    new_entry->next = first;
    head->next = new_entry;
    new_entry->prev = head;
}


/*
 * Insert entry at the back/tail of the list.
 */
static inline
void list_append_entry(struct list_head *head, struct list_head *new_entry)
{
    list_tail_insert_entry(head, new_entry);
}


/*
 * Insert entry at the front/head of the list.
 */
static inline
void list_prepend_entry(struct list_head *head, struct list_head *new_entry)
{
    list_head_insert_entry(head, new_entry);
}


/*
 * Iterate the linked list, entry by entry.
 */
#define list_for_each_entry(iterator, head) \
    for (struct list_head *__it = (head)->next, *__next = __it->next, *iterator = __it; \
            (void*) __it != (void*) (head); \
            __it = __next, __next = (__next)->next, iterator = __it)


/*
 * Get the surrounding struct.
 */
#ifndef container_of
#define container_of(ptr, type, member) ({ \
    const typeof(((type*) 0)->member) *__mptr = (ptr); \
    (type*)((char*)__mptr - offsetof(type,member)); \
    })
#endif


/*
 * Get a pointer to the struct containing a list_head (container_of), or the "node".
 */
#define list_node(head_ptr, type, member) \
    ((type*) ((char*) ((void*) head_ptr) - offsetof(type, member)))


/*
 * Get the next node in the list (assuming the list nodes are of the same type).
 */
#define list_next_node(current_node, type, member) \
    list_node((current_node)->(member).next, type, member) 


/*
 * Iterate the linked list, node by node.
 */
#define list_for_each_node(iterator, head, type, member) \
    for (type* __it = (void*) (head)->next, *__next = (type*) ((struct list_head*) __it)->next, *iterator = list_node((void*) __it, type, member); \
            (void*) __it != (void*) (head); \
            __it = __next, __next = (type*) ((struct list_head*) __next)->next, iterator = list_node((void*) __it, type, member))


#ifdef __cplusplus
}
#endif
#endif
