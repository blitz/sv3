#pragma once

#include <header/ethernet.hh>
#include <hash/ethernet.hh>
#include <hash/hashtable.hh>
#include <util.hh>

#include <list>
#include <functional>

namespace Switch {

  class Switch;

  class Packet {

  };

  class PacketJob {

  public:
    virtual void do_packets(std::function<void(Packet &)> f);
    virtual ~PacketJob() { }
  };

  class Port : Uncopyable {
    Switch     &_switch;
    char const *_name;

  protected:
    void logf(char const *str, ...);
    
  public:
    char const *name() const { return _name; }

    virtual void receive(PacketJob &pj) = 0;

    Port(Switch &sw, char const *name);
    virtual ~Port();
  };

  /// A broadcast sink. Will do the right thing.
  class BroadcastPort : public Port {

  public:
    virtual void receive(PacketJob &pj);
    BroadcastPort(Switch &sw) : Port(sw, "broadcast") { }
  };

  class Switch : Uncopyable {
    Hashtable<Ethernet::Address, Port, Ethernet::hash, 1024> _mac_table;

    std::list<Port *> _ports;
    BroadcastPort     _bcast_port;

    void logf(char const *str, ...);

  public:

    void loop();
    void attach_port(Port &p);

    Switch()
      : _mac_table(), _ports(), _bcast_port(*this)
    {
    }
              
  };
}

// EOF
