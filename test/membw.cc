#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <time.h>

#include <vector>
#include <tuple>
#include <cmath>

static const size_t len = 512 * (1 << 20);

double spectodouble(struct timespec tp)
{
  return (double)tp.tv_sec * 1000000000.0 + tp.tv_nsec;
}

typedef std::tuple<double, double> result;


template <class NUM, class VECTOR>
std::tuple<NUM, NUM> average(VECTOR v)
{
  NUM sum   = 0;
  NUM sqsum = 0;

  for (auto x : v) sum += x;

  NUM average = sum / v.size();

  for (auto x : v) {
    NUM s = x - average;
    sqsum += s*s;
  }

  return std::tuple<NUM, NUM>(average, sqrt(sqsum/(v.size() - 1)));
}

template <class T>
result bw_test(T closure)
{
  std::vector<double> res;

  struct timespec start, end;

  for (unsigned i = 0; i < 100; i++) {
    clock_gettime(CLOCK_MONOTONIC, &start);
    asm volatile ("" ::: "memory");
    closure();
    asm volatile ("" ::: "memory");
    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed = (double)(end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);

    res.push_back((double)(len * 8000000000) / elapsed /* Bit/s */);
  }

  return average<double>(res);

}


int main()
{

  void  *buf_from = malloc(len);
  void  *buf_to   = malloc(len);


  result mset = bw_test([&] () { memset(buf_to, 0, len); });
  result mcpy = bw_test([&] () { memcpy(buf_to, buf_from, len); });

  result stos = bw_test([&] () { size_t l = len;
				 void *t = buf_to;
				 asm volatile ("rep stosb" : "+D" (t), "+c"(l) : "a" (0) : "memory"); });

  result movs = bw_test([&] () { size_t l = len;
				 void *f = buf_from;
				 void *t = buf_to;
				 asm volatile ("rep movsb" : "+D" (t), "+S" (f), "+c"(l) :: "memory"); });

  printf("memset:    %.02lf+-%.02lf GBit/s\n", std::get<0>(mset) / 1000000000, std::get<1>(mset) / 1000000000);
  printf("memcpy:    %.02lf+-%.02lf GBit/s\n", std::get<0>(mcpy) / 1000000000, std::get<1>(mcpy) / 1000000000);
  printf("rep stosb: %.02lf+-%.02lf GBit/s\n", std::get<0>(stos) / 1000000000, std::get<1>(stos) / 1000000000);
  printf("rep movsb: %.02lf+-%.02lf GBit/s\n", std::get<0>(movs) / 1000000000, std::get<1>(movs) / 1000000000);

  return 0;
}
