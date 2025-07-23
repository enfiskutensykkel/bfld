#ifndef __BFLD_UTILS_RED_BLACK_TREE_H__
#define __BFLD_UTILS_RED_BLACK_TREE_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>


enum rb_color { RB_RED = 0, RB_BLACK = 1 };


/*
 * Red-black tree node.
 */
struct rb_node
{
    enum rb_color color;      // Node color
    struct rb_node *parent; // Parent node
    struct rb_node *right;  // Right child
    struct rb_node *left;   // Left child
};


/*
 * Get the containing struct (the entry) of a node of a given type.
 */
#define rb_entry(node_ptr, type, member) \
    ((type*) ((char*) ((void*) node_ptr) - offsetof(type, member)))



/*
 * Red-black tree.
 */
struct rb_tree
{
    struct rb_node *root;
};


/*
 * Inline initializer for an empty tree.
 */
#define RB_TREE (struct rb_tree) { NULL }


/*
 * Initialize an empty tree.
 */
static inline
void rb_tree_init(struct rb_tree *tree)
{
    tree->root = NULL;
}


///*
// * Link a node to a parent.
// */
//void rb_link_node(struct rb_node *node, struct rb_node *parent,
//                  struct rb_node **link);
//
//
///*
// * Insert node at the specific position, after it has been 
// * linked with a parent.
// */
//void rb_insert(struct rb_root *tree, struct rb_node *node);
//
//
///*
// * Add a node to the tree.
// */
//void rb_add(struct rb_root *tree, struct rb_node *node,
//            int (*cmp)(const struct rb_node*, const struct rb_node*));
//


#ifdef __cplusplus
}
#endif
#endif
