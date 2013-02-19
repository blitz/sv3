#pragma once

#include <cstdint>

// template <typename KEY>
// using HashFunction = uint32_t (*)(KEY const &);

template <typename KEY, typename VALUE, uint32_t (*HASH)(KEY const &), unsigned BUCKETS>
class Hashtable {
  VALUE *_buckets[BUCKETS];

public:

  VALUE *&operator[](KEY const &key) {
    return _buckets[HASH(key) % BUCKETS];
  }

  Hashtable() : _buckets() {}
};

// EOF
