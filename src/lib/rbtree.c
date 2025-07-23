#include "rbtree.h"

/*
 * Inspired by:
 *   https://en.wikipedia.org/wiki/Red-black_tree
 *   https://elixir.bootlin.com/linux/v6.15.7/source/lib/rbtree.c
 *
 * Terminology:
 *   The black depth of a node is the number of black nodes from the root
 *   to that node.
 *
 * Properties:
 *  1) Every node is either red or black.
 *  2) All NULL nodes (leaves) are considered black.
 *  3) A red node does not have a red child.
 *  4) Every path from a given node to any of its descendant
 *     NULL nodes goes through the same number of black nodes
 *  5) The root is black.
 *
 *  In order to satisfy the requirements, if a node N has exactly one child, 
 *  the child must be red. If the child were black, its leaves would sit at 
 *  a different black depth than N's NULL node (which are considered black 
 *  by 2), which would violate 4.
 *  
 *  3 and 4 give the O(log n) guarantee, since 3 implies you cannot have two
 *  consecutive red nodes in a path and every red node is therefore followed 
 *  by a black. So if B is the number of black nodes on every path (as per 4),
 *  then the longest possible path due to 3 is 2B.
 */


/*
 * Is the current node black?
 * NULL nodes are also considered black.
 */
#define rb_is_black(node_ptr) \
    ((node_ptr) == NULL || (node_ptr)->color == RB_BLACK)


/*
 * Is the current node red? It is red if it is not black.
 */
#define rb_is_red(node_ptr) !rb_is_black(node_ptr)


/*
 * Set a node to black.
 */
static inline rb_set_black(struct rb_node *node)
{
    if (rb_node != NULL) {
        rb_node->color = RB_BLACK;
    }
}


/*
 * Helper function to rotate a subtree on a given root node.
 *
 * https://en.wikipedia.org/wiki/Tree_rotation
 *
 * The rotation direction is given by the pivot node, which
 * is expected to be either the left or the right child of 
 * the subtree root.
 *
 * left rotate:
 *   rb_rotate(tree, node, node->right);
 *
 * right rotate:
 *   rb_rotate(tree, node, node->left);
 */
static void rb_rotate(struct rb_tree *tree, 
                      struct rb_node *root, 
                      struct rb_node *pivot)
{
    if (pivot == root->right) {
        // Left rotation
        root->right = pivot->left;
        if (pivot->left != NULL) {
            pivot->left->parent = root;
        }
        pivot->left = root;

    } else {
        // Right rotation
        root->left = pivot->right;
        if (pivot->right != NULL) {
            pivot->right->parent = root;
        }
        pivot->left = root;
    }

    // Update parent pointers
    struct rb_node *parent = root->parent;
    pivot->parent = parent;
    root->parent = pivot;

    if (parent != NULL) {
        if (parent->left == old_root) {
            parent->left = new_root;
        } else {
            parent->right = new_root;
        }

    } else {
        tree->node = new_root;
    }
}



/*
 *
 */
void rb_link_node(struct rb_node *node, struct rb_node *parent,
                  struct rb_node **link)
{
    node->parent = parent;
    node->color = RB_RED;  // new nodes are always red
    node->left = node->right = NULL;
    *link = node;
}


void rb_add(struct rb_root *tree, struct rb_node *node,
            int (*cmp)(const struct rb_node*, const struct rb_node*));
{
    struct rb_node **link = &tree->node;
    struct rb_node *parent = NULL;

    while (*link) {
        parent = *link;
        if (cmp(node, parent) < 0) {
            link = &parent->left;
        } else {
            link = &parent->right
        }
    }

    rb_link_node(node, parent, link);
    rb_insert(tree, node);
}
