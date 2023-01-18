#pragma once
#include "Memory.h"

typedef struct M_Pool {
	M_Arena *first;
	umem     cap;
} M_Pool;

void  M_PoolInit(M_Pool *pool, umem cap);
void *M_PoolPush(M_Pool *pool, umem size, u32 alignment, u32 flags);
void  M_PoolFree(M_Pool *pool);
