#include "rbtree.h"
#include <assert.h>

/*
 * Inspired by:
 *   https://en.wikipedia.org/wiki/Red-black_tree
 *   https://elixir.bootlin.com/linux/v6.15.7/source/lib/rbtree.c
 *   https://www.kernel.org/doc/html/v6.15/core-api/rbtree.html
 *   https://cs.brynmawr.edu/Courses/cs246/spring2016/lectures/16_RedBlackTrees.pdf
 *   https://github.com/gfxstrand/rb-tree/
 *
 * Terminology:
 *   The black depth of a node is the number of black nodes from the root
 *   to that node.
 *   
 *   The black height of a tree is the number of black nodes in any path
 *   from the root to the leaves, which is constant. The black height of
 *   a node is the subtree it is the root for.
 *
 * Properties:
 *  P1) Every node is either red or black.
 *  P2) All NULL nodes (leaves) are considered black.
 *  P3) A red node does not have a red child.
 *  P4) Every path from a given node to any of its descendant
 *      NULL nodes goes through the same number of black nodes.
 *  P5) The root is black.
 *
 *  In order to satisfy the requirements, if a node N has exactly one child, 
 *  the child must be red. If the child were black, its leaves would sit at 
 *  a different black depth than N's NULL node (which are considered black 
 *  by P2), which would violate P4 (sometimes called a 'double black').
 *  
 *  P3 and P4 give the O(log n) guarantee, since P3 implies you cannot have 
 *  two consecutive red nodes in a path and every red node is therefore 
 *  followed by a black. So if B is the number of black nodes on every path 
 *  (as per P4), then the longest possible path due to P3 is 2B.
 *
 * Theorems:
 *   T1) Root x has n >= 2^bh(x)-1 nodes, where bh(x) is the black height 
 *       of node x.
 *   T2) At least half of the nodes on any path from the root to a NULL
 *       must be black. bh(x) >= h/2
 *   T3) No path from any node x to a NULL is more than twice as long
 *       as any other path from x to any other NULL.
 *   T4) A tree with n nodes has height h <= 2log(n + 1)
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


static inline void rb_set_black(struct rb_node *node)
{
    if (node != NULL) {
        node->color = RB_BLACK;
    }
}


/*
 * Nodes that are known not to be inserted in a tree
 * are considered "cleared".
 */
static inline bool rb_is_cleared(const struct rb_node *node)
{
    return node->parent == node;
}


static inline void rb_clear_node(struct rb_node *node)
{
    node->parent = node;
    node->left = node->right = NULL;
}


/*
 * Helper function to replace a subtree rooted at old_child
 * with a subtree rooted at new_child.
 *
 * Also sometimes referred to as a "splice".
 */
