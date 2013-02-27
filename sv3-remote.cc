// 

#include <listener.hh>
#include <util.hh>
#include <cstdio>
#include <unistd.h>

using namespace Switch;

int main(int argc, char **argv)
{
  int         fd;
  sockaddr_un sa;

  if (argc < 3) goto usage;

  fd = socket(AF_LOCAL, SOCK_SEQPACKET, 0);
  if (fd < 0) { perror("socket"); return EXIT_FAILURE; }

  sa.sun_family = AF_LOCAL;
  strncpy(sa.sun_path, argv[1], sizeof(sa.sun_path));
  if (0 > connect(fd, reinterpret_cast<sockaddr *>(&sa), sizeof(sa))) {
    perror("connect");
    return EXIT_FAILURE;
  }


  if (strcmp(argv[2], "pingpong") == 0) {
    ClientRequest  req;
    req.type = ClientRequest::PING;

    const unsigned rounds = 1024;
    uint64_t start = rdtsc();
    for (unsigned i = 0; i < rounds; i++)
      Listener::call(fd, req);
    uint64_t end = rdtsc();

    printf("%lu cycles per roundtrip\n", (end - start) / rounds);

    return 0;
  }

  if (strcmp(argv[2], "tap") == 0) {
    if (argc != 4) {
      fprintf(stderr, "tap needs a device node as parameter.\n");
      return EXIT_FAILURE;
    }

    ClientRequest req;
    req.type = ClientRequest::CREATE_PORT_TAP;

    if (sizeof(req.create_port_tap.buf) < strlen(argv[3])) {
      fprintf(stderr, "Your filename is too long.\n");
      return EXIT_FAILURE;
    }

    strncpy(req.create_port_tap.buf, argv[3], sizeof(req.create_port_tap.buf));

    ServerResponse resp = Listener::call(fd, req);
    printf("%s\n", resp.status.success ? "Success" : "Failure");

    return 0;
  }

  if (strcmp(argv[2], "memmap") == 0) {
    char templ[] = "/tmp/sv3-shmem-XXXXXX";
    int tfd = mkstemp(templ);
    if (tfd < 0)                                 { perror("mkstemp"); return EXIT_FAILURE; }
    if ((off_t)-1 == lseek(tfd, 4096, SEEK_SET)) { perror("lseek"  ); return EXIT_FAILURE; }

    ClientRequest req;
    req.type = ClientRequest::MEMORY_MAP;
    req.memory_map.addr   = 0;
    req.memory_map.size   = 4096;
    req.memory_map.fd     = tfd;
    req.memory_map.offset = 0;

    ServerResponse resp = Listener::call(fd, req);
    printf("%s\n", resp.status.success ? "Success" : "Failure");

    return 0;
  }

 usage:
  fprintf(stderr, "Usage: %s switch-socket pingpong|tap ...\n", argv[0]);
  return EXIT_FAILURE;
}
