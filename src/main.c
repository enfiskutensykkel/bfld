#include <logging.h>
#include <mfile.h>
#include <stdlib.h>
#include <stdbool.h>


static bool load_file(const char *pathname)
{
    struct mfile *file = NULL;

    int status = mfile_open(&file, pathname);
    if (status != 0) {
        return false;
    }

    mfile_put(file);
    return true;
}



int main(int argc, char **argv)
{
    log_level = 5;

    for (int i = 1; i < argc; ++i) {
        if (!load_file(argv[i])) {
            log_fatal("Could not open file '%s'", argv[1]);
            exit(1);
        }
    }
    exit(0);
}