static inline void rb_transplant(struct rb_tree *tree, 
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

    // Make sure that the new_child's parent pointer points to its new parent
    if (new_child != NULL) {
        new_child->parent = parent;
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
    root->parent = pivot;

    rb_transplant(tree, parent, root, pivot);
}


/*
 * Repair/rebalance/recolor the tree after an insertion.
 *
 * See bottom-up insertion from slides for references to the cases:
 * https://cs.brynmawr.edu/Courses/cs246/spring2016/lectures/16_RedBlackTrees.pdf
 */
void rb_insert_fixup(struct rb_tree *tree, struct rb_node *node)
{
    if (node->parent == NULL) {
        // Case 0: node is root, color it black
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

        if (parent == gparent->left) {  // parent is gparent's left child
            struct rb_node *uncle = gparent->right;

            if (rb_is_red(uncle)) {
                // Case 1: both parent and uncle are red
                parent->color = RB_BLACK;
                uncle->color = RB_BLACK;
                gparent->color = RB_RED;
                node = gparent;
                continue;

            } else {
                // uncle is black

                if (node == parent->right) {
                    // Case 2: zig-zag (double rotate)
                    // node and parent are opposite-hand children

                    rb_rotate(tree, parent, node);  // rotate left
                    node = parent;
                    parent = node->parent;
                    assert(node == parent->left || node == parent->right);
                    gparent = parent->parent;
                    assert(parent == gparent->left || parent == gparent->right);

                    // Case 2 continuation by fall-through to Case 3
                }

                // Case 3: zig-zig (single rotate)
                // node and parent are both left-hand children
                parent->color = RB_BLACK;
                gparent->color = RB_RED;
                rb_rotate(tree, gparent, gparent->left);  // rotate right on grand parent
            }

        } else {  // parent is gparent's right child
            struct rb_node *uncle = gparent->left;

            if (rb_is_red(uncle)) {
                // Case 1: both parent and uncle are red
                parent->color = RB_BLACK;
                uncle->color = RB_BLACK;
                gparent->color = RB_RED;
                node = gparent;
                continue;
            
            } else {
                // uncle is black

                if (node == parent->left) {
                    // Case 2: zig-zag (double rotation)
                    // node and parent are opposite-hand children
                    rb_rotate(tree, parent, node);  // rotate right on parent
                    
                    // Move up to next parent
                    node = parent;
                    parent = node->parent;
                    assert(node == parent->left || node == parent->right);
                    gparent = parent->parent;
                    assert(parent == gparent->left || parent == gparent->right);

                    // Case 2 continuation by fall-through to Case 3
                }

                // Case 3: zig-zig (single rotation)
                // node and parent both right-hand children
                parent->color = RB_BLACK;
                gparent->color = RB_RED;
                rb_rotate(tree, gparent, gparent->right);  // rotate left on grand parent
            }
        }
    }

    // Ensure that the root is black
    tree->root->color = RB_BLACK;
}


void rb_replace_node(struct rb_tree *tree, 
                     struct rb_node *old_node, 
                     struct rb_node *new_node)
{
    struct rb_node *parent = old_node->parent;

    // Copy children, parent and color from the old node to the new
    *new_node = *old_node;

    // Set the surrounding nodes to point to the new node
    if (old_node->left != NULL) {
        old_node->left->parent = new_node;
    }

    if (old_node->right != NULL) {
        old_node->right->parent = new_node;
    }

    rb_transplant(tree, parent, old_node, new_node);
}


/*
 * "Normal" binary search tree deletion. 
 * If the tree needs to be rebalanced afterwards, the pointer to 
 * the node where rebalancing should start is returned (i.e., the parent).
 * If fixup is not needed, then NULL is returned.
 */
static struct rb_node * rb_remove_node(struct rb_tree *tree, struct rb_node *node)
{
    if (node->left == NULL) {
        // Node has only right-hand child (or no children)
        rb_transplant(tree, node->parent, node, node->right);

        if (node->right != NULL) {
            // right child inherits node's color
            node->right->parent = node->parent;
            node->right->color = node->color;
        } else if (node->color == RB_BLACK) {
            // removed node was black (and no child to absorb its color)
            // need to fixup starting from the removed node's parent
            return node->parent;
        }

        return NULL;

    } else if (node->right == NULL) {
        // Node has only left-hand child
        // child inherits the node's color
        rb_transplant(tree, node->parent, node, node->left);
        node->left->parent = node->parent;
        node->left->color = node->color;
        return NULL;
    }

    // Node has both children, we need to find the successor
    // start by finding the left-most child of node's right-hand child,
    // this is the minimum value that's smaller than the node that is removed
    struct rb_node *fixup = NULL;
    struct rb_node *successor = node->right;
    struct rb_node *rchild = NULL;  // successor's right child
    while (successor->left != NULL) {
        successor = successor->left;
    }

    if (successor->parent == node) {
        // Successor is node's direct child
        fixup = successor;
        rchild = successor->right;
    } else {
        // Remove successor from original spot and link its right child to its old parent
        fixup = successor->parent;
        rchild = successor->right;
        rb_transplant(tree, successor->parent, successor, successor->right);
        successor->right = node->right;  // node->right must be set due to the checks above
        successor->right->parent = successor;
    }
    assert(successor->left == NULL);

    // Replace node with successor in the tree
    rb_transplant(tree, node->parent, node, successor);
    successor->left = node->left;
    successor->left->parent = successor;

    // Make sure that fixup starts from the appropriate location
    if (rchild != NULL) {
        // Child is recolored black and fixup is skipped
        // This assumes that child is properly inserted (which it should be)
        fixup = NULL;
        rchild->color = RB_BLACK;
    } else if (!rb_is_black(successor)) {
        // Successor wasn't black, so no fixup is required
        fixup = NULL;
    }

    // Preserve the original color of the deleted node
    successor->color = node->color;
    return fixup;
}


/*
 * Repair/recolor/rebalance a tree after deletion.
 * Start fixing up from the specified parent node.
 */
static void rb_remove_fixup(struct rb_tree *tree, struct rb_node *parent)
{
    struct rb_node *node = NULL;  // Node has been removed. NULL nodes are black.
    
    while (true) {

        if (node == parent->left) {  // node is left-hand child
            struct rb_node *sibling = parent->right;

            if (rb_is_red(sibling)) {
                // Case 1: Sibling is red
                sibling->color = RB_BLACK;
                parent->color = RB_RED;
                rb_rotate(tree, parent, sibling);  // rotate left
                sibling = parent->right;  // update sibling after rotate
            }

            if (rb_is_black(sibling->right)) {
                if (rb_is_black(sibling->left)) {
                    // Case 2: Sibling is black, both its children are black
                    sibling->color = RB_RED;
                    if (parent->color == RB_RED) {
                        parent->color = RB_BLACK;
                    } else {
                        node = parent;
                        parent = parent->parent;
                        if (parent != NULL) {
                            continue;  // we recurse up the tree
                        }
                    }
                    break;
                } 

                // Case 3: Sibling is black, right child is black, left is red
                rb_set_black(sibling->left);
                sibling->color = RB_RED;
                rb_rotate(tree, sibling, sibling->left);  // right rotate
                sibling = parent->right;  // update sibling after rotate
                
                // Fall-through to Case 4
            }

            // Case 4: Sibling is black, right is red, left any color
            sibling->color = parent->color;
            parent->color = RB_BLACK;
            rb_set_black(sibling->right);
            rb_rotate(tree, parent, sibling);  // left rotate
            node = tree->root;
            break;

        } else {  // node is right-hand child
            struct rb_node *sibling = parent->left;

            if (rb_is_red(sibling)) {
                // Case 1: Sibling is red
                sibling->color = RB_BLACK;
                parent->color = RB_RED;
                rb_rotate(tree, parent, sibling);  // rotate right
                sibling = parent->left;
            }
            
            if (rb_is_black(sibling->left)) {
                if (rb_is_black(sibling->right)) {
                    // Case 2: Sibling is black, both its children are black
                    sibling->color = RB_RED;
                    if (parent->color == RB_RED) {
                        parent->color = RB_BLACK;
                    } else {
                        node = parent;
                        parent = parent->parent;
                        if (parent != NULL) {
                            continue;  // we recurse up the tree
                        }
                    }
                    break;
                }

                // Case 3: Sibling is black, its left is black and right is red
                rb_set_black(sibling->right);
                sibling->color = RB_RED;
                rb_rotate(tree, sibling, sibling->right);  // left rotate
                sibling = parent->left;  // update sibling after rotation
                
                // Fall-through to Case 4
            }

            // Case 4: Sibling is black, left is red, right is any color
            sibling->color = parent->color;
            parent->color = RB_BLACK;
            rb_set_black(sibling->left);
            rb_rotate(tree, parent, sibling);  // right rotate
            node = tree->root;
            break;
        }
    }

    rb_set_black(tree->root);
}


void rb_remove(struct rb_tree *tree, struct rb_node *node)
{
    if (rb_is_cleared(node)) {
        return;
    }

    struct rb_node *fixup = rb_remove_node(tree, node);
    if (fixup != NULL) {
        rb_remove_fixup(tree, fixup);
    }

    rb_clear_node(node);
}


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


struct rb_node * rb_next(const struct rb_node *node)
{
    if (rb_is_cleared(node)) {
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

    /* No right-hand children, which means that everything
     * down to the left is smaller than the current node.
     * The next node must be in the general direction of the parent.
     * Go up the tree; any time the ancestor is a right-hand child of
     * its parent, keep going up. The parent of the first ancestor 
     * that is a left-hand is the next node.
     */
    struct rb_node *parent = node->parent;
    while (parent != NULL && node == parent->right) {
        node = parent;
        parent = parent->parent;
    }

    return parent;
}


struct rb_node * rb_prev(const struct rb_node *node)
{
    if (rb_is_cleared(node)) {
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
