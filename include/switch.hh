#pragma once

#include <header/ethernet.hh>
#include <hash/ethernet.hh>
#include <hash/hashtable.hh>

namespace Switch {

  class Port {
  };

  class Switch {
    Hashtable<Ethernet::Address, Port, Ethernet::hash, 1024> _mac_table;

  public:

    Switch() : _mac_table() { }
              
  };
}
