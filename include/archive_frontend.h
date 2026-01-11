#ifndef BFLD_ARCHIVE_FRONTEND_H
#define BFLD_ARCHIVE_FRONTEND_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "archive.h"


/*
 * Archive file front-end operations.
 */
struct archive_frontend
{
    /*
     * Name of the archive front-end.
     */
    const char *name;

    /*
     * Determine if the memory mapped file content is a format
     * supported by the front-end.
     */
    bool (*probe_file)(const uint8_t *file_data, size_t file_size);

    /*
     * Parse file data and extract member and symbol metadata.
     */
    int (*parse_file)(const uint8_t *file_data, 
                      size_t file_size,
                      struct archive *archive);
};


/*
 * Register an archive file front-end.
 */
void archive_frontend_register(const struct archive_frontend *frontend);


/* 
 * Go through all registered archive file front-ends and try
 * to probe the memory area to find the front-end that supports
 * this format.
 */
const struct archive_frontend * archive_frontend_probe(const uint8_t *file_data, size_t file_size);


#ifdef __cplusplus
}
#endif
#endif
