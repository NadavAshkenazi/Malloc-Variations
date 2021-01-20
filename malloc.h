//
// Created by shahar on 1/20/2021.
//

#ifndef OS_HW4_MALLOC_H
#define OS_HW4_MALLOC_H

#include <unistd.h>
#include <cmath>
#include <stdio.h>
#include <string.h>

typedef struct MallocMetadata{
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
} MallocMetadata;

size_t _num_free_blocks();

size_t _num_free_bytes();
size_t _num_allocated_blocks();
size_t _num_allocated_bytes();
size_t _size_meta_data();

void* _metadata_pointer_to_data_pointer(MallocMetadata* metadata_pointer);


void* smalloc(size_t size);
void* scalloc(size_t num, size_t size);

void sfree(void* p);

void* srealloc(void* oldp, size_t size);




#endif //OS_HW4_MALLOC_H
