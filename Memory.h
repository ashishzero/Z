#pragma once
#include "Common.h"

#ifndef M_ARENA_COMMIT_SIZE
constexpr umem M_ARENA_COMMIT_SIZE = KiloBytes(64);
#endif

enum M_Flags {
	M_CLEAR_MEMORY = 0x1,
};

struct M_Arena {
	umem     position;
	umem     committed;
	umem     reserved;
	M_Arena *next;
};

u8 *M_AlignPointer(u8 *location, umem alignment);

M_Arena *M_ArenaAllocate(umem max_size, umem commit_size = M_ARENA_COMMIT_SIZE);
void     M_ArenaFree(M_Arena *arena);
void     M_ArenaReset(M_Arena *arena);

bool     M_EnsureCommit(M_Arena *arena, umem pos);
bool     M_EnsurePosition(M_Arena *arena, umem pos);
bool     M_PackToPosition(M_Arena *arena, umem pos);
bool     M_Align(M_Arena *arena, umem alignment);

void *M_PushSize(M_Arena *arena, umem size, u32 flags = 0);
void *M_PushSizeAligned(M_Arena *arena, umem size, u32 alignment, u32 flags = 0);

#define  M_PushType(arena, type, ...)         (type *)M_PushSizeAligned(arena, sizeof(type), alignof(type), __VA_ARGS__)
#define  M_PushArray(arena, type, count, ...) (type *)M_PushSizeAligned(arena, sizeof(type) * (count), alignof(type), __VA_ARGS__)

void M_PopSize(M_Arena *arena, umem size);

struct M_Temporary {
	M_Arena *arena;
	umem   position;
};

M_Temporary M_BeginTemporaryMemory(M_Arena *arena);
void        M_EndTemporaryMemory(M_Temporary *temp);
void        M_FreeTemporaryMemory(M_Temporary *temp);

void *M_VirtualAlloc(void *ptr, umem size);
bool  M_VirtualCommit(void *ptr, umem size);
bool  M_VirtualDecommit(void *ptr, umem size);
bool  M_VirtualFree(void *ptr, umem size);
