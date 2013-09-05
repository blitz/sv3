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


template<typename T>
class Finally {

  T &_closure;

public:
  Finally(T &closure) : _closure(closure) {}
  ~Finally() { _closure(); }
};

std::vector<std::string> string_split(std::string const &str, char delimiter);
std::string demangle(const char *name);
std::string hexdump(const void *p, unsigned len);

bool                  system_supports_dca();
std::vector<unsigned> thread_cpus();
uint32_t              thread_apic_id();

// EOF
