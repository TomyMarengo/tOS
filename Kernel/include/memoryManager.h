#ifndef _MEMORY_MANAGER_H_
#define _MEMORY_MANAGER_H_

/* Standard library */
#include <stdint.h>
#include <stddef.h>

/* Local headers */
#include "string.h"

void my_init(void* memoryStart, size_t memorySize);

void* my_malloc(size_t size);

int my_free(void* ptr);

#endif