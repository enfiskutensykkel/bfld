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

    assert(node->left == NULL || node->left->parent == node);
    validate_nodes(node->left, black_depth);
    assert(node->right == NULL || node->right->parent == node);
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

    assert(node->left == NULL || node->left->parent == node);
    assert(node->right == NULL || node->right->parent == node);
    return 1 + count_nodes(node->left) + count_nodes(node->right);
}


/*
 * Look for an item with binary search.
 */
static struct word * search(const struct rb_node *node, const char *data)
{
    if (node != NULL) {
        struct word * w = rb_entry(node, struct word, rb_node);
        int c = strcmp(data, w->data);
        if (c == 0) {
            return w;
        } else if (c < 0) {
            return search(node->left, data);
        } else {
            return search(node->right, data);
        }
    }

    return NULL;
}


/*
 * Look for an item throughout the entire tree.
 */
static struct word * traverse_find(const struct rb_node *node, const char *data)
{
    if (node != NULL) {
        struct word *w = rb_entry(node, struct word, rb_node);
        if (strcmp(data, w->data) == 0) {
            return w;
        }

        w = traverse_find(node->left, data);
        if (w != NULL) {
            return w;
        }

        return traverse_find(node->right, data);
    }
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


static size_t copy_sorted_recurse(const struct rb_node *node, const char **dst)
{
    if (node != NULL) {
        size_t pos = copy_sorted_recurse(node->left, dst);
        struct word *w = rb_entry(node, struct word, rb_node);
        dst[pos++] = w->data;
        return pos + copy_sorted_recurse(node->right, &dst[pos]);
    }
}


static size_t copy_sorted_iterative(const struct rb_tree *tree, const char **dst)
{
    size_t i = 0;
    struct rb_node *node = rb_first(tree);

    while (node != NULL) {
        struct word *w = rb_entry(node, struct word, rb_node);
        dst[i++] = w->data;
        node = rb_next(node);
    }

    return i;
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
    const char *fruits[] = {
        "mango", "pear", "cherry", "plum", "banana", "orange", 
        "apple", "coconut", "avocado", "passion fruit", "huckleberry",
        "blueberry", "guava", "pomegranate", "cantaloupe", "notafruit",
        "grape", "dragonfruit", "blackberry", "grapefruit", "lime",
        "lemon", "apricot", "date", "fig", "clementine", "strawberry",
        "raspberry", "nectarine", "jujube", "star fruit"
    };

    const char *copy[sizeof(fruits) / sizeof(*fruits)];
    const char *copy2[sizeof(fruits) / sizeof(*fruits)];

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
    }

    for (size_t i = 0; i < sizeof(fruits) / sizeof(*fruits); ++i) {
        printf("Searching manually for %s\n", fruits[i]);
        fflush(stdout);
        struct word *w = search(tree.root, fruits[i]);
        assert(w != NULL);
    }

    const char *to_find[] = {"pear", "coconut", "apple"};
    for (size_t i = 0; i < sizeof(to_find) / sizeof(*to_find); ++i) {
        printf("Searching for %s\n");
        fflush(stdout);

        struct rb_node *node = rb_find(&tree, to_find[i], searchcmp);
        assert(node != NULL);
        struct word *w = rb_entry(node, struct word, rb_node);
        struct word *manual = search(tree.root, to_find[i]);
        assert(w == manual);
    }

    assert(rb_find(&tree, "NOT IN LIST", searchcmp) == NULL);

    memset(copy, 0, sizeof(copy));
    memset(copy2, 0, sizeof(copy2));
    size_t n = copy_sorted_recurse(tree.root, copy);
    assert(n == sizeof(fruits) / sizeof(*fruits));
    n = copy_sorted_iterative(&tree, copy2);
    assert(n == sizeof(fruits) / sizeof(*fruits));

    for (size_t i = 0; i < sizeof(copy) / sizeof(*copy); ++i) {
        printf("%s == %s\n", copy[i], copy2[i]);
        fflush(stdout);
        assert(copy[i] == copy2[i]);
    }

    // FIXME: make sure that arrays are actually sorted

    // Delete some items and make sure they are deleted
    const char *to_delete[] = {"pear", "mango", "banana", "apple", "plum"};
    for (size_t i = 0; i < sizeof(to_delete) / sizeof(*to_delete); ++i) {
        size_t count_before = count_nodes(tree.root);

        printf("Searching for %s\n", to_delete[i]);
        fflush(stdout);
        struct rb_node *node = rb_find(&tree, to_delete[i], searchcmp);
        assert(node != NULL);

        printf("Deleting %s\n", rb_entry(node, struct word, rb_node)->data);
        fflush(stdout);
        rb_remove(&tree, node);

        printf("Validating tree\n");
        fflush(stdout);
        size_t count_after = count_nodes(tree.root);
        printf("%zu == %zu - 1\n", count_after, count_before);
        fflush(stdout);
        assert(count_after == count_before - 1);
        validate_tree(&tree);

        node = rb_find(&tree, to_delete[i], searchcmp);
        assert(node == NULL);

        struct word *w = traverse_find(tree.root, to_delete[i]);
        assert(w == NULL);
    }

    memset(copy2, 0, sizeof(copy2));
    n = copy_sorted_iterative(&tree, copy2);
    for (size_t i = 0, j = 0; j < n; ++i) {
        assert(i < sizeof(copy) / sizeof(*copy));
        
        size_t k = 0;
        for (; k < sizeof(to_delete) / sizeof(*to_delete); ++k) {
            if (strcmp(to_delete[k], copy[i]) == 0) {
                break;
            }
        }
        if (k < sizeof(to_delete) / sizeof(*to_delete)) {
            printf("%s was deleted\n", copy[i]);
            fflush(stdout);
        } else {
            printf("%s == %s\n", copy2[j], copy[i]);
            fflush(stdout);
            assert(copy2[j] == copy[i]);
            ++j;
        }
    }

    validate_tree(&tree);

    fflush(stdout);
    // test adding a duplicate and searching for it 
    size_t count_before = count_nodes(tree.root);
    struct word *duplicate = alloc_word("notafruit");
    assert(duplicate != NULL);
    printf("Adding duplicate %s (%p)\n", duplicate->data, &duplicate->rb_node);
    rb_add(&tree, &duplicate->rb_node, wordcmp);
    size_t count_after = count_nodes(tree.root);
    assert(count_after == count_before + 1);
    validate_tree(&tree);

    printf("Searching for duplicate\n");
    fflush(stdout);
    struct rb_node *first = rb_find(&tree, "notafruit", searchcmp);

    assert(first != NULL);
    struct word *found = rb_entry(first, struct word, rb_node);
    assert(found != duplicate);

    printf("Duplicate %s found (%p)\n", found->data, &found->rb_node);
    fflush(stdout);

    count_before = count_after;
    rb_remove(&tree, first);
    count_after = count_nodes(tree.root);
    assert(count_after == count_before - 1);
    validate_tree(&tree);

    struct rb_node *second = rb_find(&tree, "notafruit", searchcmp);
    assert(second != NULL);
    assert(second != first);


    return 0;
}
