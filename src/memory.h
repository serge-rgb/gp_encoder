/**
 * memory.h
 *  Sergio Gonzalez
 *
 * Interface for simple arena/zone/region memory managment.
 * Includes functions for arrays.
 *
 */

#ifndef MEMORY_H_DEFINED
#define MEMORY_H_DEFINED

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef assert
#include <assert.h>
#endif

#include <stdint.h>

typedef struct Arena_s
{
    size_t size;
    size_t count;
    int8_t* ptr;
} Arena;

// =========================================
// ==== Arena creation                  ====
// =========================================

// Create a root arena from a memory block.
Arena arena_init(void* base, size_t size);
// Create a child arena.
Arena arena_spawn(Arena* parent, size_t size);

// =========================================
// ====          Allocation             ====
// =========================================
#define arena_alloc_elem(arena, T)           (T *)arena_alloc_bytes(arena, sizeof(T))
#define arena_alloc_array(arena, count, T)   (T *)arena_alloc_bytes(arena, (count) * sizeof(T))
void*   arena_alloc_bytes (Arena* arena, size_t num_bytes);

// =========================================
// ====            Reuse                ====
// =========================================
void arena_reset(Arena* arena);

// =========================================
// ====       Use as array              ====
// =========================================

// Usage example
//  int* foo = arena_array(&arena, max_capacity);
//  array_push(foo, 42);
//  array[0];  // -> 42
//  array_count(foo) // -> 1;
//
// *Will* reserve max_capacity, so it is not
// exactly like std::vector.
// You can forget about it when you clear its parent.

#pragma pack(push, 1)
typedef struct
{
    size_t size;
    size_t count;
} ArrayHeader;
#pragma pack(pop)

#define arena_array(arena, type, size) (type *)arena__array_typeless(arena, sizeof(type) * size)
void* arena__array_typeless(Arena* arena, size_t size)
{
    Arena child = arena_spawn(arena, size);
    ArrayHeader head = { 0 };
    {
        head.size = child.size;
    }
    memcpy(child.ptr, &head, sizeof(ArrayHeader));
    return (void*)(((uint8_t*)child.ptr) + sizeof(ArrayHeader));
}

#define array_push(array, elem) \
    (arena__array_try_grow(array), array[arena__array_header(array)->count - 1] = elem)

#define array_reset(array) \
    (arena__array_header(array)->count = 0)

#define array_count(array) \
    (arena__array_header(array)->count)

// =========================================
//        Implementation
// =========================================
#define arena__array_header(array) \
    ((ArrayHeader*)((uint8_t*)array - sizeof(ArrayHeader)))

void arena__array_try_grow(void* array)
{
    ArrayHeader* head = arena__array_header(array);
    assert(head->size >= (head->count + 1));
    ++head->count;
}

void* arena_alloc_bytes(Arena* arena, size_t num_bytes)
{
    size_t total = arena->count + num_bytes;
    assert(total <= arena->size);
    void* result = arena->ptr + arena->count;
    arena->count += num_bytes;
    return result;
}

Arena arena_init(void* base, size_t size)
{
    Arena arena = { 0 };
    arena.ptr = (int8_t*)base;
    if (arena.ptr)
    {
        arena.size   = size;
    }
    return arena;
}

Arena arena_spawn(Arena* parent, size_t size)
{
    void* ptr = arena_alloc_bytes(parent, size);
    assert(ptr);

    Arena child = { 0 };
    {
        child.ptr    = ptr;
        child.size   = size;
    }

    return child;
}

void arena_reset(Arena* arena)
{
    arena->count = 0;
}

#ifdef __cplusplus
}
#endif

#endif  // MEMORY_H_DEFINED
