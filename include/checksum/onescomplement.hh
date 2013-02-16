// One's complement checksum

#pragma once

#include <cstdint>
#include <cassert>
#include <cstring>

#ifdef __SSE2__
# include <emmintrin.h>
#endif

#ifdef __SSSE3__
# include <tmmintrin.h>
#endif

#include <compiler.h>
#include <endian.hh>

namespace OnesComplement {

  static inline unsigned long
  add(unsigned long a, unsigned long b)
  {
    asm ("add %1, %0;"
         "adc $0, %0;" : "+r" (a) : "rm" (b));
    return a;
  }

  static inline unsigned long
  add(unsigned long a, unsigned long b, unsigned long c)
  {
    asm ("add %1, %0;"
         "add %2, %0;"
         "adc $0, %0;" : "+r" (a) : "rm" (b), "rm" (c));
    return a;
  }

  /// Fold checksum state into the final 16-bit word. This is in host
  /// byte order.
  static uint16_t fold(unsigned long words)
  {
    uint32_t res = 0;
    for (unsigned word = 0; word < sizeof(words)/2; word++)
      res += (words >> 16*word) & 0xFFFF;
    return (res + (res >> 16));
  }

  static inline unsigned long
  checksum_rest(unsigned long state, uint8_t const *buf, size_t size, bool &odd)
  {
    while (size--) {
      uint16_t v = !odd ? *(buf++) : Endian::bswap16(*(buf++));
      state = add(state, v);
      odd = !odd;
    }
    return state;
  }

  static inline unsigned long
  checksum_move_rest(unsigned long state, uint8_t const *src, uint8_t *dst, size_t size, bool &odd)
  {
    memcpy(dst, src, size);
    return checksum_rest(state, src, size, odd);
  }

  /// Computes a one's complement checksum. Data is in network byte
  /// order. Checksum is in host byte order.
  static inline unsigned long
  checksum_adc(uint8_t const *buf, size_t size, bool &odd)
  {
    unsigned long rstate = 0;

    while (size >= sizeof(unsigned long)*4) {
      unsigned long const *b = reinterpret_cast<unsigned long const *>(buf);
      asm ("add %1, %0\n"
           "adc %2, %0\n"
           "adc %3, %0\n"
           "adc %4, %0\n"
           "adc $0, %0\n"
           : "+r" (rstate)
           : "rm" (b[0]),
             "rm" (b[1]),
             "rm" (b[2]),
             "rm" (b[3]));

      buf  += sizeof(unsigned long)*4;
      size -= sizeof(unsigned long)*4;
    }
    rstate = !odd ? rstate : Endian::bswapl(rstate);

    return checksum_rest(rstate, buf, size, odd);
  }

  static inline unsigned long
  checksum_move_adc(uint8_t const *src, uint8_t *dst, size_t size, bool &odd)
  {
    unsigned long rstate = 0;

    while (size >= sizeof(unsigned long)*4) {
      unsigned long const *b = reinterpret_cast<unsigned long const *>(src);
      unsigned long       *d = reinterpret_cast<unsigned long       *>(dst);

      d[0] = b[0];
      d[1] = b[1];
      d[2] = b[2];
      d[3] = b[3];

      asm ("add %1, %0\n"
           "adc %2, %0\n"
           "adc %3, %0\n"
           "adc %4, %0\n"
           "adc $0, %0\n"
           : "+r" (rstate)
           : "rm" (b[0]),
             "rm" (b[1]),
             "rm" (b[2]),
             "rm" (b[3]));


      src  += sizeof(unsigned long)*4;
      dst  += sizeof(unsigned long)*4;
      size -= sizeof(unsigned long)*4;
    }
    rstate = !odd ? rstate : Endian::bswapl(rstate);

    return checksum_move_rest(rstate, src, dst, size, odd);
  }

#ifdef __SSE2__
  static inline __m128i
  sse_step(__m128i sum, __m128i v1, __m128i v2)
  {
    __m128i z  = _mm_setzero_si128();
    __m128i s1 = _mm_add_epi64(_mm_unpackhi_epi32(v1, z),
                               _mm_unpacklo_epi32(v1, z));
    __m128i s2 = _mm_add_epi64(_mm_unpackhi_epi32(v2, z),
                               _mm_unpacklo_epi32(v2, z));

    sum = _mm_add_epi64(sum, s1);
    sum = _mm_add_epi64(sum, s2);
    return sum;
  }

  static inline unsigned long
  sse_final(__m128i sum, bool odd)
  {
    /* Add top to bottom 64-bit word */
    sum = _mm_add_epi64(sum, _mm_srli_si128(sum, 8));
    
    /* Add low two 32-bit words */
#ifdef __SSSE3__
    sum = _mm_hadd_epi32(sum, _mm_setzero_si128());
#else  // No SSSE3
#warning SSSE3 not available
    sum = _mm_add_epi32(sum, _mm_srli_si128(sum, 4));
#endif
    if (odd) {
      return Endian::bswap16(fold(_mm_cvtsi128_si32(sum)));
    } else
      return _mm_cvtsi128_si32(sum);
  }
#endif

  /// Computes a one's complement checksum using SSE. Data is in
  /// network byte order. Checksum is in host byte order.
  static inline unsigned long
  checksum_sse(uint8_t const *buf, size_t size, bool &odd)
  {
    unsigned long astate = 0;
#ifdef __SSE2__
    {
      __m128i     sum = _mm_setzero_si128();

      while (size >= sizeof(__m128i)*2) {
        __m128i v1 = _mm_loadu_si128(reinterpret_cast<__m128i const *>(buf));
        __m128i v2 = _mm_loadu_si128(reinterpret_cast<__m128i const *>(buf) + 1);

        sum = sse_step(sum, v1, v2);

        size -= sizeof(__m128i)*2;
        buf  += sizeof(__m128i)*2;
      }

      astate = sse_final(sum, odd);
    }
#else
#warning SSE2 not available
#endif

    return checksum_rest(astate, buf, size, odd);
  }

  static inline unsigned long
  checksum_move_sse(uint8_t const *src, uint8_t *dst, size_t size, bool &odd)
  {
    unsigned long astate = 0;

#ifdef __SSE2__
    {
      __m128i     sum = _mm_setzero_si128();

      while (size >= sizeof(__m128i)*2) {
        __m128i v1 = _mm_loadu_si128(reinterpret_cast<__m128i const *>(src));
        __m128i v2 = _mm_loadu_si128(reinterpret_cast<__m128i const *>(src) + 1);

        sum = sse_step(sum, v1, v2);

        _mm_storeu_si128(reinterpret_cast<__m128i *>(dst),     v1);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(dst) + 1, v2);

        size -= sizeof(__m128i)*2;
        src  += sizeof(__m128i)*2;
        dst  += sizeof(__m128i)*2;
      }

      astate = sse_final(sum, odd);
    }
#else
#warning SSE2 not available
#endif

    return checksum_move_rest(astate, src, dst, size, odd);
  }


  // Our checksum function of choice
  static inline unsigned long
  checksum(uint8_t const *buf, size_t size, bool &odd) {
    return checksum_sse(buf, size, odd);
  }

  static inline unsigned long
  checksum(uint8_t const *buf, size_t size) {
    bool odd = false;
    return checksum_sse(buf, size, odd);
  }

}

// EOF
