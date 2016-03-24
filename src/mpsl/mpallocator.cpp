// [MPSL]
// Shader-Like Mathematical Expression JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MPSL_EXPORTS

// [Dependencies - MPSL]
#include "./mpallocator_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [mpsl::Allocator - Helpers]
// ============================================================================

template<typename T>
static MPSL_INLINE T mpAllocatorAlign(T p, uintptr_t alignment) noexcept {
  uintptr_t mask = static_cast<uintptr_t>(alignment - 1);
  return (T)(((uintptr_t)p + mask) & ~mask);
}

// ============================================================================
// [mpsl::Allocator - Clear / Reset]
// ============================================================================

void Allocator::reset(bool releaseMemory) noexcept {
  Chunk* chunk = _first;

  // Dynamic blocks are always freed, regardless of `releaseMemory`.
  DynamicBlock* block = _dynamicBlocks;
  _dynamicBlocks = nullptr;

  while (block != nullptr) {
    DynamicBlock* next = block->next;
    ::free(block);
    block = next;
  }

  if (releaseMemory) {
    _first = nullptr;

    while (chunk != nullptr) {
      Chunk* next = chunk->next;
      ::free(chunk);
      chunk = next;
    }
  }
  else {
    if (chunk != nullptr)
      chunk->ptr = mpAllocatorAlign<uint8_t*>(reinterpret_cast<uint8_t*>(chunk) + sizeof(Chunk), kChunkAlignment);
  }

  _current = chunk;
  ::memset(_slots, 0, MPSL_ARRAY_SIZE(_slots) * sizeof(Slot*));
}

// ============================================================================
// [mpsl::Allocator - Alloc / Release]
// ============================================================================

void* Allocator::_alloc(size_t size, size_t& allocatedSize) noexcept {
  uint32_t slot;

  // We use our memory pool only if the requested block is of reasonable size.
  if (_getSlotIndex(size, slot, allocatedSize)) {
    uint8_t* ptr = reinterpret_cast<uint8_t*>(_slots[slot]);
    Chunk* chunk = _current;

    size = allocatedSize;
    if (ptr != nullptr) {
      // Lucky, memory reuse.
      _slots[slot] = reinterpret_cast<Slot*>(ptr)->next;
      return ptr;
    }

    if (chunk != nullptr) {
      ptr = chunk->ptr;
      uint8_t* end = chunk->end;
      size_t remain = (size_t)(end - ptr);

      if (remain >= size) {
        // Allocate from the current chunk.
        chunk->ptr += size;
        return ptr;
      }
      else {
        // Distribute the remaining block to suitable slots.
        while (remain >= kLoGranularity) {
          size_t curSize = remain < kLoMaxSize ? remain : kLoMaxSize;
          slot = static_cast<uint32_t>(curSize - (kLoGranularity - 1)) / kLoCount;

          reinterpret_cast<Slot*>(ptr)->next = _slots[slot];
          _slots[slot] = reinterpret_cast<Slot*>(ptr);

          ptr += curSize;
          remain -= curSize;
        }
      }

      // Advance if there is a next chunk to the current one.
      if (chunk->next != nullptr) {
        chunk = chunk->next;
        ptr = mpAllocatorAlign<uint8_t*>(reinterpret_cast<uint8_t*>(chunk) + sizeof(Chunk), kChunkAlignment);

        _current = chunk;
        chunk->ptr = ptr + size;

        return ptr;
      }
    }

    // Allocate a new chunk.
    size_t chunkSize = 0;
    if (_current != nullptr)
      chunkSize = (static_cast<size_t>((uintptr_t)_current->end - (uintptr_t)_current) + kMallocOverhead) * 2;

    if (chunkSize < kMinChunkSize) chunkSize = kMinChunkSize;
    if (chunkSize > kMaxChunkSize) chunkSize = kMaxChunkSize;

    chunkSize -= kMallocOverhead;
    chunk = static_cast<Chunk*>(::malloc(chunkSize));
    ptr = mpAllocatorAlign<uint8_t*>(reinterpret_cast<uint8_t*>(chunk) + sizeof(Chunk), kChunkAlignment);

    if (chunk == nullptr) {
      allocatedSize = 0;
      return nullptr;
    }

    chunk->next = nullptr;
    chunk->ptr = ptr + size;
    chunk->end = reinterpret_cast<uint8_t*>(chunk) + chunkSize;

    if (_first == nullptr)
      _first = chunk;
    else
      _current->next = chunk;

    _current = chunk;
    return ptr;
  }
  else {
    // Allocate a dynamic block.
    size_t overhead = sizeof(DynamicBlock) + kChunkAlignment;

    // Handle a possible overflow.
    if (MPSL_UNLIKELY(~static_cast<size_t>(0) - size > overhead))
      return nullptr;

    void* p = ::malloc(size + overhead);
    if (MPSL_UNLIKELY(p == nullptr)) {
      allocatedSize = 0;
      return NULL;
    }

    // Link as first in `_dynamicBlocks` double-linked list.
    DynamicBlock* block = static_cast<DynamicBlock*>(p);
    DynamicBlock* next = _dynamicBlocks;

    if (next != nullptr)
      next->prev = block;

    block->next = next;
    _dynamicBlocks = block;

    // Align the pointer to the guaranteed alignment and store `DynamicBlock`
    // at the end of the memory block, so `_releaseDynamic()` can find it.
    p = mpAllocatorAlign(static_cast<char*>(p) + sizeof(DynamicBlock), kChunkAlignment);
    *reinterpret_cast<DynamicBlock**>(static_cast<char*>(p) + size) = block;

    allocatedSize = size;
    return p;
  }
}

void* Allocator::_allocZeroed(size_t size, size_t& allocatedSize) noexcept {
  void* p = alloc(size, allocatedSize);
  if (p != nullptr)
    ::memset(p, 0, allocatedSize);
  return p;
}

char* Allocator::_allocString(const char* s, size_t len) noexcept {
  size_t dummy;
  char* p = static_cast<char*>(alloc(len + 1, dummy));

  if (p == nullptr)
    return nullptr;

  // Copy string and NULL terminate as `s` doesn't have to be NULL terminated.
  if (len > 0)
    ::memcpy(p, s, len);
  p[len] = 0;

  return p;
}

void Allocator::_releaseDynamic(void* p, size_t size) noexcept {
  // Pointer to `DynamicBlock` is stored at the end of `p`.
  DynamicBlock* block = *reinterpret_cast<DynamicBlock**>(static_cast<char*>(p) + size);

  // Unlink and free.
  DynamicBlock* prev = block->prev;
  DynamicBlock* next = block->next;

  if (prev != nullptr)
    prev->next = next;
  else
    _dynamicBlocks = next;

  if (next != nullptr)
    next->prev = prev;

  ::free(block);
}

bool Allocator::multiAlloc(void** pDst, const size_t* sizes, size_t count) noexcept {
  MPSL_ASSERT(count > 0);

  size_t i = 0;
  do {
    void* p = alloc(sizes[i]);
    if (p == nullptr)
      goto _Fail;
    pDst[i] = p;
  } while (++i < count);
  return true;

_Fail:
  while (i > 0) {
    i--;
    release(pDst[i], sizes[i]);
  }
  return false;
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
