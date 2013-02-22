#pragma once

#include <header/ethernet.hh>
#include <hash/ethernet.hh>
#include <hash/hashtable.hh>
#include <util.hh>
#include <packetjob.hh>

#include <list>


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

    virtual void       receive(Port &src_port, PacketJob const &pj) = 0;
    virtual PacketJob *poll() = 0;

    Port(Switch &sw, char const *name);
    virtual ~Port();
  };

  /// A broadcast sink. Will do the right thing.
  class BroadcastPort : public Port {

  public:
    virtual void       receive(Port &src_port, PacketJob const &pj);
    virtual PacketJob *poll   ();

    BroadcastPort(Switch &sw) : Port(sw, "broadcast") { }
  };


  typedef Hashtable<Ethernet::Address, Port *, Ethernet::hash, 1024, 1, nullptr> SwitchHash;

  class Switch : Uncopyable {
  protected:
    SwitchHash        _mac_table;
    std::list<Port *> _ports;
    BroadcastPort     _bcast_port;

    void logf(char const *str, ...);

  public:

    std::list<Port *> const ports() const { return _ports; }

    void loop();
    void attach_port(Port &p);

    Switch()
      : _mac_table(), _ports(), _bcast_port(*this)
    {
    }
              
  };
}

// EOF
