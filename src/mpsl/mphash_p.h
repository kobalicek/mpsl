// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPHASH_P_H
#define _MPSL_MPHASH_P_H

// [Dependencies - MPSL]
#include "./mpallocator_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [mpsl::HashUtils]
// ============================================================================

namespace HashUtils {
  // \internal
  static MPSL_INLINE uint32_t hashPointer(const void* kPtr) noexcept {
    uintptr_t p = (uintptr_t)kPtr;
    return static_cast<uint32_t>(
      ((p >> 3) ^ (p >> 7) ^ (p >> 12) ^ (p >> 20) ^ (p >> 27)) & 0xFFFFFFFFU);
  }

  // \internal
  static MPSL_INLINE uint32_t hashChar(uint32_t hash, uint32_t c) noexcept {
    return hash * 65599 + c;
  }

  // \internal
  //
  // Get a hash of the given string `kStr` of `kLen` length. This function doesn't
  // require `kStr` to be NULL terminated.
  MPSL_NOAPI uint32_t hashString(const char* kStr, size_t kLen) noexcept;
  //! \overload
  static MPSL_INLINE uint32_t hashString(const StringRef& str) noexcept {
    return hashString(str.getData(), str.getLength());
  }

  // \internal
  //
  // Get a prime number that is close to `x`, but always greater than or equal to `x`.
  MPSL_NOAPI uint32_t closestPrime(uint32_t x) noexcept;
};

// ============================================================================
// [mpsl::HashNode]
// ============================================================================

struct HashNode {
  MPSL_INLINE HashNode(uint32_t hVal = 0) noexcept
    : _next(nullptr), _hVal(hVal) {}

  //! Next node in the chain, `nullptr` if last node.
  HashNode* _next;
  //! Hash code.
  uint32_t _hVal;
};

// ============================================================================
// [mpsl::HashBase]
// ============================================================================

struct HashBase {
  MPSL_NO_COPY(HashBase)

  enum {
    kExtraFirst = 0,
    kExtraCount = 1
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE HashBase(Allocator* allocator) noexcept {
    _allocator = allocator;
    _length = 0;

    _bucketsCount = 1;
    _bucketsGrow = 1;

    _data = _embedded;
    for (uint32_t i = 0; i <= kExtraCount; i++)
      _embedded[i] = nullptr;
  }

  MPSL_INLINE ~HashBase() noexcept {
    if (_data != _embedded)
      _allocator->release(_data, static_cast<size_t>(_bucketsCount + kExtraCount) * sizeof(void*));
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE Allocator* getAllocator() const noexcept { return _allocator; }

  // --------------------------------------------------------------------------
  // [Ops]
  // --------------------------------------------------------------------------

  void _rehash(uint32_t newCount) noexcept;
  void _mergeToInvisibleSlot(HashBase& other) noexcept;

  HashNode* _put(HashNode* node) noexcept;
  HashNode* _del(HashNode* node) noexcept;

  // --------------------------------------------------------------------------
  // [Reset / Rehash]
  // --------------------------------------------------------------------------

  Allocator* _allocator;

  uint32_t _length;
  uint32_t _bucketsCount;
  uint32_t _bucketsGrow;

  HashNode** _data;
  HashNode* _embedded[1 + kExtraCount];
};

// ============================================================================
// [mpsl::Hash<Key, Node>]
// ============================================================================

//! \internal
//!
//! Low level hash table container used by MPSL, with some "special" features.
//!
//! Notes:
//!
//! 1. This hash table allows duplicates to be inserted (the API is so low
//!    level that it's up to you if you allow it or not, as you should first
//!    `get()` the node and then modify it or insert a new node by using `put()`,
//!    depending on the intention).
//!
//! 2. This hash table also contains "invisible" nodes that are not used by
//!    the basic hash functions, but can be used by functions having "invisible"
//!    in their name. These are used by the parser to merge symbols from the
//!    current scope that is being closed into the root local scope, but without
//!    making these symbols visible to the symbol resolver. This feature can be
//!    completely ignored if not needed.
//!
//! Hash is currently used by AST to keep references of global and local
//! symbols and by AST to IR translator to associate IR specific data with AST.
template<typename Key, typename Node>
struct Hash : public HashBase {
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE Hash(Allocator* allocator) noexcept
    : HashBase(allocator) {}

