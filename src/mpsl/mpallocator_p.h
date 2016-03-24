// [MPSL]
// Shader-Like Mathematical Expression JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPALLOCATOR_P_H
#define _MPSL_MPALLOCATOR_P_H

// [Dependencies - MPSL]
#include "./mpsl_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [mpsl::Allocator]
// ============================================================================

//! \internal
//!
//! Custom memory allocator used by MPSL, aligns to 16-bytes at least.
struct Allocator {
  MPSL_NO_COPY(Allocator)

  enum {
    //! How many bytes per a low granularity pool (has to be at least 16).
    kLoGranularity = 16,
    //! Number of slots of a low granularity pool.
    kLoCount = 16,
    //! Maximum size of a block that can be allocated in a low granularity pool.
    kLoMaxSize = kLoGranularity * kLoCount,

    //! How many bytes per a high granularity pool.
    kHiGranularity = 64,
    //! Number of slots of a high granularity pool.
    kHiCount = 4,
    //! Maximum size of a block that can be allocated in a high granularity pool.
    kHiMaxSize = kLoMaxSize + kHiGranularity * kHiCount,

    //! Alignment of every pointer returned by `alloc()`.
    kChunkAlignment = kLoGranularity,

    //! Malloc overhead, used to decrease each chunk size by this amount.
    kMallocOverhead = static_cast<int>(sizeof(void*) * 4),

    //! A minimal size of one chunk - the first chunk dynamically allocated
    //! will have this size.
    kMinChunkSize = 1024 * 16,
    //! A maximum size of one chunk - this is basically a threshold to stop
    //! multiplying the previous size.
    kMaxChunkSize = 1024 * 256
  };

  //! A chunk of memory allocated by LibC's `malloc` or on the stack if it's
  //! a first chunk provided by `AllocatorTmp<>`.
  struct Chunk {
    //! Link to the next chunk in a single-linked list.
    Chunk* next;
    //! Current pointer in the chunk to a 16-byte aligned memory.
    uint8_t* ptr;
    //! End of the chunk (the first invalid byte).
    uint8_t* end;
  };

  //! Single-linked list used to store unused blocks.
  struct Slot {
    //! Link to a next slot in a single-linked list.
    Slot* next;
  };

  //! A block of memory that has been allocated dynamically and is not part of
  //! chunk-list used by the allocator. This is used to keep track of all these
  //! blocks so they can be freed by `reset()` if not freed explicitly.
  struct DynamicBlock {
    DynamicBlock* prev;
    DynamicBlock* next;
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE Allocator() noexcept { ::memset(this, 0, sizeof(*this)); }
  MPSL_INLINE ~Allocator() noexcept { reset(true); }

  // --------------------------------------------------------------------------
  // [Clear / Reset]
  // --------------------------------------------------------------------------

  MPSL_NOAPI void reset(bool releaseMemory = true) noexcept;

  // --------------------------------------------------------------------------
  // [Utilities]
  // --------------------------------------------------------------------------

  //! \internal
  //!
  //! Get the slot index to be used for `size`. Returns `true` if a valid slot
  //! has been written to `slot` and `allocatedSize` has been filled with slot
  //! exact size (`allocatedSize` can be equal or slightly greater than `size`).
  static MPSL_INLINE bool _getSlotIndex(size_t size, uint32_t& slot) noexcept {
    MPSL_ASSERT(size > 0);
    if (size > kHiMaxSize)
      return false;

    if (size <= kLoMaxSize)
      slot = static_cast<uint32_t>((size - 1) / kLoGranularity);
    else
      slot = static_cast<uint32_t>((size - kLoMaxSize - 1) / kHiGranularity);

    return true;
  }

  //! \overload
  static MPSL_INLINE bool _getSlotIndex(size_t size, uint32_t& slot, size_t& allocatedSize) noexcept {
    MPSL_ASSERT(size > 0);
    if (size > kHiMaxSize)
      return false;

    if (size <= kLoMaxSize) {
      slot = static_cast<uint32_t>((size - 1) / kLoGranularity);
      allocatedSize = (slot + 1) * kLoGranularity;
    }
    else {
      slot = static_cast<uint32_t>((size - kLoMaxSize - 1) / kHiGranularity);
      allocatedSize = kLoMaxSize + (slot + 1) * kHiGranularity;
    }

    return true;
  }

  // --------------------------------------------------------------------------
  // [Alloc / Release]
  // --------------------------------------------------------------------------

  MPSL_NOAPI void* _alloc(size_t size, size_t& allocatedSize) noexcept;
  MPSL_NOAPI void* _allocZeroed(size_t size, size_t& allocatedSize) noexcept;
  MPSL_NOAPI char* _allocString(const char* s, size_t len) noexcept;
  MPSL_NOAPI void _releaseDynamic(void* p, size_t size) noexcept;

  //! Allocate `size` bytes of memory, ideally from an available pool.
  //!
  //! NOTE: `size` can't be zero, it will assert in debug mode in such case.
  MPSL_INLINE void* alloc(size_t size) noexcept {
    void* p;
    uint32_t slot;

    // NOTE: This is the most used allocation function, so the ideal path has
    // been inlined. If the `size` is a constant expression `_getSlotIndex()`
    // will be folded to a slot index directly and the code generated for the
    // whole allocation will be just few instructions.
    if (_getSlotIndex(size, slot) && (p = reinterpret_cast<uint8_t*>(_slots[slot])) != nullptr) {
      _slots[slot] = reinterpret_cast<Slot*>(p)->next;
      return p;
    }
    else {
      size_t dummy;
      return _alloc(size, dummy);
    }
  }

  //! Like `alloc(size)`, but provides a second argument `allocatedSize` that
  //! provides a way to know how big the block returned actually is. This is
  //! useful for containers to prevent growing too early.
  MPSL_INLINE void* alloc(size_t size, size_t& allocatedSize) noexcept {
    return _alloc(size, allocatedSize);
  }

  //! Like `alloc(size)`, but returns zeroed memory.
  MPSL_INLINE void* allocZeroed(size_t size) noexcept {
    size_t dummy;
    return allocZeroed(size, dummy);
  }

  //! Like `alloc(size, allocatedSize)`, but returns zeroed memory.
  MPSL_INLINE void* allocZeroed(size_t size, size_t& allocatedSize) noexcept {
    return _allocZeroed(size, allocatedSize);
  }

  //! Return a copy of string `s` of size `len`.
  MPSL_INLINE char* allocString(const char* s, size_t len) noexcept {
    return _allocString(s, len);
  }

  //! Release the memory previously allocated by `alloc()`. The `size` argument
  //! has to be the same as used to call `alloc()` or `allocatedSize` returned
  //! by `alloc()`.
  MPSL_INLINE void release(void* p, size_t size) noexcept {
    MPSL_ASSERT(p != nullptr);
    MPSL_ASSERT(size != 0);

    // NOTE: Inlined for the same reason as `alloc()`.
    uint32_t slot;
    if (_getSlotIndex(size, slot)) {
      static_cast<Slot*>(p)->next = static_cast<Slot*>(_slots[slot]);
      _slots[slot] = static_cast<Slot*>(p);
    }
    else {
      _releaseDynamic(p, size);
    }
  }

  MPSL_NOAPI bool multiAlloc(void** pDst, const size_t* sizes, size_t count) noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  Chunk* _first;
  Chunk* _current;
  DynamicBlock* _dynamicBlocks;

  Slot* _slots[kLoCount + kHiCount];
};

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPALLOCATOR_P_H
