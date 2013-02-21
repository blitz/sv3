
#include <switch.hh>
#include <hash/ethernet.hh>

#include <cstdio>

class TestPort : public Switch::Port {

public:
  void receive(Switch::PacketJob &pj)
  {
    logf("Receiving packets into bit bucket.");
  }

  // GCC 4.8: using Port::Port;
  TestPort(Switch::Switch &sw, char const *name) : Port(sw, name) {}
};


int main()
{
  Switch::Switch sv3;
  TestPort       tport(sv3, "test");

  sv3.loop();

  return 0;
}

// EOF
