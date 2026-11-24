#ifndef MAP_ROTATION_H
#define MAP_ROTATION_H

#include "header/shared.h"
#include <stdio.h>

/* Write the next map only if the requested section contains a usable entry. */
bool MapRotation_Next(FILE *file, const char *section, const char *current,
                      char *next, size_t capacity);

#endif
