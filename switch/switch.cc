
#include <switch.hh>
#include <cstdarg>
#include <unistd.h>
#include <sys/eventfd.h>

#include <timer.hh>

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
    _shutdown_called.store(true, std::memory_order_seq_cst);
    uint64_t val = 1;
    write(_event_fd, &val, sizeof(val));
  }


  /// Switch a couple of packets. Returns false if we were idle.
  bool Switch::work_quantum(PortsList const &ports,
			    SwitchHash      &mac_cache,
			    bool enabled_notifications)
  {
    const unsigned packets_per_port = 16;
    bool work_done                  = false;

    for (Port *src_port : ports) { // Packet switching loop

      for (unsigned quota = packets_per_port; quota > 0; quota--) {
	Packet p(src_port);

	if (not src_port->poll(p, enabled_notifications)) break;
	// logf("Polling port '%s' returned %u byte packet.", src_port->name().c_str(),
	//      p.packet_length);

	auto &ehdr = p.ethernet_header();
	// logf("Destination %s", ehdr.dst.to_str());
	// logf("Source      %s", ehdr.src.to_str());

	Port *dst_port = LIKELY(not ehdr.dst.is_multicast()) ? mac_cache[ehdr.dst] : nullptr;
	assert(dst_port != src_port);

	if (LIKELY(not ehdr.src.is_multicast())) {
	  if (UNLIKELY(mac_cache[ehdr.src] != src_port))
	    logf("MAC %s (%08x) owned by port '%s'.", ehdr.src.to_str(),
	         Ethernet::hash(ehdr.src),
	         src_port->name().c_str());

	  mac_cache.add(ehdr.src, src_port);
	}

	if (LIKELY(dst_port)) {
	  dst_port->receive(p);
	} else {
	  for (Port *dst_port : ports)
	    if (dst_port != src_port)
	      dst_port->receive(p);
	}

	// XXX We need to call this even when dst_port->deliver()
	// throws an exception! Otherwise, src_port is stuck.
	src_port->mark_done(p);

	work_done = true;
      }
    }

    // Deliver interrupts.
    for (Port *port : ports) port->poll_irq();

    return work_done;
  }

  void Switch::loop()
  {

    const unsigned idle_freq    = 10000;
    const unsigned idle_timeout = 5 /* in 10 us */;
    Timer          idle_timer(idle_freq);

    logf("Main loop entered. Idle poll time is 50us.");

    do {			// Main loop
      enum {
	WORK,
	IDLE,
	NOTIFICATION_ENABLE,
      } state = WORK;
      
      while (LIKELY(not should_shutdown())) { // RCU Loop
	bool work_done = false;
	rcu_quiescent_state();

	try {
	  // Casting madness... Otherwise this won't compile.
	  PortsList      **pports    =  const_cast<PortsList **>(&_ports);
	  PortsList const &ports     = *rcu_dereference(*pports);
	  SwitchHash      &mac_cache = *rcu_dereference(_mac_table);

	  work_done = work_quantum(ports, mac_cache, state == NOTIFICATION_ENABLE);
	} catch (PortBrokenException &e) {
	  detach_port(e.port());
	  work_done = true;
	}

	// We have seen action.
	if (LIKELY(work_done)) {
	  state = WORK;
	  continue;
	}
	
	switch (state) {
	case WORK:
	  // The switch was idle for the first time. Start the idle
	  // clock.
	  assert(not work_done);
	  state = IDLE;
	  idle_timer.arm();
	  break;
	case IDLE:
	  if (idle_timer.elapsed(idle_timeout)) {
	    // We have been idle for the maximum idle
	    // duration. Reenable notifications so we can block.
	    state = NOTIFICATION_ENABLE;
	  }
	  continue;
	case NOTIFICATION_ENABLE:
	  // We still haven't seen action and notifications are
	  // already enabled. Time to block.
	  goto block;
	}
      }
    block:

      // Block
      rcu_thread_offline();
      {
	uint64_t val;
	int r = read(_event_fd, &val, sizeof(val));
	if (r != sizeof(val))
	  break;
      }
      rcu_thread_online();


    } while (not should_shutdown());

    logf("Main loop returned.");
  }

  void Switch::cb_free_pending(struct rcu_head *head)
  {
    Switch *sw = static_cast<Switch *>(head);
    sw->free_pending();
  }

  void Switch::free_pending()
  {
    std::vector<std::function<void()>> pending;
    {
      std::lock_guard<std::mutex> lock(_pending_free_mtx);
      pending.swap(_pending_free);
    }

    for (std::function<void()> &p : pending) p();
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

    // Delete MAC address cache. No problem to race here.
    oldm = rcu_xchg_pointer(&_mac_table, newm);

    {
      std::lock_guard<std::mutex> lock(_pending_free_mtx);
      _pending_free.push_back([=] () {
	  delete oldm;
	  delete oldp;
	});
    }

    call_rcu(this, cb_free_pending);
  }

  void Switch::attach_port(Port &p)
  {
    size_t size;
    modify_ports([&](PortsList &ports) { ports.push_front(&p); size = ports.size(); });

    logf("Attaching port '%s'. We have %zu port%s.",
	 p.name().c_str(), size, size == 1 ? "" : "s");
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
		 p.name().c_str(), size, size == 1 ? "" : "s");
	    break;
	  }
      });
  }

  void Switch::schedule_poll()
  {
    uint64_t v = 1;
    write(_event_fd, &v, sizeof(v));
  }

  Switch::Switch()
    : _shutdown_called(false),
      _mac_table(new SwitchHash), _ports(new PortsList),
      _ports_mtx()
  {
    _event_fd = eventfd(0, 0);
  }

  Switch::~Switch()
  {
    // Don't need to do anything here for now.
    logf("Switch destroyed.");
  }

}

// EOF
