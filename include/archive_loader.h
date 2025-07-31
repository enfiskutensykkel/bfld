#ifndef __BFLD_ARCHIVE_FILE_LOADER_H__
#define __BFLD_ARCHIVE_FILE_LOADER_H__
#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "objfile_loader.h"


/*
 * Representation of an archive file loader.
 */
struct archive_loader
{
    /*
     * The name of the archive file loader.
     */
    const char *name;

    /*
     * The object file loader used to extract archive members.
     */
    const struct objfile_loader *member_loader;

    /*
     * Determine if the memory mapped file is a format
     * that is supported by the archive file loader.
     */
    bool (*probe)(const uint8_t *file_data, size_t file_size);

    /*
     * Parse file data and allocate private file data handle (if needed).
     *
     * This function should set up necessary pointers and handles
     * to avoid parsing the file later.
     *
     * If this function returns anything but 0, it is assumed
     * to mean that a fatal error occurred and parsing is aborted.
     */
    int (*parse_file)(void **archive_loader_data,
                      const uint8_t *file_data,
                      size_t file_size);


    /*
     * Parse archive member metadata and emit them, one by one.
     */
    int (*parse_members)(void *archive_loader_data,
                         bool (*emit_member)(void *callback_data, 
                                             uint64_t member_id, 
                                             const char *name, 
                                             const uint8_t *content,
                                             size_t offset, 
                                             size_t size),
                         void *callback_data);


    /*
     * Parse the symbol index and emit symbols one by one.
     */
    int (*parse_symbol_index)(void *archive_loader_data,
                              bool (*emit_symbol)(void *callback_data, const char *name, uint64_t member_id),
                              void *callback_data);


    /*
     * Release the private data associated with the file; we're done with the file.
     */
    void (*release)(void *archive_loader_data);
};


/*
 * Register an archive file loader.
 */
int archive_loader_register(const struct archive_loader *loader);


/*
 * Try to look up an archive file loader by its name.
 */
const struct archive_loader * archive_loader_find(const char *name);


/*
 * Go through all registered archive file loaders and 
 * try to probe the memory area to check if the loader
 * supports this format.
 */
const struct archive_loader * archive_loader_probe(const uint8_t *file_data, size_t file_size);


/*
 * Mark a loader initializer so that it is invoked on startup.
 *
 * Example usage:
 *
 * const struct archive_loader my_loader = {
 *   .name = "my_ar_loader",
 *   .probe = my_ar_probe,
 *   .parse_file = &my_ar_parser,
 *   ...
 * }
 *
 * ARCHIVE_LOADER_INIT void my_ar_loader_init(void) {
 *     ...
 *     archive_loader_register(&my_loader);
 * }
 */
#define ARCHIVE_LOADER_INIT __attribute__((constructor))


#ifdef __cplusplus
}
#endif
#endif
