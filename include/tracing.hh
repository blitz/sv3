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
#include <util.hh>

namespace Switch {

  enum {
    BLOCK,
    WAKEUP,
    PACKET_RX,
    PACKET_TX,
    WENT_IDLE,
    WENT_BUSY,
    IRQ,
    QUIESCENT,
  };

#ifdef TRACING

  extern char   *trace_buffer;
  extern size_t  trace_offset;

  enum { TRACE_SIZE = 1024*1024*16 };

  struct trace_entry {
    uint64_t time;
    uint8_t  event;
    uint8_t  port;
    uint16_t length;

    uint32_t _align;
  };

  static inline void
  trace(uint8_t event, uint8_t port = 0, uint16_t length = 0,
        uint64_t now = rdtsc())
  {
    trace_entry *e = reinterpret_cast<trace_entry *>(trace_buffer + trace_offset);

    e->time   = now;
    e->event  = event;
    e->port   = port;
    e->length = length;
    e->_align = 0;

    trace_offset = (trace_offset + sizeof(trace_entry)) % TRACE_SIZE;
  }

#else
  static inline void
  trace(uint8_t event, uint8_t port = 0, uint16_t length = 0,
        uint64_t now = 0)
  {
  }
#endif

}

// EOF
