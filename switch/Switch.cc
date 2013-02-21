
#include <switch.hh>
#include <cstdarg>
#include <unistd.h>

namespace Switch {

  void Switch::logf(char const *str, ...)
  {
    va_list  ap;
    va_start(ap, str);
    printf("%16s: ", "switch");
    vprintf(str, ap);
    puts("");
    va_end(ap);
  }

  void Switch::loop()
  {
    const unsigned ms = 100;
    logf("Main loop entered. Polling every %ums.", ms);

    while (true) {


      // XXX We should block here.
      usleep(1000*ms);
    }
  }

  void Switch::attach_port(Port &p)
  {
    if (&p == &_bcast_port) {
      logf("Ignoring attach request from broadcast port.");
    } else {
      _ports.push_front(&p);
      logf("Attaching Port '%s'. We have %u port%s.",
           p.name(), _ports.size(), _ports.size() == 1 ? "" : "s");
    }
  }
}

// EOF
