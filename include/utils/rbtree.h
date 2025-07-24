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
 * Nodes that are known not to be inserted in a tree
 * are considered "cleared".
 */
static inline 
bool rb_is_clear(const struct rb_node *node)
{
    return node->parent == node;
}


static inline
void rb_clear_node(struct rb_node *node)
{
    node->parent = node;
}


#define RB_CLEAR_NODE(name) (struct rb_node) {RB_RED, &(name), NULL, NULL}


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
 * specified by parent and the parent's child link.
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
void rb_insert_fixup(struct rb_tree *tree, struct rb_node *node);


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
 * Instead of doing this, you can make your own insertion function:
 *
 * int my_insert(struct rb_tree *tree, struct mytype *data)
 * {
 *   struct rb_node **new = *(root->rb_node), *parent = NULL;
 *
 *   while (*new != NULL) {
 *     struct mytype *this = rb_entry(*new, struct mytype, node);  // assumes that struct mytype has a rb_node member called "node"
 *     int result = strcmp(data->keystring, this->keystring);
 *
 *     parent = *new;
 *     if (result < 0) {
 *       new = &((*new)->left);
 *     } else if (result > 0) {
 *       new = &((*new)->right);
 *     } else {
 *       // something with the same key already exists in the tree
 *       return -1;
 *     }
 *   }
 *   
 *   // Add new node and rebalance the tree
 *   rb_insert_node(node, parent, new);
 *   rb_insert_fixup(tree, node);
 *   return 0;
 * }
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
    rb_insert_fixup(tree, node);
}


/*
 * Remove the specified node and rebalance the tree.
 */
void rb_remove(struct rb_tree *tree, struct rb_node *node);


/*
 * Do a binary search for a specific node, using a comparator callback.
 *
 * Instead of doing this, you can make your own custom search function:
 *
 * struct mytype * my_search(struct rb_tree *tree, const char *string)
 * {
 *   struct rb_node *node = tree->root;
 *
 *   while (node != NULL) {
 *     struct mytype *data = rb_entry(node, struct mytype, node);  // assumes struct mytype has a rb_node member called "node"
 *     
 *     int result = strcmp(string, data->keystring);
 *
 *     if (result < 0) {
 *       node = node->left;
 *     } else if (result > 0) {
 *       node = node->right;
 *     } else {
 *       return data;
 *     }
 *   }
 *
 *   return NULL;
 * }
 */
static inline
struct rb_node * rb_find(const struct rb_tree *tree, const void *key,
                         int (*cmp)(const void *key, const struct rb_node*))
{
    struct rb_node *node = tree->root;

    while (node != NULL) {
        int c = cmp(key, node);

        if (c < 0) {
            node = node->left;
        } else if (c > 0) {
            node = node->right;
        } else {
            return node;
        }
    }

    return NULL;
}


/*
 * Get the first node (in sort order) of the tree.
 * Also known as the "minimum".
 */
static inline
struct rb_node * rb_first(const struct rb_tree *tree)
{
    struct rb_node *node = tree->root;

    if (node == NULL) {
        return NULL;
    }

    while (node->left != NULL) {
        node = node->left;
    }

    return node;
}


/*
 * Get the last node (in sort order) of the tree.
 * Also known as the "maximum".
 */
static inline
struct rb_node * rb_last(const struct rb_tree *tree)
{
    struct rb_node *node = tree->root;

    if (node == NULL) {
        return NULL;
    }

    while (node->right != NULL) {
        node = node->right;
    }

    return node;
}


/*
 * Get the next node (in sort order) of the tree.
 */
static inline
struct rb_node * rb_next(const struct rb_node *node)
{
    if (rb_is_clear(node)) {
        return NULL;
    }

    // If we have a right-hand child, go down and one step 
    // and then as far left as we can go.
    if (node->right != NULL) {
        node = node->right;
        while (node->left != NULL) {
            node = node->left;
        }

        return (struct rb_node*) node;
    }

    // No right-hand children, which means that everything
    // down to the left is smaller than the current node.
    // The next node must be in the general direction of the parent.
    // Go up the tree; any time the ancestor is a right-hand child of
    // its parent, keep going up. The parent of the first ancestor 
    // that is a left-hand is the next node.
    struct rb_node *parent = node->parent;
    while (parent != NULL && node == parent->right) {
        node = parent;
        parent = parent->parent;
    }

    return parent;
}


/*
 * Get the previous node of the tree.
 */
static inline
struct rb_node * rb_prev(const struct rb_node *node)
{
    if (rb_is_clear(node)) {
        return NULL;
    }

    // If we have a left-hand child, go down one level and then
    // as far right as we are able to.
    if (node->left != NULL) {
        node = node->left;
        while (node->right != NULL) {
            node = node->right;
        }
        return (struct rb_node*) node;
    }

    // No left-hand children, which means that we need to go
    // up until we find an ancestor that is a right-hand child of
    // its parent.
    struct rb_node *parent = node->parent;
    while (parent != NULL && node == parent->left) {
        node = parent;
        parent = parent->parent;
    }

    return parent;
}


/*
 * Get the first node in post-order (visiting both children first
 * before the parent node).
 */
struct rb_node * rb_first_postorder(const struct rb_tree *tree);


/*
 * Get the next node in post-order (visiting both children first
 * before the parent node).
 */
struct rb_node *rb_next_postorder(const struct rb_tree *tree);


#ifdef __cplusplus
}
#endif
#endif