  // --------------------------------------------------------------------------
  // [Ops]
  // --------------------------------------------------------------------------

  template<typename ReleaseHandler>
  void reset(ReleaseHandler& handler) noexcept {
    HashNode** data = _data;
    uint32_t count = _bucketsCount + kExtraCount;

    for (uint32_t i = 0; i < count; i++) {
      HashNode* node = data[i];

      while (node != nullptr) {
        HashNode* next = node->_next;
        handler.release(static_cast<Node*>(node));
        node = next;
      }
    }

    if (data != _embedded)
      _allocator->release(data, static_cast<size_t>(count + kExtraCount) * sizeof(void*));

    _bucketsCount = 1;
    _bucketsGrow = 1;

    _length = 0;
    _data = _embedded;

    for (uint32_t i = 0; i <= kExtraCount; i++)
      _embedded[i] = nullptr;
  }

  MPSL_INLINE void mergeToInvisibleSlot(Hash<Key, Node>& other) noexcept {
    _mergeToInvisibleSlot(other);
  }

  MPSL_INLINE Node* get(const Key& key, uint32_t hVal) const noexcept {
    uint32_t hMod = hVal % _bucketsCount;
    Node* node = static_cast<Node*>(_data[hMod]);

    while (node != nullptr) {
      if (node->eq(key))
        return node;
      node = static_cast<Node*>(node->_next);
    }

    return nullptr;
  }

  MPSL_INLINE Node* put(Node* node) noexcept { return static_cast<Node*>(_put(node)); }
  MPSL_INLINE Node* del(Node* node) noexcept { return static_cast<Node*>(_del(node)); }
};

// ============================================================================
// [mpsl::Map<Key, Value>]
// ============================================================================

//! \internal
template<typename Key, typename Value>
struct Map {
  MPSL_NO_COPY(Map)

  struct Node : public HashNode {
    MPSL_INLINE Node(const Key& key, const Value& value, uint32_t hVal) noexcept
      : HashNode(hVal),
        _key(key),
        _value(value) {}
    MPSL_INLINE bool eq(const Key& key) noexcept { return _key == key; }

    Key _key;
    Value _value;
  };

  struct ReleaseHandler {
    MPSL_INLINE ReleaseHandler(Allocator* allocator) noexcept
      : _allocator(allocator) {}

    MPSL_INLINE void release(Node* node) noexcept {
      _allocator->release(node, sizeof(Node));
    }

    Allocator* _allocator;
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE Map(Allocator* allocator) noexcept
    : _hash(allocator) {}

  MPSL_INLINE ~Map() noexcept {
    ReleaseHandler releaseHandler(_hash.getAllocator());
    _hash.reset(releaseHandler);
  }

  // --------------------------------------------------------------------------
  // [Ops]
  // --------------------------------------------------------------------------

  MPSL_INLINE bool has(const Key& key) const noexcept {
    uint32_t hVal = HashUtils::hashPointer(key);
    uint32_t hMod = hVal % _hash._bucketsCount;
    Node* node = static_cast<Node*>(_hash._data[hMod]);

    while (node != nullptr) {
      if (node->eq(key))
        return true;
      node = static_cast<Node*>(node->_next);
    }

    return false;
  }

  MPSL_INLINE Value get(const Key& key) const noexcept {
    uint32_t hVal = HashUtils::hashPointer(key);
    uint32_t hMod = hVal % _hash._bucketsCount;
    Node* node = static_cast<Node*>(_hash._data[hMod]);

    while (node != nullptr) {
      if (node->eq(key))
        return node->_value;
      node = static_cast<Node*>(node->_next);
    }

    return Value();
  }

  MPSL_INLINE Error put(const Key& key, const Value& value) noexcept {
    Node* node = static_cast<Node*>(_hash._allocator->alloc(sizeof(Node)));
    if (node == nullptr)
      return MPSL_TRACE_ERROR(kErrorNoMemory);

    uint32_t hVal = HashUtils::hashPointer(key);
    _hash.put(new(node) Node(key, value, hVal));

    return kErrorOk;
  }

  Hash<Key, Node> _hash;
};

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPHASH_P_H
