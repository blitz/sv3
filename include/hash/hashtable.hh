#pragma once

#include <cstdint>

// template <typename KEY>
// using HashFunction = uint32_t (*)(KEY const &);

template <typename KEY, typename VALUE, uint32_t (*HASH)(KEY const &), unsigned BUCKETS>
class Hashtable {
  struct {
    KEY    k;
    VALUE *v;
  } _buckets[BUCKETS];
  

public:

  VALUE *&operator[](KEY const &key) {
    auto &entry = _buckets[HASH(key)];
    return (entry.k == key) ? entry.v : nullptr;
  }

  void add(KEY const &key, VALUE *value)
  {
    auto &entry = _buckets[HASH(key)];
    // XXX Locking or atomic update!
    entry.k = key;
    entry.v = value;
  }

  Hashtable() : _buckets() {}
};

// EOF
