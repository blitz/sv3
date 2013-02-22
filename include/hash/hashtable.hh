#pragma once

#include <cstdint>

template <typename KEY, typename VALUE, uint32_t (HASH)(KEY const &),
          size_t BUCKETS, unsigned WAYS, VALUE EMPTY>
class Hashtable {
  struct {
    VALUE v;
    KEY   k;
  } _buckets[BUCKETS][WAYS];

public:

  VALUE operator[](KEY const &key) {
    uint32_t index = HASH(key) % BUCKETS;
    auto &slot = _buckets[index];
    for (unsigned i = 0; i < WAYS; i++) {
      auto &way = slot[i];
      if (way.k == key) return way.v;
    }
    return nullptr;
  }

  void add(KEY const &key, VALUE value)
  {
    auto &slot = _buckets[HASH(key) % BUCKETS];

    // XXX Locking or atomic update!
  again:
    for (unsigned i = 0; i < WAYS; i++) {
      auto &way = slot[i];
      if (way.v == EMPTY) {
        way.k = key; way.v = value;
        return;
      }
    }

    // Reset way
    for (unsigned i = 0; i < WAYS; i++)
      slot[i].v = EMPTY;
 
    goto again;
  }

  Hashtable() : _buckets() {}
};

// EOF
