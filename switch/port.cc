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


#include <switch.hh>
#include <cstdarg>

namespace Switch {

  void Port::logf(char const *str, ...)
  {
    va_list  ap;
    va_start(ap, str);
    printf("%10s: ", _name.c_str());
    vprintf(str, ap);
    puts("");
    va_end(ap);
  }

  void Port::enable()
  {
    _switch.attach_port(*this);
  }

  void Port::disable()
  {
    _switch.detach_port(*this);
    synchronize_rcu();
  }

  Port::Port(Switch &sw, std::string name)
    : _switch(sw),  _name(name)
  {
  }

  Port::~Port()
  {
    disable();
  }

}

// EOF
