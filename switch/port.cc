
#include <switch.hh>
#include <cstdarg>

namespace Switch {

  void Port::logf(char const *str, ...)
  {
    va_list  ap;
    va_start(ap, str);
    printf("%10s: ", _name);
    vprintf(str, ap);
    puts("");
    va_end(ap);
  }

  void Port::enable()
  {
    _switch.attach_port(*this);
  }

  Port::Port(Switch &sw, char const *name)
    : _switch(sw),  _name(strdup(name))
  {
  }

  Port::~Port()
  {
    _switch.detach_port(*this);
    delete _name;
  }

}

// EOF
