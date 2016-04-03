// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MPSL_EXPORTS

// [Dependencies - MPSL]
#include "./mphash_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [Helpers]
// ============================================================================

static const uint32_t mpPrimeTable[] = {
  19, 53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593
};

// ============================================================================
// [mpsl::HashUtils]
// ============================================================================

uint32_t HashUtils::hashString(const char* kStr, size_t kLen) noexcept {
  if (kLen == 0)
    return 0;

  uint32_t hVal = *kStr++;
  if (--kLen == 0)
    return hVal;

  do {
    hVal = hashChar(hVal, static_cast<unsigned char>(*kStr++));
  } while (--kLen);

  return hVal;
}

uint32_t HashUtils::closestPrime(uint32_t x) noexcept {
  uint32_t p, i = 0;

  do {
    if ((p = mpPrimeTable[i]) > x)
      break;
  } while (++i < MPSL_ARRAY_SIZE(mpPrimeTable));

  return p;
}

// ============================================================================
// [mpsl::HashBase - Reset / Rehash]
// ============================================================================

void HashBase::_rehash(uint32_t newCount) noexcept {
  Allocator* allocator = _allocator;

  HashNode** oldData = _data;
  HashNode** newData = static_cast<HashNode**>(
    allocator->allocZeroed(
      static_cast<size_t>(newCount + kExtraCount) * sizeof(void*)));

  if (newData == nullptr)
    return;

  uint32_t i;
  uint32_t oldCount = _bucketsCount;

  for (i = 0; i < oldCount; i++) {
    HashNode* node = oldData[i];
    while (node != nullptr) {
      HashNode* next = node->_next;
      uint32_t hMod = node->_hVal % newCount;

      node->_next = newData[hMod];
      newData[hMod] = node;

      node = next;
    }
  }

  // Move extra entries.
  for (i = 0; i < kExtraCount; i++) {
    newData[i + newCount] = oldData[i + oldCount];
  }

  // 90% is the maximum occupancy, can't overflow since the maximum capacity
  // is limited to the last prime number stored in the `mpPrimeTable[]` array.
  _bucketsCount = newCount;
  _bucketsGrow = newCount * 9 / 10;

  _data = newData;
  if (oldData != _embedded)
    allocator->release(oldData,
      static_cast<size_t>(oldCount + kExtraCount) * sizeof(void*));
}

void HashBase::_mergeToInvisibleSlot(HashBase& other) noexcept {
  uint32_t i, count = other._bucketsCount + kExtraCount;
  HashNode** data = other._data;

  HashNode* first;
  HashNode* last;

  // Find the `first` node.
  for (i = 0; i < count; i++) {
    first = data[i];
    if (first != nullptr)
      break;
  }

  if (first != nullptr) {
    // Initialize `first` and `last`.
    last = first;
    while (last->_next != nullptr)
      last = last->_next;
    data[i] = nullptr;

    // Iterate over the rest and append so `first` stay the same and `last`
    // is updated to the last node added.
    while (++i < count) {
      HashNode* node = data[i];
      if (node != nullptr) {
        last->_next = node;
        last = node;
        while (last->_next != nullptr)
          last = last->_next;
        data[i] = nullptr;
      }
    }

    // Link with ours.
    if (last != nullptr) {
      i = _bucketsCount + kExtraFirst;
      last->_next = _data[i];
      _data[i] = first;
    }
  }
}

// ============================================================================
// [mpsl::HashBase - Ops]
// ============================================================================

HashNode* HashBase::_put(HashNode* node) noexcept {
  uint32_t hMod = node->_hVal % _bucketsCount;
  HashNode* next = _data[hMod];

  node->_next = next;
  _data[hMod] = node;

  if (++_length >= _bucketsGrow && next != nullptr) {
    uint32_t newCapacity = HashUtils::closestPrime(_bucketsCount + kExtraCount);
    if (newCapacity != _bucketsCount)
      _rehash(newCapacity);
  }

  return node;
}

HashNode* HashBase::_del(HashNode* node) noexcept {
  uint32_t hMod = node->_hVal % _bucketsCount;

  HashNode** pPrev = &_data[hMod];
  HashNode* p = *pPrev;

  while (p != nullptr) {
    if (p == node) {
      *pPrev = p->_next;
      return node;
    }

    pPrev = &p->_next;
    p = *pPrev;
  }

  return nullptr;
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
