// -*- Mode: C++ -*-

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <hash/onescomplement.hh>
#include <util.hh>

using namespace OnesComplement;

static uint8_t p0[] = { 0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF,
};

static uint8_t p1[] = { 0x00, 0x01, 0x00, 0x01,
                        0x00, 0x01, 0x00, 0x01,
                        0x00, 0x01, 0x00, 0x01,
                        0x00, 0x01, 0x00, 0x01,
                        0x00, 0x01, 0x00, 0x01,
                        0x00, 0x01, 0x00, 0x01,
                        0x00, 0x01, 0x00, 0x01,
                        0x00, 0x01, 0x00, 0x01,
                        0x00, 0x01, 0x00, 0x01,
};

static uint8_t p2[] = { 0x45, 0x00, 0x00, 0x73,
                        0x00, 0x00, 0x40, 0x00,
                        0x40, 0x11, 0xb8, 0x61,
                        0xc0, 0xa8, 0x00, 0x01,
                        0xc0, 0xa8, 0x00, 0xc7,

};

static struct {
  uint8_t *data;
  size_t   length;
  uint16_t checksum;
} data[] = {
  { p0, sizeof(p0), 0xFFFF },
  { p1, sizeof(p1), 0x1200 },
  { p2, sizeof(p1), 0xFFFF },
};

static unsigned long (*checksum_fun[])(uint8_t const *, size_t, bool &) = {
  checksum_adc,
  checksum_sse,
};

static unsigned long (*move_fun[])(uint8_t const *, uint8_t *, size_t, bool &) = {
  checksum_move_adc,
  checksum_move_sse,
};

int main()
{
  int ret = EXIT_SUCCESS;

  // Correctness
  for (unsigned f = 0; f < sizeof(checksum_fun)/sizeof(checksum_fun[0]); f++)
    for (unsigned i = 0; i < sizeof(data)/sizeof(data[0]); i++) {
      for (unsigned start = 0; start <= data[i].length; start++) {
        bool odd        = false;
        unsigned long first  = checksum_fun[f](data[i].data, start, odd);
        unsigned long second = checksum_fun[f](data[i].data + start, data[i].length - start, odd);
        uint16_t checksum = fold(add(first, second));

        if (checksum != data[i].checksum) {
          printf("CHECKSUM FAIL [%02u:%02u:%04u] %08lx + %08lx = %04x : %04x\n",
                 f, i, start, first, second, checksum, data[i].checksum);
          ret = EXIT_FAILURE;
        }
      }
    }

  // Performance
  const size_t buf_len = 16 << 20;
  uint8_t *buf = new uint8_t[buf_len];
  uint8_t *dst = new uint8_t[buf_len];
  uint32_t lchecksum = ~0L;

  //  ... Checksumming
  for (unsigned f = 0; f < sizeof(checksum_fun)/sizeof(checksum_fun[0]); f++) {
    memset(buf, 0xFE, buf_len);

    bool     odd   = false;
    uint64_t start = rdtsc();
    uint16_t checksum = fold(checksum_fun[f](buf, buf_len, odd));
    uint64_t end   = rdtsc();

    printf("f%02u:      checksum %3.2f bytes/cycle: %04x\n", f, static_cast<float>(buf_len) / (end - start), checksum);
    if (~lchecksum == 0) lchecksum = checksum;

    if (lchecksum != checksum) {
      printf("f%02u: checksum fail!\n", f);
      ret = EXIT_FAILURE;
    }
  }

  //  ... Move
  lchecksum = ~0L;
  for (unsigned f = 0; f < sizeof(move_fun)/sizeof(move_fun[0]); f++) {
    memset(buf, 0xFE, buf_len);
    memset(dst, 0xFE, buf_len);

    bool     odd   = false;
    uint64_t start = rdtsc();
    uint16_t checksum = fold(move_fun[f](buf, dst, buf_len, odd));
    uint64_t end   = rdtsc();

    assert(memcmp(dst, buf, buf_len) == 0);
    printf("f%02u:          move %3.2f bytes/cycle: %04x\n", f, static_cast<float>(buf_len) / (end - start), checksum);
    if (~lchecksum == 0) lchecksum = checksum;

    if (lchecksum != checksum) {
      printf("f%02u: checksum fail!\n", f);
      ret = EXIT_FAILURE;
    }
  }

  //  ... Copy+Move
  assert(sizeof(move_fun)/sizeof(move_fun[0])
         == sizeof(checksum_fun)/sizeof(checksum_fun[0]));

  for (unsigned f = 0; f < sizeof(move_fun)/sizeof(move_fun[0]); f++) {
    memset(buf, 0xFE, buf_len);
    memset(dst, 0xFE, buf_len);

    bool     odd   = false;
    uint64_t start = rdtsc();
    uint16_t checksum = fold(checksum_fun[f](buf, buf_len, odd));
    memcpy(buf, dst, buf_len);
    uint64_t end   = rdtsc();

    assert(memcmp(dst, buf, buf_len) == 0);
    printf("f%02u: checksum+copy %3.2f bytes/cycle: %04x\n", f, static_cast<float>(buf_len) / (end - start), checksum);
  }


  return ret;
}

// EOF
