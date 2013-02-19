
#include <switch.hh>
#include <hash/ethernet.hh>

#include <cstdio>

int main()
{
  Switch::Switch sv3;
  Ethernet::Address a;

  printf("%08x\n", Ethernet::hash(a));

  return 0;
}

// EOF
