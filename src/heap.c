#include "heap.h"


#include "debug.h"
#include "mutex.h"
#include "tlsf/tlsf.h"


#include <stddef.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "dbghelp.h"

#define CALLSTACK_DEPTH 10

typedef struct arena_t
{
	pool_t pool;
	struct arena_t* next;
} arena_t;

typedef struct heap_t
{
	tlsf_t tlsf;
	size_t grow_increment;
	arena_t* arena;
	mutex_t* mutex;
} heap_t;

heap_t* heap_create(size_t grow_increment)
{
	heap_t* heap = VirtualAlloc(NULL, sizeof(heap_t) + tlsf_size(),
		MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!heap)
	{
		debug_print(
			k_print_error,
			"OUT OF MEMORY!\n");
		return NULL;
	}

	heap->mutex = mutex_create();
	heap->grow_increment = grow_increment;
	heap->tlsf = tlsf_create(heap + 1);
	heap->arena = NULL;

	return heap;
}

void* heap_alloc(heap_t* heap, size_t size, size_t alignment)
{
	mutex_lock(heap->mutex);


	size_t real_size = size + sizeof(void*) * CALLSTACK_DEPTH;
	void* address = tlsf_memalign(heap->tlsf, alignment, real_size);
	if (!address)
	{
		size_t arena_size =
			__max(heap->grow_increment, real_size * 2) +
			sizeof(arena_t);
		arena_t* arena = VirtualAlloc(NULL,
			arena_size + tlsf_pool_overhead(),
			MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!arena)
		{
			debug_print(
				k_print_error,
				"OUT OF MEMORY!\n");
			return NULL;
		}

		arena->pool = tlsf_add_pool(heap->tlsf, arena + 1, arena_size);

		arena->next = heap->arena;
		heap->arena = arena;

		address = tlsf_memalign(heap->tlsf, alignment, real_size);
	}
	
	int traces = debug_backtrace((void**) address, CALLSTACK_DEPTH);
	
	mutex_unlock(heap->mutex);
	return ((char*)address + sizeof(void*) * CALLSTACK_DEPTH);
}

void heap_free(heap_t* heap, void* address)
{
	mutex_lock(heap->mutex);
	tlsf_free(heap->tlsf, (char*)address - sizeof(void*) * CALLSTACK_DEPTH);
	mutex_unlock(heap->mutex);
}

static void leak_walker(void* ptr, size_t size, int used, void* user) {
	if (used) {
		heap_t* heap = (heap_t*)user;

		debug_print(k_print_warning, "Memory leak of size %u bytes with callstack:\n", (uint32_t)(size - sizeof(void*) * CALLSTACK_DEPTH));
		callstack_printer(k_print_warning,  (void**)ptr, CALLSTACK_DEPTH);

	}
}

void heap_destroy(heap_t* heap)
{
	
	
	
	tlsf_destroy(heap->tlsf);

	arena_t* arena = heap->arena;
	while (arena)
	{
		arena_t* next = arena->next;
		tlsf_walk_pool(arena->pool, leak_walker, heap);
		
		VirtualFree(arena, 0, MEM_RELEASE);
		arena = next;
	}

	mutex_destroy(heap->mutex);

	VirtualFree(heap, 0, MEM_RELEASE);
}
