#ifndef __BFLD_UTILS_RED_BLACK_TREE_H__
#define __BFLD_UTILS_RED_BLACK_TREE_H__
#ifdef __cplusplus
extern "C" {
#endif

// FIXME: Do we need an "augmented" rbtree for making interval/segment trees?
// Segment tree stores intervals, and optimized for "which of these intervals contains a given point" queries. Interval tree stores intervals as well, but optimized for "which of these intervals overlap with a given interval" queries. It can also be used for point queries - similar to segment tree

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>


enum rb_color { RB_RED = 0, RB_BLACK = 1 };


/*
 * Red-black tree node.
 */
struct rb_node
{
    enum rb_color color;    // Node color
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


/*
 * Insert a node directly into a given position in the tree,
 * specified by parent and link.
 *
 * After calling this, the tree must be rebalanced (recolored).
 */
static inline
void rb_insert_node(struct rb_node *node,
                    struct rb_node *parent,
                    struct rb_node **link)
{
    node->parent = parent;
    node->color = RB_RED;  // new nodes are always red
    node->left = node->right = NULL;
    *link = node;
}


/*
 * Rebalance (recolor) the tree after inserting a node.
 */
void rb_rebalance(struct rb_tree *tree, struct rb_node *node);


/*
 * Replace an existing node in the tree with a new one
 * with the same key, avoiding rebalancing of the tree.
 */
void rb_replace_node(struct rb_tree *tree,
                     struct rb_node *old_node, 
                     struct rb_node *new_node);


/*
 * Add a node to the tree, using a comparator callback
 * to determine where to insert it, and rebalance the tree.
 *
 * This function is inlined in hopes that the compiler will
 * optimize away the comparison function pointer call.
 */
static inline
void rb_add(struct rb_tree *tree, struct rb_node *node,
            int (*cmp)(const struct rb_node*, const struct rb_node*))
{
    struct rb_node **link = &tree->root;
    struct rb_node *parent = NULL;

    while (*link != NULL) {
        parent = *link;
        if (cmp(node, parent) < 0) {
            link = &parent->left;
        } else {
            link = &parent->right;
        }
    }

    rb_insert_node(node, parent, link);
    rb_rebalance(tree, node);
}


/*
 * Remove the specified node and rebalance the tree.
 */
void rb_remove(struct rb_tree *tree, struct rb_node *node);


#ifdef __cplusplus
}
#endif
#endif
