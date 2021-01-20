#include <unistd.h>
#include <cmath>
#include <string.h>
#include <sys/mman.h>
#include <iostream>

#define MMAP_SIZE 128*1000

typedef struct MallocMetadata{
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
} MallocMetadata;
MallocMetadata* first_metadata = NULL;
MallocMetadata* first_metadata_mmap = NULL;

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

    MallocMetadata* current_mmap_block = first_metadata_mmap;
    while (current_mmap_block != NULL) {
        counter++;
        current_mmap_block = current_mmap_block->next;
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

    MallocMetadata* current_mmap_block = first_metadata_mmap;
    while (current_mmap_block != NULL) {
        counter += current_mmap_block->size;
        current_mmap_block = current_mmap_block->next;
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

    return (void*)(metadata_pointer+1);
}
MallocMetadata* _data_pointer_to_metadata_pointer(void* p){
    return (MallocMetadata*)p - 1;
}

bool _can_be_splited(size_t block_size, size_t size){
    if (block_size >= size + 128 + _size_meta_data())
        return true;

    return false;
}

void* _split_block(MallocMetadata* current_metadata, size_t size){
    // create new metadata
    MallocMetadata new_metadata;
    new_metadata.size = current_metadata->size - size - _size_meta_data();
    new_metadata.is_free = true;
    new_metadata.next = current_metadata->next;
    new_metadata.prev = current_metadata;

    MallocMetadata* new_metadata_location = (MallocMetadata*)((char*)_metadata_pointer_to_data_pointer(current_metadata)+size);
    *new_metadata_location = new_metadata;

    // allocate the given size
    current_metadata->is_free = false;
    current_metadata->size = size;
    if (current_metadata->next != NULL)
        current_metadata->next->prev = new_metadata_location;
    current_metadata->next = new_metadata_location;

    return _metadata_pointer_to_data_pointer(new_metadata_location);
}

void* _merge_blocks(MallocMetadata* prev_block , MallocMetadata* next_block){
    prev_block->size += next_block->size + _size_meta_data();
    prev_block->next = next_block->next;
    if (next_block->next != NULL)
        next_block->next->prev = prev_block;

    return _metadata_pointer_to_data_pointer(prev_block);
}

void* _create_mmap_allocation(size_t size) {
    MallocMetadata new_metadata;
    new_metadata.size = size;
    new_metadata.is_free = false;
    new_metadata.next = NULL;
    new_metadata.prev = NULL;

    MallocMetadata* new_allocation_mmap = (MallocMetadata*)mmap(NULL, size+_size_meta_data(), PROT_WRITE | PROT_READ , MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ( new_allocation_mmap == (void*)(-1)) // mmap failed
        return NULL;
    *new_allocation_mmap = new_metadata;

    MallocMetadata* current_block = first_metadata_mmap;
    if (current_block == NULL){ // first block in mmap list
        first_metadata_mmap = new_allocation_mmap;
        return _metadata_pointer_to_data_pointer(new_allocation_mmap);
    }
    while (current_block->next != NULL) // find last block in mmap list
        current_block = current_block->next;

    current_block->next = new_allocation_mmap;
    new_allocation_mmap->prev = current_block;

    return _metadata_pointer_to_data_pointer(new_allocation_mmap);
}
void _free_mmap_allocation(void* p){
    MallocMetadata* p_metadata = _data_pointer_to_metadata_pointer(p);
    if (p_metadata->prev != NULL)
        p_metadata->prev->next = p_metadata->next;
    if (p_metadata->next != NULL)
        p_metadata->next->prev = p_metadata->prev;

    if (p_metadata == first_metadata_mmap)
        first_metadata_mmap = p_metadata->next;

    if (munmap(p, p_metadata->size + _size_meta_data()) == -1){
        return; // TODO what to return?
    }
    return;
}

void* smalloc(size_t size){
    if (size == 0 || size > pow(10,8))
        return NULL;

    if (size >= MMAP_SIZE){
        return _create_mmap_allocation(size);
    }

    MallocMetadata* current_block = first_metadata;
    while (current_block != NULL){
        if (current_block->is_free && current_block->size >= size){
            if (_can_be_splited(current_block->size, size)){
                return _split_block(current_block, size);
            }
            current_block->is_free = false;
            return _metadata_pointer_to_data_pointer(current_block);
        }
        // Check if last block is free - is so, enlarge the wilderness
        if (current_block->next == NULL){
            if (current_block->is_free){
                current_block->size = size;
                current_block->is_free = false;
                return _metadata_pointer_to_data_pointer(current_block);
            }
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

    MallocMetadata* new_allocation = (MallocMetadata*)sbrk(size + _size_meta_data());
    if (new_allocation == (void*)(-1)) // sbrk failed
        return NULL;
    *new_allocation = new_metadata;

    if (first_metadata == NULL) // update the metadata list
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

    memset(new_allocation, 0, size*num);
    return new_allocation;
}

void sfree(void* p){
    if (p == NULL)
        return;

    MallocMetadata* p_metadata = _data_pointer_to_metadata_pointer(p);
    if (p_metadata->size >= MMAP_SIZE){
        return _free_mmap_allocation(p);
    }

    p_metadata->is_free = true;

    // try to combine with adjacent free blocks
    if (p_metadata->next != NULL && p_metadata->next->is_free){
        p_metadata = _data_pointer_to_metadata_pointer(_merge_blocks(p_metadata, p_metadata->next));
    }
    if (p_metadata->prev != NULL && p_metadata->prev->is_free){
        p_metadata = _data_pointer_to_metadata_pointer(_merge_blocks(p_metadata->prev, p_metadata));
    }

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

    if (size >= MMAP_SIZE){
        if (size == oldp_metadata->size)
            return oldp;
        if (size < oldp_metadata->size){
            void* new_mmap_allocation = _create_mmap_allocation(size);
            memcpy(new_mmap_allocation, oldp, std::min(oldp_metadata->size,size));
            _free_mmap_allocation(oldp);
            return new_mmap_allocation;
        }
    }

    if (oldp_metadata->size >= size) { // reuse oldp
        if (_can_be_splited(oldp_metadata->size, size)) {
            _split_block(oldp_metadata, size);
        }
        return oldp;
    }

    // merge adjacent cells
    if (oldp_metadata->prev != NULL && oldp_metadata->prev->is_free && oldp_metadata->prev->size+oldp_metadata->size+_size_meta_data() >= size) {
        void* new_allocation = _merge_blocks(oldp_metadata->prev, oldp_metadata);
        MallocMetadata* new_allocation_metadata = _data_pointer_to_metadata_pointer(new_allocation);
        (new_allocation_metadata)->is_free = false;
        if (_can_be_splited(new_allocation_metadata->size, size)) {
            _split_block(new_allocation_metadata, size);
        }
        return new_allocation;
    }
    if (oldp_metadata->next != NULL && oldp_metadata->next->is_free && oldp_metadata->next->size+oldp_metadata->size+_size_meta_data() >= size) {
        void* new_allocation = _merge_blocks(oldp_metadata, oldp_metadata->next);
        MallocMetadata* new_allocation_metadata = _data_pointer_to_metadata_pointer(new_allocation);
        (new_allocation_metadata)->is_free = false;
        if (_can_be_splited(new_allocation_metadata->size, size)) {
           _split_block(new_allocation_metadata, size);
        }
        return new_allocation;
    }
    if (oldp_metadata->prev != NULL && oldp_metadata->next != NULL && oldp_metadata->prev->is_free && oldp_metadata->next->is_free
        && oldp_metadata->prev->size+oldp_metadata->next->size+oldp_metadata->size+2*_size_meta_data() >= size){
        MallocMetadata* temp_metadata = _data_pointer_to_metadata_pointer(_merge_blocks(oldp_metadata->prev, oldp_metadata));
        void* new_allocation = _merge_blocks(temp_metadata, oldp_metadata->next);
        MallocMetadata* new_allocation_metadata = _data_pointer_to_metadata_pointer(new_allocation);
        (new_allocation_metadata)->is_free = false;
        if (_can_be_splited(new_allocation_metadata->size, size)) {
            _split_block(new_allocation_metadata, size);
        }
        return new_allocation;
    }

    // wildernes block
    if (oldp_metadata->next == NULL){ // oldp is the last block
        oldp_metadata->size = size;
        oldp_metadata->is_free = false;
        return _metadata_pointer_to_data_pointer(oldp_metadata);
    }

    // can't re-allocate, find new allocation
    void* new_allocation = smalloc(size);
    if (new_allocation == NULL) //sbrk fails
        return NULL;

    memcpy(new_allocation, oldp, oldp_metadata->size);
    sfree(oldp);

    return new_allocation;
}