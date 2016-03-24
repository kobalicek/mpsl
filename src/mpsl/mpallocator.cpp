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
  if (releaseMemory) {
    Chunk* chunk = _first;
    _first = nullptr;

    while (chunk != nullptr) {
      Chunk* next = chunk->next;
      ::free(chunk);
      chunk = next;
    }
  }
  else {
  }

  _current = _first;
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
    void* p = ::malloc(size);
    allocatedSize = p != nullptr ? size : static_cast<size_t>(0);
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

  // Copy string and NULL terminate as `s` doesn't have to be nullptr terminated.
  if (len > 0)
    ::memcpy(p, s, len);
  p[len] = 0;

  return p;
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
