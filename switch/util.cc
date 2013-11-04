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

#include <util.hh>
#include <cxxabi.h>
#include <sched.h>
#include <unistd.h>
#include <sstream>
#include <boost/format.hpp>

std::vector<std::string> string_split(std::string const &str, char delimiter)
{
  std::stringstream         ss(str);
  std::vector<std::string>  res;

  for (std::string item; std::getline(ss, item, delimiter); )
    res.push_back(item);

  return res;
}

std::string demangle(const char *name)
{
  char   buf[1024];
  size_t size = sizeof(buf);
  int    status;
  return abi::__cxa_demangle (name, buf, &size, &status);
}

std::string hexdump(const void *p, unsigned len)
{
  const unsigned     chars_per_row = 16;
  const char        *data = reinterpret_cast<const char *>(p);
  const char        *data_end = data + len;
  std::stringstream  ss;

  for (unsigned cur = 0; cur < len; cur += chars_per_row) {
    ss << boost::format("%08x") % cur;
    for (unsigned i = 0; i < chars_per_row; i++)
      if (data+i < data_end)
	ss << boost::format(" %02x") % (int)reinterpret_cast<const uint8_t *>(data)[i];
      else
	ss << "   ";
    ss << " | ";
    for (unsigned i = 0; i < chars_per_row; i++) {
      if (data < data_end)
	ss << boost::format("%c") % (((data[0] >= 32) && (data[0] > 0)) ? data[0] : '.');
      else
	ss << " ";
      data++;
    }
    ss << std::endl;
  }

  return ss.str();
}

std::vector<unsigned> thread_cpus()
{
  std::vector<unsigned> cpu_list;
  cpu_set_t *cpuset = CPU_ALLOC(128);

  sched_getaffinity(getpid(), CPU_ALLOC_SIZE(128), cpuset);
  for (unsigned i = 0; i < 128; i++)
    if (CPU_ISSET(i, cpuset))
      cpu_list.push_back(i);

  CPU_FREE(cpuset);
  return cpu_list;
}

uint32_t thread_apic_id()
{
  uint32_t eax = 1;
  uint32_t ebx;

  asm ("cpuid" : "+a" (eax), "=b" (ebx) :: "ecx", "edx");

  return ebx >> 24;
}

// EOF
