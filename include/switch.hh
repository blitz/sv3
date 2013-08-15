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

#include <urcu-qsbr.h>

#include <list>
#include <vector>
#include <functional>
#include <mutex>
#include <string>
#include <atomic>

#include <header/ethernet.hh>
#include <hash/ethernet.hh>
#include <hash/hashtable.hh>

#include <util.hh>
#include <packetjob.hh>

namespace Switch {

  class Switch;

  class Port : Uncopyable {
  protected:
    Switch     &_switch;
    std::string _name;

  public:
    std::string const name() const { return _name; }

    /// Log a message.
    void logf(std::string str, ...);

    /// Receive a packet.
    virtual void receive(Packet &p) = 0;

    /// Poll for packets. This method has to enable or disable
    /// notifications according to the given parameter, BEFORE polling
    /// the guest. Returns true, if p has been populated.
    virtual bool poll(Packet &p, bool enable_notifications) = 0;

    /// This method is called on a packet returned by poll when the
    /// switch doesn't need to access packet data anymore.
    virtual void mark_done(Packet &p) = 0;

    /// Check whether interrupts are pending and deliver them.
    virtual void poll_irq() = 0;

    /// Call this after the instance is completely constructed and
    /// ready to receive packets.
    virtual void enable();

    /// Call this if the port should not be polled any more. Waits
    /// until the switch loop doesn't see the port anymore, i.e. calls
    /// to poll() and receive() have finished.
    virtual void disable();

    Port(Switch &sw, std::string name);
    virtual ~Port();
  };

  typedef Hashtable<Ethernet::Address, Port *, Ethernet::hash, 1024, 1, nullptr> SwitchHash;

  /// A port behaved strangely and needs to be removed. Can be thrown
  /// in the dynamic extent of Switch::loop().
  class PortBrokenException {
    Port       &_port;
    char const *_reason;
  public:

    Port       &port()   const { return _port; }
    char const *reason() const { return _reason; }

    PortBrokenException(Port &port, char const *reason = "???")
      : _port(port), _reason(reason) { }
  };

  class Switch final : protected rcu_head, Uncopyable {
    friend class Listener;
  protected:
    typedef std::list<Port *> PortsList;

    /// The event fd the switch main loop uses to block, when idle.
    int              _event_fd;

    /// How many microseconds to poll, before blocking when idle.
    const unsigned   _poll_us;

    /// How many packets to switch from a single port in one batch.
    const unsigned   _batch_size;

    // Signal handling
    std::atomic<bool> _shutdown_called;

    // RCU
    std::vector<std::function<void()>> _pending_free;
    std::mutex                         _pending_free_mtx;

    static void         cb_free_pending(struct rcu_head *);
    void                free_pending(); // called via RCU

    SwitchHash      *_mac_table;
    PortsList const *_ports;

    // Serializes access to _ports and _mac_table
    std::mutex       _ports_mtx;

    // Modify the list of ports.
    void modify_ports(std::function<void(PortsList &)> f);


    bool work_quantum(PortsList const &ports,
		      SwitchHash      &mac_cache,
		      bool             enabled_notifications);

  public:

    /// We expose this to allow ports to map this file descriptor
    /// outside of the switch process.
    int event_fd() const { return _event_fd; }

    std::list<Port *> const &ports() const { return *_ports; }

    void logf(std::string str, ...);

    void loop();
    void attach_port(Port &p);
    void detach_port(Port &p);

    // This function can be called from any thread or from signal
    // context to shut the switch down. It will exit from its loop()
    // method, if that is currently executing.
    void shutdown();

    /// Has the shutdown been initiated?
    /// XXX Can we get by with mo_relaxed here?
    bool should_shutdown()
    { return _shutdown_called.load(std::memory_order_relaxed); }

    // Wake up the polling thread and have it poll all ports.
    void schedule_poll();

    explicit Switch(unsigned poll_us, unsigned batch_size);
    ~Switch();

  };
}

// EOF
