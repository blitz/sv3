
#include <switch.hh>
#include <cstdarg>
#include <unistd.h>

namespace Switch {

  void Switch::logf(char const *str, ...)
  {
    va_list  ap;
    va_start(ap, str);
    printf("%10s: ", "switch");
    vprintf(str, ap);
    puts("");
    va_end(ap);
  }

  void Switch::loop()
  {
    const unsigned ms       = 1000;
    logf("Main loop entered. Polling every %ums.", ms);

    _loop_running = true;
    while (true) {
      PortsList const &ports     = *_ports;     CONSIDER_MODIFIED(_ports);
      SwitchHash      &mac_cache = *_mac_table; CONSIDER_MODIFIED(_mac_table);

      for (Port *src_port : ports) {

        PacketJob *pj = src_port->poll();
        if (pj == nullptr) continue;
        logf("Polling port '%s' returned packet.", src_port->name());

        auto &ehdr     = pj->ethernet_header();
        logf("Destination %s", ehdr.dst.to_str());
        logf("Source      %s", ehdr.src.to_str());

        Port *dst_port = LIKELY(not ehdr.dst.is_multicast()) ? mac_cache[ehdr.dst] : nullptr;
        assert(dst_port != src_port and
               dst_port != &_bcast_port);

        if (not (src_port == &_bcast_port or ehdr.src.is_multicast())) {
          if (mac_cache[ehdr.src] != src_port)
            logf("MAC %s (%08x) owned by port '%s'.", ehdr.src.to_str(),
                 Ethernet::hash(ehdr.src),
                 src_port->name());

          mac_cache.add(ehdr.src, src_port);
        }

        if (UNLIKELY(!dst_port)) dst_port = &_bcast_port;
        dst_port->receive(*src_port, *pj);
      }

      MEMORY_BARRIER;
      _loop_count++;
      MEMORY_BARRIER;

      // XXX We should block here.
      usleep(1000*ms);
    }
  }

  void Switch::wait_loop_iteration()
  {
      unsigned old_loop_count = _loop_count;
      while (_loop_running and old_loop_count == _loop_running) {
        CONSIDER_MODIFIED(_loop_running);
        CONSIDER_MODIFIED(_loop_count);
        RELAX();
      }
  }

  void Switch::modify_ports(std::function<void(PortsList &)> f)
  {
    Mutex::Guard g(_ports_mtx);
    std::list<Port *> const *oldp = _ports;
    std::list<Port *>       *newp = new std::list<Port *>(*_ports);
    
    f(*newp);

    // Invalidate MAC address cache and update ports list. Wait until
    // the main loop has picked it up.
    SwitchHash *oldm = _mac_table;
    _mac_table = new SwitchHash;
    _ports     = newp;
    MEMORY_BARRIER;
    wait_loop_iteration();

    delete oldm;
    delete oldp;
  }

  void Switch::attach_port(Port &p)
  {
    size_t size;
    modify_ports([&](PortsList &ports) { ports.push_front(&p); size = ports.size(); });

    logf("Attaching port '%s'. We have %zu port%s.",
         p.name(), size, size == 1 ? "" : "s");
  }

  void Switch::detach_port(Port &p)
  {
    size_t size;
    modify_ports([&](PortsList &ports) {
        for (auto it = ports.begin(); it != ports.end(); ++it)
          if (*it == &p) {
            ports.erase(it);
            size = ports.size();

            logf("Detaching port '%s'. %zu port%s left.",
                 p.name(), size, size == 1 ? "" : "s");
            break;
          }
      });
  }
}

// EOF
