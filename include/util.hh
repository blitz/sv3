// Copyright (C) 2013, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
// Economic rights: Technische Universitaet Dresden (Germany)

// This file is part of sv3.

// sv3 is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

// sv3 is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License version 2 for more details.

#pragma once

#include <cstdint>

#include <tuple>
#include <string>
#include <vector>


static inline uint64_t rdtsc()
{
  uint32_t lo, hi;
  asm volatile ("rdtsc" : "=a" (lo), "=d" (hi));
  return static_cast<uint64_t>(hi) << 32 | lo;
}

/// Like memcpy, but advances src and dst pointers just as rep movsb
/// does.
template <typename T>
static inline void movs(T * &dst, T const *&src, size_t size)
{
#if defined(__x86_64__) or defined(__i386__)
  // At least on Ivy Bridge upwards, this is the fastest way to copy
  // for larger amounts of data. We should check for the corresponding
  // CPUID bit.
  asm volatile ("rep; movsb"
                : "+D" (dst),
                  "+S" (src),
                  "+c" (size)
                :
                : "memory");
#else
#warning No optimized memcpy available.
  memcpy(dst, src, size);
  src_ptr    += size;
  dst_ptr    += size;
#endif
}

class Uncopyable {
private:
  Uncopyable(Uncopyable const &);
  Uncopyable const &operator=(Uncopyable const &) ;
protected:
  Uncopyable() {}
  virtual ~Uncopyable() {}
};

#define RELAX() asm volatile ("pause");

/// Call a method when the current scope is exitted.
/// XXX Use a closure here, but check whether this generates the same
/// code.
template<typename CLASS, typename RETURN, typename... Arguments>
class Finally {
  template<int ...>
  struct seq {};

  template<int N, int ...S>
  struct gens : gens<N-1, N-1, S...> {};

  template<int ...S>
  struct gens<0, S...>{ typedef seq<S...> type; };

  template <int ...S>
  RETURN call(seq<S...>)
  {
    (_instance->*_method)(std::get<S>(_arguments) ...);
  }

  typedef RETURN (CLASS::*method_ptr)(Arguments...);

  CLASS                   *_instance;
  method_ptr               _method;
  std::tuple<Arguments...> _arguments;

public:


  Finally(CLASS *instance, method_ptr method, Arguments... param)
    : _instance(instance), _method(method),
      _arguments(param...)
  { }

  ~Finally() {
    call(typename gens<sizeof...(Arguments)>::type());
  }

};

std::vector<std::string> string_split(std::string const &str, char delimiter);
std::string demangle(const char *name);
std::string hexdump(const void *p, unsigned len);

// EOF
