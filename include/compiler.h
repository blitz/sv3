/** @file
 * Compiler-specific annotations
 *
 * Copyright (C) 2010-2011, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Vancouver.
 *
 * Vancouver is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vancouver is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#pragma once

#if !defined(__GNUC__)
#error Your platform is not supported.
#endif

#define REGPARM(x) __attribute__((regparm(x)))
#define NORETURN   __attribute__((noreturn))
#define PURE       __attribute__((pure))
#define COLD       __attribute__((cold))
#define ALIGNED(x) __attribute__((aligned(x)))
#define PACKED     __attribute__((packed))
#define MEMORY_BARRIER __asm__ __volatile__ ("" ::: "memory")
#define RESTRICT   __restrict__
#define UNUSED     __attribute__((unused))

#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

/* EOF */
