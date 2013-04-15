#pragma once

#include <list>
#include <functional>


#include <header/ethernet.hh>
#include <hash/ethernet.hh>
#include <hash/hashtable.hh>

#include <util.hh>
#include <packetjob.hh>
#include <mutex.hh>



namespace Switch {

  constexpr static unsigned MAX_MTU = 9000;

  class Switch;

  class Port : Uncopyable {
  protected:
    Switch     &_switch;
    char const *_name;

    void logf(char const *str, ...);
    
  public:
    char const *name() const { return _name; }

    virtual void receive(Port &src_port, Packet &p) = 0;
    virtual bool poll(Packet &p) = 0;

    // Call this after the instance is completely constructed.
    void    enable();

    Port(Switch &sw, char const *name);
    virtual ~Port();
  };

  /// A broadcast sink. Will do the right thing.
  class BroadcastPort : public Port {

  public:
    virtual void receive(Port &src_port, Packet &p);
    virtual bool poll(Packet &p);

    BroadcastPort(Switch &sw) : Port(sw, "broadcast") { }
  };


  typedef Hashtable<Ethernet::Address, Port *, Ethernet::hash, 1024, 1, nullptr> SwitchHash;

  class Switch : Uncopyable {
    friend class Listener;
  protected:
    typedef std::list<Port *> PortsList;

    // Signal handling
    bool             _shutdown_called;

    // Count main loop iterations for our primitive RCU scheme.
    unsigned         _loop_count;
    bool             _loop_running;

    SwitchHash      *_mac_table;
    PortsList const *_ports;

    // Serializes access to _ports and _mac_table
    Mutex            _ports_mtx;

    // Broadcast traffic goes here.
    BroadcastPort    _bcast_port;

    // Wait until one loop iteration has passed. This is a no-op, if
    // the main loop is not running.
    void wait_loop_iteration();

    // Modify the list of ports.
    void modify_ports(std::function<void(PortsList &)> f);

  public:

    std::list<Port *> const &ports() const { return *_ports; }

    void logf(char const *str, ...);

    void loop();
    void attach_port(Port &p);
    void detach_port(Port &p);

    // This function can be called from any thread or from signal
    // context to shut the switch down. It will exit from its loop()
    // method, if that is currently executing.
    void shutdown();

    Switch();
    ~Switch();
              
  };
}

// EOF
