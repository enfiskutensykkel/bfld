#ifndef __BFLD_FILE_IO_H__
#define __BFLD_FILE_IO_H__

#include <stddef.h>


/*
 * Input file handle.
 * 
 * Holds the opened file descriptor, the memory-mapped pointer
 * and the size of the file.
 */
struct ifile
{
    int fd;
    size_t size;
    const void *ptr;
};



/*
 * Open a file as read-only and memory-map it's content,
 * for convenient file access.
 */
int ifile_open(struct ifile *fp, const char *path);



/*
 * Unmap and close an opened input file.
 */
void ifile_close(struct ifile *fp);


#endif
