
#include <cstdio>
#include <algorithm>


#include <hash/ethernet.hh>
#include <switch.hh>
#include <listener.hh>
#include <tapport.hh>

int main()
{
  Switch::Switch   sv3;
  Switch::Listener listener(sv3);

  // Switch::TapPort egress(sv3, "/dev/tap4");

  sv3.loop();

  return 0;
}

// EOF
