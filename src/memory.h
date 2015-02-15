/**
 * memory.h
 *  Sergio Gonzalez
 *
 * Interface for simple arena/zone/region memory managment.
 *
 */
#pragma once
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
    int8_t* ptr;
    size_t size;
    size_t count;
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
#define arena_push_elem(arena, T)           (T *)arena_push_bytes(arena, sizeof(T))
#define arena_push_array(arena, count, T)   (T *)arena_push_bytes(arena, (count) * sizeof(T))
void*   arena_push_bytes (Arena* arena, size_t num_bytes);

// =========================================
// ====            Reuse                ====
// =========================================
void arena_reset(Arena* arena);

// =========================================
//        Implementation
// =========================================
void* arena_push_bytes(Arena* arena, size_t num_bytes)
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
    void* ptr = arena_push_bytes(parent, size);
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
