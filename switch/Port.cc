
#include <switch.hh>
#include <cstdarg>

namespace Switch {

  void Port::logf(char const *str, ...)
  {
    va_list  ap;
    va_start(ap, str);
    printf("%16s: ", _name);
    vprintf(str, ap);
    puts("");
    va_end(ap);
  }

  Port::Port(Switch &sw, char const *name)
    : _switch(sw),  _name(name)
  {
    logf("Created.");
    sw.attach_port(*this);
  }

  Port::~Port()
  {
    logf("Destroyed.");
  }

}

// EOF
