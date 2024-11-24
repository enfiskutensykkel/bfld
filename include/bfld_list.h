#ifndef __BFLD_LIST_H__
#define __BFLD_LIST_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>


struct bfld_list
{
    struct bfld_list *next;
    struct bfld_list *prev;
};


#define _bfld_list_entry(type, ptr, field) \
    ((struct bfld_list*) (((size_t) (ptr)) + offsetof(type, field)))


#define _bfld_list_struct(type, entry, field) \
    ((type*) (((size_t) entry) - offsetof(type, field)))


#define bfld_list_init(head) \
    do { \
        (head)->next = (head); \
        (head)->prev = (head); \
    } while (0) 


#define bfld_list_empty(head) \
    ((head)->next == (head))


#define bfld_list_remove_entry(entry) \
    do { \
        (entry)->prev->next = (entry)->next; \
        (entry)->next->prev = (entry)->prev; \
        (entry)->next = (entry); \
        (entry)->prev = (entry); \
    } while (0)


#define bfld_list_push_back(head, entry) \
    do { \
        struct bfld_list* tail = (head)->prev; \
        tail->next = (entry); \
        (entry)->prev = tail; \
        (head)->prev = (entry); \
        (entry)->next = (head); \
    } while (0)


#define bfld_list_push_front(head, entry) \
    do { \
        struct bfld_list* first = (head)->next; \
        first->prev = (entry); \
        (entry)->next = first; \
        (head)->next = (entry); \
        (entry)->prev = (head); \
    } while (0)


#define bfld_list_front(head) \
    ( (head)->next != (head) ? (head)->next : NULL )


#define bfld_list_back(head) \
    ( (head)->prev != (head) ? (head)->prev : NULL )


#define bfld_list_insert(head, node, field) \
    bfld_list_push_back(head, _bfld_list_entry(typeof(*node), node, field))


#define bfld_list_insert_front(head, node, field) \
    bfld_list_push_front(head, _bfld_list_entry(typeof(*node), node, field))


#define bfld_list_remove(node, field) \
    bfld_list_remove_entry(_bfld_list_entry(typeof(*node), node, field))


#define bfld_list_first(type, head, field) \
    (bfld_list_front(head) ? _bfld_list_struct(type, bfld_list_front(head), field) : NULL)


#define bfld_list_last(type, head, field) \
    (bfld_list_back(head) ? _bfld_list_struct(type, bfld_list_back(head), field) : NULL)


#define bfld_list_next(head, node, field) \
    (_bfld_list_entry(typeof(*node), node, field)->next != (head) ? _bfld_list_struct(typeof(*node), _bfld_list_entry(typeof(*node), node, field)->next, field) : NULL)


#define bfld_list_prev(head, node, field) \
    (_bfld_list_entry(typeof(*node), node, field)->next != (head) ? _bfld_list_struct(typeof(*node), _bfld_list_entry(typeof(*node), node, field)->prev, field) : NULL)


#define bfld_list_foreach(type, name, head, field) \
    for (type* __it = (void*) (head)->next, *__next = (type*) ((struct bfld_list*) __it)->next, *name = _bfld_list_struct(type, __it, field); \
            (void*) __it != (void*) (head); \
            __it = __next, __next = (type*) ((struct bfld_list*) __next)->next, name = _bfld_list_struct(type, __it, field))



#ifdef __cplusplus
}
#endif
#endif
