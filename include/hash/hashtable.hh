#pragma once

#include <cstdint>

template <typename KEY, typename VALUE, uint32_t (HASH)(KEY const &), size_t BUCKETS>
class Hashtable {
  struct {
    KEY    k;
    VALUE *v;
  } _buckets[BUCKETS];
  

public:

  VALUE *operator[](KEY const &key) {
    uint32_t index = HASH(key) % BUCKETS;
    auto &entry = _buckets[index];
    return (entry.k == key) ? entry.v : nullptr;
  }

  void add(KEY const &key, VALUE *value)
  {
    auto &entry = _buckets[HASH(key) % BUCKETS];
    // XXX Locking or atomic update!
    entry.k = key;
    entry.v = value;
  }

  Hashtable() : _buckets() {}
};

// EOF
