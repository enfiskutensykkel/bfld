#ifndef __BFLD_UTILS_LIST_H__
#define __BFLD_UTILS_LIST_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>


/*
 * Doubly linked list.
 *
 * Lists are useful for making ordered collections, 
 * for example queues or stacks.
 *
 * Doubly linked lists offer fast insertion and deletion as they have 
 * a list head that points to the first (and last) node in the list.
 *
 * Note that the head node is considered part of the list itself,
 * even though both the list head and the list nodes use the same struct.
 */
struct list_head
{
    struct list_head *next;
    struct list_head *prev;
};


/*
 * Inline initializer for an empty linked list.
 *
 * struct list_head my_list;
 * my_list = (struct list_head) LIST_HEAD_INIT(my_list);
 */
#define LIST_HEAD_INIT(name) { &(name), &(name) }


/*
 * Initialize an empty linked list.
 * An empty list is where the head's pointers point to itself.
 */
static inline
void list_head_init(struct list_head *head)
{
    head->next = head->prev = head;
}


/*
 * Check if the list is empty.
 */
static inline
bool list_empty(const struct list_head *head)
{
    return head->next == head;
}


/*
 * Is the node the list head?
 */
static inline
bool list_is_head(const struct list_head *head, const struct list_head *node)
{
    return node == head;
}


/*
 * Is the specified node the first element in the list?
 */
static inline
bool list_is_first(const struct list_head *head, const struct list_head *node)
{
    return node->prev == head;
}


/*
 * Is the specified node the last element in the list?
 */
static inline
bool list_is_last(const struct list_head *head, const struct list_head *node)
{
    return node->next == head;
}


/*
 * Remove the specified node from its list.
 * The removed node is reinitialized.
 */
static inline
void list_remove(struct list_head *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;

    // We reinitialize the node after removing it
    // to make sure that it does not refer to the old list
    node->prev = node->next = node;
}


/*
 * Insert node before the specified head, making the new
 * element the last element in the list (tail insert).
 *
 * This is useful for implementing a queue, i.e., appending elements
 * to the back of a FIFO queue.
 */
static inline
void list_insert_tail(struct list_head *head, struct list_head *new_node)
{
    struct list_head *tail = head->prev;
    tail->next = new_node;
    new_node->prev = tail;
    head->prev = new_node;
    new_node->next = head;
}


/*
 * Insert node after the specified head, making the new element
 * the first element after head.
 *
 * This is useful for implementing a stack, i.e., pushing elements 
 * to a LIFO stack.
 */
static inline
void list_insert(struct list_head *head, struct list_head *new_node)
{
    struct list_head *first = head->next;
    first->prev = new_node;
    new_node->next = first;
    head->next = new_node;
    new_node->prev = head;
}


/*
 * Move elements from the list pointed to by old_head
 * to the back of the list pointed to by head.
 *
 * The old_head is reinitialized.
 */
static inline
void list_splice_tail(struct list_head *head, struct list_head *old_head)
{
    head->prev->next = old_head->next;
    old_head->next->prev = head;
    head->prev = old_head->prev;
    old_head->prev->next = head;

    old_head->next = old_head->prev = old_head;
}


/*
 * Helper macro to iterate a list forwards.
 */
#define list_for_each(iterator, head_ptr) \
    for (struct list_head *__it = (head_ptr)->next, *__next = __it->next, *iterator = __it; \
            (void*) __it != (void*) (head_ptr); \
            __it = __next, __next = (__next)->next, iterator = __it)


/*
 * Helper macro to iterate a list in reverse.
 */
#define list_for_each_reverse(iterator, head_ptr) \
    for (struct list_head *__it = (head_ptr)->prev, *__next = __it->prev, *iterator = __it; \
            (void*) __it != (void*) (head_ptr); \
            __it = __next, __next = (__next)->prev, iterator = __it)


/*
 * Get next node or NULL if we are at the end of the list.
 */
#define list_next(head_ptr, node_ptr) \
    (((void*) (node_ptr)->next != (void*) (head_ptr)) ? (node_ptr)->next : NULL)


/*
 * Get next node in the list and wrap around if 
 * we are at the end, or NULL if the list is empty.
 */
#define list_next_circular(head_ptr, node_ptr) \
    (((void*) (head_ptr)->next != (void*) (head_ptr)) ? \
     (((void*) (node_ptr)->next != (head_ptr)) ? (node_ptr->next) : (head_ptr)->next) : NULL)


/*
 * Get previous node or NULL if we are at the begining of the list.
 */
#define list_prev(head_ptr, node_ptr) \
    (((void*) (node_ptr)->prev != (void*) (head_ptr)) ? (node_ptr)->prev : NULL)


/*
 * Get the containing struct (the entry) of a list node of a given type.
 */
#define list_entry(node_ptr, type, member) \
    ((type*) ((char*) ((void*) node_ptr) - offsetof(type, member)))


/*
 * Get the first entry of the list, or NULL if the list is empty.
 */
#define list_first_entry(head_ptr, type, member) \
    (((void*) (head_ptr)->next != (void*) (head_ptr)) ? list_entry((head_ptr)->next, type, member) : NULL)


/*
 * Get the last entry of the list, or NULL if the list is empty.
 */
#define list_last_entry(head_ptr, type, member) \
    (((void*) (head_ptr)->prev != (void*) (head_ptr)) ? list_entry((head_ptr)->prev, type, member) : NULL)


/*
 * Get the next entry of the list, or NULL if we are at the end of the list.
 */
#define list_next_entry(head_ptr, entry, type, member) \
    (((void*) (head_ptr)->next != (void*) (head_ptr)) ? list_entry((entry)->next, type, member) : NULL)


/*
 * Get the previous entry of the list, or NULL if we are at the beginning of the list.
 */
#define list_prev_entry(head_ptr, entry, type, member) \
    (((void*) (head_ptr)->prev != (void*) (head_ptr)) ? list_entry((entry)->prev, type, member) : NULL)


/*
 * Helper macro to iterate a list entries of given type.
 */
#define list_for_each_entry(iterator, head_ptr, type, member) \
    for (type* __it = (void*) (head_ptr)->next, *__next = (type*) ((struct list_head*) __it)->next, *iterator = list_entry((void*) __it, type, member); \
            (void*) __it != (void*) (head_ptr); \
            __it = __next, __next = (type*) ((struct list_head*) __next)->next, iterator = list_entry((void*) __it, type, member))


#ifdef __cplusplus
}
#endif
#endif
