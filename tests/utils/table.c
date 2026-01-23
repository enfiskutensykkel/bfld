#include "table.h"
#include <assert.h>

TABLE_DECLARE(int, tbl);

int main(int argc, char **argv)
{
    struct tbl tbl;

    int v[] = {10, 20, 30, 40};
    uint64_t i[] = {1, 2, 3, 5};

    for (size_t k = 0; k < 4; ++k) {
        bool r = tbl_insert(&tbl, i[k], &v[k], NULL);
        assert(r);
    }

    int *e = NULL;
    bool r = tbl_insert(&tbl, i[2], &v[0], &e);
    assert(!r);
    assert(e != NULL);
    assert(*e == v[2]);

    return 0;
}
