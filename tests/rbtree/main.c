#include <rbtree.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>


struct word
{
    struct rb_node rb_node;
    char data[];
};


static void validate_nodes(const struct rb_node *node, int black_depth)
{
    if (node == NULL) {
        assert(black_depth == 0);
        return;
    }

    if (node->color == RB_BLACK) {
        black_depth--;
    } else {
        assert(node->left == NULL || node->left->color == RB_BLACK);
        assert(node->right == NULL || node->right->color == RB_BLACK);
    }

    validate_nodes(node->left, black_depth);
    validate_nodes(node->right, black_depth);
}


static void validate_tree(const struct rb_tree *tree)
{
    if (tree->root == NULL) {
        return;
    }

    assert(tree->root->color == RB_BLACK);

    int black_depth = 0;

    for (struct rb_node *n = tree->root; n != NULL; n = n->left) {
        if (n->color == RB_BLACK) {
            black_depth++;
        }
    }

    validate_nodes(tree->root, black_depth);
}


static size_t count_nodes(const struct rb_node *node)
{
    if (node == NULL) {
        return 0;
    }

    return 1 + count_nodes(node->left) + count_nodes(node->right);
}


static struct word * traverse_find(const struct rb_node *node, const char *data)
{
    if (node != NULL) {
        struct word * w = rb_entry(node, struct word, rb_node);
        int c = strcmp(data, w->data);
        if (c == 0) {
            return w;
        } else if (c < 0) {
            return traverse_find(node->left, data);
        } else {
            return traverse_find(node->right, data);
        }
    }

    return NULL;
}


static struct word * alloc_word(const char *str)
{
    size_t n = strlen(str);

    struct word *w = malloc(sizeof(struct word) + n + 1);
    assert(w != NULL);
    strcpy(w->data, str);
    return w;
}


static int wordcmp(const struct rb_node *a, const struct rb_node *b) 
{
    assert(a != NULL);
    assert(b != NULL);
    assert(a != b);
    struct word *A = rb_entry(a, struct word, rb_node);
    struct word *B = rb_entry(b, struct word, rb_node);

    return strcmp(A->data, B->data);
}

static int searchcmp(const void *key, const struct rb_node *item)
{
    const char *s = (const char*) key;
    assert(key != NULL && item != NULL);
    return strcmp(s, rb_entry(item, struct word, rb_node)->data);
}


static void print_sorted(const struct rb_node *node)
{
    if (node != NULL) {
        print_sorted(node->left);
        struct word *w = rb_entry(node, struct word, rb_node);
        puts(w->data);
        print_sorted(node->right);
    }
}


int main()
{
    const char *fruits[] = {"mango", "pear", "cherry", "plum", "banana", "orange", "apple", "coconut"};
    // TODO: add duplicates

    struct rb_tree tree = RB_TREE;

    for (size_t i = 0; i < sizeof(fruits) / sizeof(*fruits); ++i) {
        struct word *word = alloc_word(fruits[i]);

        size_t count_before = count_nodes(tree.root);
        assert(count_before == i);

        printf("Adding word %s\n", word->data);
        fflush(stdout);
        rb_add(&tree, &word->rb_node, wordcmp);

        printf("Validating tree\n");
        fflush(stdout);
        validate_tree(&tree);

        size_t count_after = count_nodes(tree.root);
        printf("%zu == %zu + 1\n", count_after, count_before);
        fflush(stdout);
        assert(count_after == count_before + 1);

        print_sorted(tree.root);
    }

    for (size_t i = 0; i < sizeof(fruits) / sizeof(*fruits); ++i) {
        printf("Searching manually for %s\n", fruits[i]);
        fflush(stdout);
        struct word *w = traverse_find(tree.root, fruits[i]);
        assert(w != NULL);
    }

    printf("Searching for banana\n");
    fflush(stdout);
    struct rb_node *node = rb_find(&tree, "banana", searchcmp);
    assert(node != NULL);
    struct word *w = rb_entry(node, struct word, rb_node);

    node = rb_find(&tree, "NOT IN LIST", searchcmp);
    assert(node == NULL);

    print_sorted(tree.root);

    return 0;
}
