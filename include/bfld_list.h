#ifndef __LIST_H__
#define __LIST_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>


#ifndef container_of
#define container_of(ptr, type, member) ({ \
    const typeof(((type*) 0)->member) *__mptr = (ptr); \
    (type*)((char*)__mptr - offsetof(type,member)); \
    })
#endif


/*
 * Linked list implementation.
 */
struct list_head
{
    struct list_head *next;
    struct list_head *prev;
};


/*
 * Get a pointer to the struct containing a list_head (container_of).
 */
#define list_node(head_ptr, type, member) \
    ((type*) ((char*) ((void*) head_ptr) - offsetof(type, member)))


/*
 * Inline list initializer.
 */
#define LIST_HEAD_INIT(name) \
    { &(name), &(name) }


/*
 * Initialize the linked list.
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
void list_remove(struct list_head *entry)
{
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;

    // Make sure that the removed entry do not refer
    // to any entries still in the list
    list_head_init(entry); 
}


/*
 * Insert entry before the specified head.
 * For example, adding elements to a queue.
 */
static inline
void list_insert_before(struct list_head *head, struct list_head *new_entry)
{
    struct list_head *tail = head->prev;
    tail->next = new_entry;
    new_entry->prev = tail;
    head->prev = new_entry;
    new_entry->next = head;
}


/*
 * Insert entry after the specified head.
 * For example, pushing to a stack.
 */
static inline
void list_insert_after(struct list_head *head, struct list_head *new_entry)
{
    struct list_head *first = head->next;
    first->prev = new_entry;
    new_entry->next = first;
    head->next = new_entry;
    new_entry->prev = head;
}


static inline
void list_append(struct list_head *head, struct list_head *new_entry)
{
    list_insert_before(head, new_entry);
}


static inline
void list_prepend(struct list_head *head, struct list_head *new_entry)
{
    list_insert_after(head, new_entry);
}


/*
 * Get the element in front of the specified head.
 */
static inline
struct list_head * list_prev_entry(const struct list_head *head)
{
    return head->prev != head ? head->prev : NULL;
}


/*
 * Get the last entry after the specified head.
 */
static inline
struct list_head * list_next_entry(const struct list_head *head)
{
    return head->next != head ? head->next : NULL;
}


#define list_for_each_entry(iterator, head) \
    for (struct list_head *__it = (head)->next, *__next = __it->next, *iterator = __it; \
            (void*) __it != (void*) (head); \
            __it = __next, __next = (__next)->next, iterator = __it)


#define list_next_node(current_node, type, member) \
    ((current_node)->(member)->next != &(current_node)->next->(member) ? list_node((current_node)->next, type, member) : NULL)


#define list_node_is_head(head, node, member) \
    (list_entry_is_head(head, &(node)->member))


#define list_for_each_node(iterator, head, type, member) \
    for (type* __it = (void*) (head)->next, *__next = (type*) ((struct list_head*) __it)->next, *iterator = list_node((void*) __it, type, member); \
            (void*) __it != (void*) (head); \
            __it = __next, __next = (type*) ((struct list_head*) __next)->next, iterator = list_node((void*) __it, type, member))


#ifdef __cplusplus
}
#endif
#endif
