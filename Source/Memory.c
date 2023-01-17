#include "Memory.h"

#include <string.h>

#ifndef M_ARENA_COMMIT_SIZE
static const umem M_ARENA_COMMIT_SIZE = KiloBytes(64);
#endif

static M_Arena EmptyArena = { 0,0,0, nullptr };

u8 *M_AlignPointer(u8 *location, umem alignment) {
	return (u8 *)((umem)(location + (alignment - 1)) & ~(alignment - 1));
}

M_Arena *M_ArenaAllocate(umem max_size, umem initial_size) {
	if (max_size == 0) {
		return (M_Arena *)&EmptyArena;
	}

	max_size = AlignPower2Up(max_size, 64 * 1024);
	u8 *mem = (u8 *)M_VirtualAlloc(0, max_size);
	if (mem) {
		umem commit_size = AlignPower2Up(initial_size, M_ARENA_COMMIT_SIZE);
		commit_size = Clamp(M_ARENA_COMMIT_SIZE, max_size, commit_size);
		if (M_VirtualCommit(mem, commit_size)) {
			M_Arena *arena   = (M_Arena *)mem;
			arena->position  = sizeof(M_Arena);
			arena->reserved  = max_size;
			arena->committed = commit_size;
			arena->next      = (M_Arena *)&EmptyArena;
			return arena;
		}
		M_VirtualFree(mem, max_size);
	}

	return (M_Arena *)&EmptyArena;
}

void M_ArenaFree(M_Arena *arena) {
	if (arena != &EmptyArena)
		M_VirtualFree(arena, arena->reserved);
}

void M_ArenaReset(M_Arena *arena) {
	arena->position = sizeof(M_Arena);
}

bool M_EnsureCommit(M_Arena *arena, umem pos) {
	if (pos <= arena->committed) {
		return true;
	}

	pos = Max(pos, M_ARENA_COMMIT_SIZE);
	u8 *mem = (u8 *)arena;

	umem committed = AlignPower2Up(pos, M_ARENA_COMMIT_SIZE);
	committed = Min(committed, arena->reserved);
	if (M_VirtualCommit(mem + arena->committed, committed - arena->committed)) {
		arena->committed = committed;
		return true;
	}
	return false;
}

bool M_EnsurePosition(M_Arena *arena, umem pos) {
	if (M_EnsureCommit(arena, pos)) {
		arena->position = pos;
		return true;
	}
	return false;
}

bool M_PackToPosition(M_Arena *arena, umem pos) {
	if (M_EnsurePosition(arena, pos)) {
		umem committed = AlignPower2Up(pos, M_ARENA_COMMIT_SIZE);
		committed = Clamp(M_ARENA_COMMIT_SIZE, arena->reserved, committed);

		u8 *mem = (u8 *)arena;
		if (committed < arena->committed) {
			if (M_VirtualDecommit(mem + committed, arena->committed - committed))
				arena->committed = committed;
		}
		return true;
	}
	return false;
}

bool M_Align(M_Arena *arena, umem alignment) {
	u8 *mem = (u8 *)arena + arena->position;
	u8 *aligned = M_AlignPointer(mem, alignment);
	umem pos = arena->position + (aligned - mem);
	if (M_EnsurePosition(arena, pos))
		return true;
	return false;
}

void *M_PushSize(M_Arena *arena, umem size, u32 flags) {
	u8 *mem = (u8 *)arena + arena->position;
	umem pos = arena->position + size;
	if (M_EnsurePosition(arena, pos)) {
		if (flags & M_CLEAR_MEMORY)
			memset(mem, 0, size);
		return mem;
	}
	return nullptr;
}

void *M_PushSizeAligned(M_Arena *arena, umem size, u32 alignment, u32 flags) {
	if (M_Align(arena, alignment))
		return M_PushSize(arena, size, flags);
	return nullptr;
}

M_Temp M_BeginTemporaryMemory(M_Arena *arena) {
	M_Temp mem;
	mem.arena    = arena;
	mem.position = arena->position;
	return mem;
}

void M_PopSize(M_Arena *arena, umem size) {
	umem pos = arena->position - size;
	Assert(pos >= sizeof(M_Arena) && pos <= arena->reserved);
	M_EnsurePosition(arena, pos);
}

void M_EndTemporaryMemory(M_Temp *temp) {
	temp->arena->position = temp->position;
}

void M_FreeTemporaryMemory(M_Temp *temp) {
	M_EnsurePosition(temp->arena, temp->position);
	M_PackToPosition(temp->arena, temp->position);
}

//
//
//

#if PLATFORM_WINDOWS == 1
#pragma warning(push)
#pragma warning(disable : 5105)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#pragma warning(pop)

void *M_VirtualAlloc(void *ptr, umem size) {
	return VirtualAlloc(ptr, size, MEM_RESERVE, PAGE_READWRITE);
}

bool M_VirtualCommit(void *ptr, umem size) {
	return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != NULL;
}

bool M_VirtualDecommit(void *ptr, umem size) {
	return VirtualFree(ptr, size, MEM_DECOMMIT);
}

bool M_VirtualFree(void *ptr, umem size) {
	return VirtualFree(ptr, 0, MEM_RELEASE);
}

#endif

#if PLATFORM_LINUX == 1 || PLATFORM_MAC == 1
#include <sys/mman.h>
#include <stdlib.h>

void *M_VirtualAlloc(void *ptr, umem size) {
	void *result = mmap(ptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (result == MAP_FAILED)
		return NULL;
	return result;
}

bool M_VirtualCommit(void *ptr, umem size) {
	return mprotect(ptr, size, PROT_READ | PROT_WRITE) == 0;
}

bool M_VirtualDecommit(void *ptr, umem size) {
	return mprotect(ptr, size, PROT_NONE) == 0;
}

bool M_VirtualFree(void *ptr, umem size) {
	return munmap(ptr, size) == 0;
}

#endif