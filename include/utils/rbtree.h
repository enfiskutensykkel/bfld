#ifndef _BFLD_UTILS_RED_BLACK_TREE_H
#define _BFLD_UTILS_RED_BLACK_TREE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>


struct rb_node;


/*
 * Red-black tree.
 *
 * Red-black trees are a type of self-balancing binary search tree.
 * They offer relatively fast look up of nodes. However, note that
 * iterating, inserting into and deleting from a tree are slow operations,
 * so this is not the appropriate data structure for those.
 */
struct rb_tree
{
    struct rb_node *root;
};


/*
 * Inline initializer for an empty tree.
 */
#define RB_TREE (struct rb_tree) { NULL }


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
 * Inline initializer for red-black tree node.
 */
#define RB_NODE(name) (struct rb_node) {RB_RED, &(name), NULL, NULL}


/*
 * Check if the tree is empty.
 */
static inline
bool rb_tree_empty(const struct rb_tree *tree)
{
    return tree->root == NULL;
}


/*
 * Initialize an empty node that's not part of a tree.
 */
static inline 
void rb_node_init(struct rb_node *node)
{
    node->parent = node;
    node->left = node->right = NULL;
}



/*
 * Initialize an empty tree.
 */
static inline
void rb_tree_init(struct rb_tree *tree)
{
    tree->root = NULL;
}


/*
 * Is the node inserted into a tree?
 */
static inline
bool rb_node_is_inserted(const struct rb_node *node)
{
    return node->parent != node;
}


/*
 * Insert a node directly into a given position in the tree,
 * specified by parent and the parent's child link.
 *
 * Note that this only links the new node in its proper position.
 * The tree must be rebalanced after insertions.
 *
 * See also: rb_insert_fixup()
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
 * Rebalance the tree after insertion(s).
 *
 * This is a separate step, also known as "lazy fixup",
 * allowing bulk insertions before rebalancing the tree.
 *
 * See also: rb_insert_node()
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
 * This function is inlined in hope that the compiler will optimize
 * away calls to the comparator callback. Instead of doing using this function,
 * you should ideally make your own insertion function:
 *
 * int my_insert(struct rb_tree *tree, struct mytype *data)
 * {
 *   struct rb_node **new = &(tree->root), *parent = NULL;
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
 *   rb_insert_node(&data->node, parent, new);
 *   rb_insert_fixup(tree, &data->node);
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
 * Remove the specified node and rebalance the tree afterwards.
 */
void rb_remove(struct rb_tree *tree, struct rb_node *node);


/*
 * Do a binary search for a specific node, using a comparator callback.
 *
 * This function is inlined in hope that the compiler will optimize
 * away calls to the comparator callback. Instead of using this function, 
 * you ideally should make your own custom search function:
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
struct rb_node * rb_first(const struct rb_tree *tree);


/*
 * Get the last node (in sort order) of the tree.
 * Also known as the "maximum".
 */
struct rb_node * rb_last(const struct rb_tree *tree);


/*
 * Get the next node (in sort order) of the tree.
 */
struct rb_node * rb_next(const struct rb_node *node);


/*
 * Get the previous node of the tree.
 */
struct rb_node * rb_prev(const struct rb_node *node);


/*
 * Get the first node in post-order (visiting both children first
 * before the parent node).
 */
struct rb_node * rb_first_postorder(const struct rb_tree *tree);


/*
 * Get the next node in post-order (visiting both children first
 * before the parent node).
 */
struct rb_node *rb_next_postorder(const struct rb_node *node);


#ifdef __cplusplus
}
#endif
#endif
