
#include <hash/ethernet.hh>

#include <cstdlib>
#include <cstdio>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <set>

using namespace Ethernet;
using std::set;

int main()
{
  constexpr unsigned tries = 1024*1024;
  set<uint32_t> hashset;
  set<uint32_t> hashrset;

  int       devr = open("/dev/urandom", O_RDONLY);

  if (devr < 0) {
    perror("open");
    return EXIT_FAILURE;
  }

  for (unsigned i = 0; i < tries; i++) {
    Address a;
    if (read(devr, a.byte, sizeof(a.byte)) != sizeof(a.byte)) {
      perror("read");
      return EXIT_FAILURE;
    }
    uint32_t h = hash(a);
    hashset.insert(h);
    hashrset.insert(h % (2 << 16));
  }

  printf("full    %2.4lf\n", 1 - double(hashset.size())/tries); 
  printf("reduced %2.4lf\n", 1 - double(hashrset.size())/tries); 

  return 0;
}
