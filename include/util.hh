#pragma once

#include <cstdint>
#include <x86intrin.h>

static inline uint64_t rdtsc() { return __rdtsc(); }

// EOF
