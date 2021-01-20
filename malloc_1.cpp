#include <unistd.h>
#include <cmath>

void* smalloc(size_t size){
    if (size == 0 || size > pow(10,8))
        return NULL;

    void* prev_prog_break = sbrk(size);
    if (prev_prog_break == (void*)(-1)) // sbrk failed
        return NULL;

    return sbrk(0);
}
