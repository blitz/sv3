/* eventfd benchmark */

#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <cstdio>
#include <cstdint>
#include <cinttypes>
#include <cstdlib>


#include <thread>

using namespace std;

static void pin(pthread_t thread, unsigned cpu)
{
  cpu_set_t      set;

  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  if (0 != pthread_setaffinity_np(thread, sizeof(set), &set))
    abort();
}

static void eventfd_signal(int fd)
{
  uint64_t val = 1;
  int r = write(fd, &val, sizeof(val));
  if (r != sizeof(val)) abort();
}

static void eventfd_wait(int fd)
{
  uint64_t val;
  int r = read(fd, &val, sizeof(val));
  if (r != sizeof(val)) abort();
}

static void ping(int fd1, int fd2, unsigned rounds, uint64_t *results)
{
  //pin(pthread_self(), 2);

  // Synchronize
  eventfd_signal(fd1);
  eventfd_wait(fd2);


  for (int i = rounds; i > 0; i--) {
    uint64_t start = __builtin_ia32_rdtsc();
    eventfd_signal(fd1);
    eventfd_wait(fd2);
    uint64_t end = __builtin_ia32_rdtsc();

    *(results++) = end - start;
  }
}

static void pong(int fd1, int fd2, unsigned rounds)
{
  //pin(pthread_self(), 4);

  // Synchronize
  eventfd_wait(fd1);
  eventfd_signal(fd2);

  for (int i = rounds; i > 0; i--) {
    eventfd_wait(fd1);
    eventfd_signal(fd2);
  }
}


int main()
{
  const unsigned repeat = 1024*1024;
  static uint64_t results[repeat];

  int eventfd1 = eventfd(0, 0);
  int eventfd2 = eventfd(0, 0);
  
  thread ping_thread(&ping, eventfd1, eventfd2, repeat, results);
  thread pong_thread(&pong, eventfd1, eventfd2, repeat);

  ping_thread.join();
  pong_thread.join();

  // for (uint64_t r : results)
  //   printf("%" PRIu64 "\n", r);

  uint64_t sum = 0;
  for (uint64_t r : results)
    sum += r;

  printf("Pingpong: %llu cycles\n", sum/repeat);

  return 0;
}
