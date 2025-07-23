#ifndef __BFLD_UTILS_RBTREE_H__
#define __BFLD_UTILS_RBTREE_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>


enum rb_color
{
    RB_RED, RB_BLACK
};


struct rb_node
{
    enum rb_color color;
    struct rb_node *parent;
    struct rb_node *right;
    struct rb_node *left;
};


struct rb_root
{
    struct rb_node *rb_node;
};


/*
 * Callback function for comparing two tree nodes.
 */
typedef int (*rb_node_cmp)(const struct rb_node*, 
                           const struct rb_node*);



/*
 * Callback function for comparing a tree node with a key.
 */
typedef int (*rb_key_cmp)(const void *,
                          const struct rb_node*);



/*
 * Get the containing struct (the entry) of a node of a given type.
 */
#define rb_entry(node_ptr, type, member) \
    ((type*) ((char*) ((void*) node_ptr) - offsetof(type, member)))



static inline
void rb_insert(struct rb_root *tree, struct rb_node *node,
               bool (*less_than)(struct rb_node *


#ifdef __cplusplus
}
#endif
#endif
