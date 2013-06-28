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

    void logf(char const *str, ...);

  public:
    std::string const name() const { return _name; }

    virtual void receive(Port &src_port, Packet &p) = 0;
    virtual bool poll(Packet &p) = 0;

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
    Port &_port;
  public:
    Port &port() const { return _port; }
    PortBrokenException(Port &port) : _port(port) { }
  };

  class Switch final : protected rcu_head, Uncopyable {
    friend class Listener;
  protected:
    typedef std::list<Port *> PortsList;

    // Blocking
    int              _event_fd;

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
		      SwitchHash      &mac_cache);

  public:

    /// We expose this to allow ports to map this file descriptor
    /// outside of the switch process.
    int event_fd() const { return _event_fd; }

    std::list<Port *> const &ports() const { return *_ports; }

    void logf(char const *str, ...);

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

    Switch();
    ~Switch();

  };
}

// EOF
