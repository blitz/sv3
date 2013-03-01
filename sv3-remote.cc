// 

#include <listener.hh>
#include <util.hh>
#include <cstdio>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/eventfd.h>

using namespace Switch;

int main(int argc, char **argv)
{
  int         fd;

  if (argc < 3) goto usage;

  fd = sv3_connect(argv[1]);

  if (strcmp(argv[2], "pingpong") == 0) {
    Sv3Request  req;
    req.type = SV3_REQ_PING;

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

    Sv3Request req;
    req.type = SV3_REQ_CREATE_PORT_TAP;

    if (sizeof(req.create_port_tap.buf) < strlen(argv[3])) {
      fprintf(stderr, "Your filename is too long.\n");
      return EXIT_FAILURE;
    }

    strncpy(req.create_port_tap.buf, argv[3], sizeof(req.create_port_tap.buf));

    Sv3Response resp = Listener::call(fd, req);
    printf("%s\n", resp.status.success ? "Success" : "Failure");

    return 0;
  }

  if (strcmp(argv[2], "memmap") == 0) {
    Sv3Request  req;
    Sv3Response resp;

    const size_t mlen = 4 << 20;
    int tfd = sv3_memory(mlen);
    if (tfd < 0) { perror("tempfile"); return EXIT_FAILURE; }
    void *m = mmap(nullptr, mlen, PROT_READ | PROT_WRITE, MAP_SHARED, tfd, 0);
    if (m == MAP_FAILED) { perror("mmap"); return EXIT_FAILURE; }

    int efd = sv3_call_eventfd(fd, 0);
    if (efd < 0) { perror("eventfd"); return EXIT_FAILURE; }

    req.type = SV3_REQ_MEMORY_MAP;
    req.memory_map.addr   = reinterpret_cast<uintptr_t>(m);
    req.memory_map.size   = 256 << 20;
    req.memory_map.fd     = tfd;
    req.memory_map.offset = 0;

    resp = Listener::call(fd, req);
    printf("map: %s\n", resp.status.success ? "Success" : "Failure");
    if (not resp.status.success) return EXIT_FAILURE;

    Sv3QueuePair *qp   = reinterpret_cast<Sv3QueuePair *>(m);
    uint8_t      *pmem = reinterpret_cast<uint8_t *>(m) + sizeof(Sv3QueuePair);

    req.type = SV3_REQ_CREATE_PORT_QP;
    req.create_port_qp.qp = reinterpret_cast<uintptr_t>(qp);
    resp = Listener::call(fd, req);
    printf("reg: %s\n",  resp.status.success ? "Success" : "Failure");

    Sv3Desc d;
    d.buf_ptr = reinterpret_cast<uintptr_t>(pmem);
    d.len     = 2048;
    sv3_queue_enqueue(&qp->rx, &d);

    do {
      while (sv3_queue_dequeue(&qp->done, &d)) {
        printf("done: tail %x head %x\n", qp->done.tail, qp->done.head);
        printf("rx:   tail %x head %x\n", qp->rx.tail, qp->rx.head);
        printf("Got packet %u.\n", d.len);

        d.len = 2048;
        sv3_queue_enqueue(&qp->rx, &d);
      }

      qp->blocked = 1;
      asm ("" ::: "memory");
      uint64_t d;
      if (sizeof(d) != read(efd, &d, sizeof(d))) {
        perror("read");
        return EXIT_FAILURE;
      }
    } while (true);

    return 0;
  }

 usage:
  fprintf(stderr, "Usage: %s switch-socket pingpong|tap ...\n", argv[0]);
  return EXIT_FAILURE;
}
