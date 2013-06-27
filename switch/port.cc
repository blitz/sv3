
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
