#include "Pool.h"

#include <stdlib.h>

static void *M_OutOfMemory() {
	TriggerBreakpoint();
	exit(1);
	return nullptr;
}

void M_PoolInit(M_Pool *pool, umem cap) {
	pool->first = M_ArenaAllocate(0, 0);
	pool->cap   = cap;
}

void *M_PoolPush(M_Pool *pool, umem size, u32 alignment, u32 flags) {
	void *ptr = M_PushSizeAligned(pool->first, size, alignment, flags);
	if (ptr) return ptr;

	M_Arena *arena = M_ArenaAllocate(pool->cap, 0);
	arena->next    = pool->first;
	pool->first  = arena;

	ptr = M_PushSizeAligned(pool->first, size, alignment, flags);
	if (ptr) return ptr;

	return M_OutOfMemory();
}

void M_PoolFree(M_Pool *pool) {
	for (M_Arena *arena = pool->first; arena; ) {
		M_Arena *temp = arena->next;
		M_ArenaFree(arena);
		arena = temp;
	}
}
