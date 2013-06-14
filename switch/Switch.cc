
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

  void Switch::shutdown()
  {
    _shutdown_called = true;
  }

  void Switch::loop()
  {
    const unsigned ms = 10;
    logf("Main loop entered. Polling every %ums.", ms);

    while (not _shutdown_called) {
      CONSIDER_MODIFIED(_shutdown_called);

      PortsList const &ports     = *_ports;     CONSIDER_MODIFIED(_ports);
      SwitchHash      &mac_cache = *_mac_table; CONSIDER_MODIFIED(_mac_table);

      for (Port *src_port : ports) {

        Packet p;
        if (not src_port->poll(p)) continue;
        // logf("Polling port '%s' returned %u byte packet.", src_port->name(),
        //      p.packet_length);

        auto &ehdr = p.ethernet_header();
        // logf("Destination %s", ehdr.dst.to_str());
        // logf("Source      %s", ehdr.src.to_str());

        Port *dst_port = LIKELY(not ehdr.dst.is_multicast()) ? mac_cache[ehdr.dst] : nullptr;
        assert(dst_port != src_port and
               dst_port != &_bcast_port);

        if (LIKELY(not (src_port == &_bcast_port or ehdr.src.is_multicast()))) {
          // if (mac_cache[ehdr.src] != src_port)
          //   logf("MAC %s (%08x) owned by port '%s'.", ehdr.src.to_str(),
          //        Ethernet::hash(ehdr.src),
          //        src_port->name());

          mac_cache.add(ehdr.src, src_port);
        }

        if (UNLIKELY(!dst_port)) dst_port = &_bcast_port;
        dst_port->receive(*src_port, p);

        p.callback(p);
      }

      rcu_quiescent_state_qsbr();

      // XXX We should block here.
      usleep(1000*ms);
    }

    logf("main loop returned.");
  }

  void Switch::cb_free_pending(struct rcu_head *head)
  {
    Switch *sw = static_cast<Switch *>(head);
    sw->free_pending();
  }

  void Switch::free_pending()
  {
    std::vector<void *> pending;
    {
      std::lock_guard<std::mutex> lock(_pending_free_mtx);
      pending.swap(_pending_free);
    }

    for (void *p : pending) free(p);
  }

  void Switch::modify_ports(std::function<void(PortsList &)> f)
  {
    std::list<Port *> const *oldp;
    std::list<Port *>       *newp = new std::list<Port *>(*_ports);
    SwitchHash              *oldm;
    SwitchHash              *newm = new SwitchHash;

    // Update ports list. Use a mutex to not race with other calls to
    // this function.
    {
      std::lock_guard<std::mutex> lock(_ports_mtx);
      oldp   = _ports;
      f(*newp);
      _ports = newp;
    }

    // Delete MAC address cache.
    oldm = rcu_xchg_pointer(&_mac_table, newm);

    {
      std::lock_guard<std::mutex> lock(_pending_free_mtx);
      _pending_free.push_back(oldm);
      _pending_free.push_back((void *)oldp);
    }

    call_rcu(static_cast<struct rcu_head *>(this), cb_free_pending);
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

  Switch::Switch()
    : _shutdown_called(false),
      _mac_table(new SwitchHash), _ports(new PortsList),
      _ports_mtx(),
      _bcast_port(*this)
  {
    _bcast_port.enable();
  }

  Switch::~Switch()
  {
    // Don't need to do anything here for now.
  }

}

// EOF
