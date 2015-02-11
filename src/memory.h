#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#ifndef assert
#include <assert.h>
#endif

#include <stdint.h>

#define arena_push_elem(arena, type) (type*)arena_push_(arena, sizeof(type))
#define arena_push_array(arena, count, type) (type*)arena_push_(arena, (count) * sizeof(type))

typedef struct
{
    int8_t* ptr;
    size_t size;
    size_t count;
} Arena;

int8_t* arena_push_(Arena* arena, size_t num_bytes)
{
    assert((arena->count + num_bytes) <= arena->size);
    int8_t* result = arena->ptr + arena->count;
    arena->count += num_bytes;
    return result;
}

Arena create_arena_from_array(void* base, size_t size)
{
    Arena arena = {0};
    arena.ptr = (int8_t*)base;
    if (arena.ptr)
    {
        arena.size = size;
        arena.count = 0;
    }
    return arena;
}

Arena create_arena(Arena* parent, size_t size)
{
    void* ptr = arena_push_(parent, size);
    assert(ptr);

    Arena child;
    {
        child.size = size;
        child.ptr = ptr;
        child.count = 0;
    }
    return child;

}
#ifdef __cplusplus
}
#endif
