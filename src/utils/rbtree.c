#include "rbtree.h"
#include <assert.h>

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
static inline void rb_set_black(struct rb_node *node)
{
    if (node != NULL) {
        node->color = RB_BLACK;
    }
}


/*
 * Helper function to change a child.
 * This is also known as a transplant.
 */
static inline void rb_change_child(struct rb_tree *tree, 
                                   struct rb_node *parent,
                                   struct rb_node *old_child,
                                   struct rb_node *new_child)
{
    if (parent != NULL) {
        if (parent->left == old_child) {
            parent->left = new_child;
        } else {
            assert(parent->right == old_child);
            parent->right = new_child;
        }

    } else {
        assert(tree->root == old_child);
        tree->root = new_child;
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
    assert(root != NULL && pivot != NULL);
    assert(pivot == root->left || pivot == root->right);

    if (pivot == root->right) {
        // Left rotation
        root->right = pivot->left;
        if (pivot->left != NULL) {
            assert(pivot->left->parent == pivot);
            pivot->left->parent = root;
        }
        pivot->left = root;

    } else {
        // Right rotation
        assert(pivot == root->left);
        root->left = pivot->right;
        if (pivot->right != NULL) {
            assert(pivot->right->parent == pivot);
            pivot->right->parent = root;
        }
        pivot->right = root;
    }

    // Update parent pointers
    struct rb_node *parent = root->parent;
    pivot->parent = parent;
    root->parent = pivot;

    rb_change_child(tree, parent, root, pivot);
}


/*
 * Fix up after the insertion.
 */
void rb_insert_fixup(struct rb_tree *tree, struct rb_node *node)
{
    if (node->parent == NULL) {
        tree->root = node;
        node->color = RB_BLACK;
        return;
    }
    
    while (rb_is_red(node->parent)) {
        struct rb_node *parent = node->parent;
        assert(node == parent->left || node == parent->right);

        struct rb_node *gparent = parent->parent;

        assert(gparent != NULL);
        assert(parent == gparent->left || parent == gparent->right);

        if (parent == gparent->left) {
            struct rb_node *uncle = gparent->right;

            if (rb_is_red(uncle)) {
                parent->color = RB_BLACK;
                uncle->color = RB_BLACK;
                gparent->color = RB_RED;
                node = gparent;

            } else {

                if (node == parent->right) {
                    rb_rotate(tree, parent, node);  // rotate left
                    node = parent;
                    parent = node->parent;
                    assert(node == parent->left || node == parent->right);
                    gparent = parent->parent;
                    assert(parent == gparent->left || parent == gparent->right);
                }

                parent->color = RB_BLACK;
                gparent->color = RB_RED;

                rb_rotate(tree, gparent, gparent->left);  // rotate right
            }

        } else {  // parent == gparent->right
            struct rb_node *uncle = gparent->left;

            if (rb_is_red(uncle)) {
                parent->color = RB_BLACK;
                uncle->color = RB_BLACK;
                gparent->color = RB_RED;
                node = gparent;
            
            } else {

                if (node == parent->left) {
                    rb_rotate(tree, parent, node);  // rotate right
                    node = parent;
                    parent = node->parent;
                    assert(node == parent->left || node == parent->right);
                    gparent = parent->parent;
                    assert(parent == gparent->left || parent == gparent->right);
                }

                parent->color = RB_BLACK;
                gparent->color = RB_RED;
                rb_rotate(tree, gparent, gparent->right);  // rotate left
            }

        }
    }

    tree->root->color = RB_BLACK;
}


void rb_replace_node(struct rb_tree *tree, 
                     struct rb_node *old_node, 
                     struct rb_node *new_node)
{
    struct rb_node *parent = old_node->parent;

    // Copy children and color from the old node to the new
    *new_node = *old_node;

    // Set the surrounding nodes to point to the new node
    if (old_node->left != NULL) {
        old_node->left->parent = new_node;
    }

    if (old_node->right != NULL) {
        old_node->right->parent = new_node;
    }

    rb_change_child(tree, parent, old_node, new_node);
}
