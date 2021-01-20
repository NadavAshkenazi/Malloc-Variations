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
MallocMetadata* first_metadata = NULL;

size_t _num_free_blocks(){
    int counter = 0;
    MallocMetadata* current_block = first_metadata;
    while (current_block != NULL) {
        if (current_block->is_free)
            counter++;
        current_block = current_block->next;
    }

    return counter;
}
size_t _num_free_bytes(){
    int counter = 0;
    MallocMetadata* current_block = first_metadata;
    while (current_block != NULL) {
        if (current_block->is_free)
            counter += current_block->size;
        current_block = current_block->next;
    }

    return counter;
}
size_t _num_allocated_blocks(){
    int counter = 0;
    MallocMetadata* current_block = first_metadata;
    while (current_block != NULL) {
        counter++;
        current_block = current_block->next;
    }
    return counter;
}
size_t _num_allocated_bytes(){
    int counter = 0;
    MallocMetadata* current_block = first_metadata;
    while (current_block != NULL) {
        counter += current_block->size;
        current_block = current_block->next;
    }

    return counter;
}
size_t _size_meta_data(){
    return sizeof(MallocMetadata);
}
size_t _num_meta_data_bytes(){
    return _num_allocated_blocks()*_size_meta_data();
}

void* _metadata_pointer_to_data_pointer(MallocMetadata* metadata_pointer){
    return (void*)(metadata_pointer + 1);
}
MallocMetadata* _data_pointer_to_metadata_pointer(void* p){
    return (MallocMetadata*)p - 1;
}


void* smalloc(size_t size){
    if (size == 0 || size > pow(10,8))
        return NULL;

    MallocMetadata* current_block = first_metadata;
    while (current_block != NULL){
        if (current_block->is_free && current_block->size >= size){
            current_block->is_free = false;
            return _metadata_pointer_to_data_pointer(current_block);
        }
        if (current_block->next == NULL){
            break;
        }
        current_block = current_block->next;
    }

    // allocate new memory
    MallocMetadata new_metadata;
    new_metadata.size = size;
    new_metadata.is_free = false;
    new_metadata.next = NULL;
    new_metadata.prev = current_block;

    MallocMetadata* new_allocation= (MallocMetadata*)sbrk(size+_size_meta_data());
    if ( new_allocation == (void*)(-1)) // sbrk failed
        return NULL;
    *new_allocation = new_metadata;

    // update the global metadata list
    if (first_metadata == NULL)
        first_metadata = new_allocation;
    else{
        if (current_block->next != NULL){
            current_block->next->prev = new_allocation;
        }
        current_block->next = new_allocation;
    }
    return _metadata_pointer_to_data_pointer(new_allocation);
}

void* scalloc(size_t num, size_t size){
    if (size == 0 || size*num > pow(10,8))
        return NULL;

    void* new_allocation = smalloc(size*num);
    if (new_allocation == NULL)
        return NULL;

    new_allocation = memset(new_allocation, 0, size*num);
    return new_allocation;

}

void sfree(void* p){
    if (p == NULL)
        return;

    MallocMetadata* p_metadata = _data_pointer_to_metadata_pointer(p);
    p_metadata->is_free = true;
    return;
}

void* srealloc(void* oldp, size_t size){
    if (size == 0 || size > pow(10,8))
        return NULL;

    if (oldp == NULL){
        void* new_allocation = smalloc(size);
        if (new_allocation == NULL) //sbrk fails
            return NULL;
        return new_allocation;
    }

    MallocMetadata* oldp_metadata = _data_pointer_to_metadata_pointer(oldp);
    if (size <= oldp_metadata->size) // reuse oldp
        return oldp;

    void* new_allocation = smalloc(size);
    if (new_allocation == NULL) //sbrk fails
        return NULL;

    memcpy(new_allocation, oldp, oldp_metadata->size);
    oldp_metadata->is_free = true;

    return new_allocation;
}

