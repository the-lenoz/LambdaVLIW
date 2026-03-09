#ifndef MEMDUP_H
#define MEMDUP_H
#include <stdlib.h>
#include <string.h>

/**
 * Duplicates a block of memory.
 * 
 * @param src  Pointer to the memory block to duplicate.
 * @param len  Number of bytes to copy.
 * @return     A pointer to the newly allocated copy, or NULL if allocation fails.
 */
static void *memdup(const void *src, size_t len) {
    if (src == NULL) return NULL;

    // Allocate memory for the copy
    void *dest = malloc(len);

    // If allocation was successful, copy the data
    if (dest != NULL) {
        memcpy(dest, src, len);
    }

    return dest;
}
#endif // MEMDUP_H
