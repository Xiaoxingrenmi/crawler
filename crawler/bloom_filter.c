
// Bloom Filter
//   by BOT Man & ZhangHan, 2018

#include "bloom_filter.h"

#include <string.h>

// hash functions

unsigned RSHash(const char* str, unsigned length) {
  unsigned b = 378551;
  unsigned a = 63689;
  unsigned hash = 0;
  unsigned i = 0;

  for (i = 0; i < length; ++str, ++i) {
    hash = hash * a + (*str);
    a = a * b;
  }

  return hash;
}

unsigned JSHash(const char* str, unsigned length) {
  unsigned hash = 1315423911;
  unsigned i = 0;

  for (i = 0; i < length; ++str, ++i) {
    hash ^= ((hash << 5) + (*str) + (hash >> 2));
  }

  return hash;
}

unsigned PJWHash(const char* str, unsigned length) {
  const unsigned BitsInUnsignedInt = (unsigned)(sizeof(unsigned) * 8);
  const unsigned ThreeQuarters = (unsigned)((BitsInUnsignedInt * 3) / 4);
  const unsigned OneEighth = (unsigned)(BitsInUnsignedInt / 8);
  const unsigned HighBits = (unsigned)(0xFFFFFFFF)
                            << (BitsInUnsignedInt - OneEighth);
  unsigned hash = 0;
  unsigned test = 0;
  unsigned i = 0;

  for (i = 0; i < length; ++str, ++i) {
    hash = (hash << OneEighth) + (*str);

    if ((test = hash & HighBits) != 0) {
      hash = ((hash ^ (test >> ThreeQuarters)) & (~HighBits));
    }
  }

  return hash;
}

unsigned ELFHash(const char* str, unsigned length) {
  unsigned hash = 0;
  unsigned x = 0;
  unsigned i = 0;

  for (i = 0; i < length; ++str, ++i) {
    hash = (hash << 4) + (*str);

    if ((x = hash & 0xF0000000L) != 0) {
      hash ^= (x >> 24);
    }

    hash &= ~x;
  }

  return hash;
}

unsigned BKDRHash(const char* str, unsigned length) {
  unsigned seed = 131; /* 31 131 1313 13131 131313 etc.. */
  unsigned hash = 0;
  unsigned i = 0;

  for (i = 0; i < length; ++str, ++i) {
    hash = (hash * seed) + (*str);
  }

  return hash;
}

unsigned DJBHash(const char* str, unsigned length) {
  unsigned hash = 5381;
  unsigned i = 0;

  for (i = 0; i < length; ++str, ++i) {
    hash = ((hash << 5) + hash) + (*str);
  }

  return hash;
}

unsigned DEKHash(const char* str, unsigned length) {
  unsigned hash = length;
  unsigned i = 0;

  for (i = 0; i < length; ++str, ++i) {
    hash = ((hash << 5) ^ (hash >> 27)) ^ (*str);
  }

  return hash;
}

unsigned APHash(const char* str, unsigned length) {
  unsigned hash = 0xAAAAAAAA;
  unsigned i = 0;

  for (i = 0; i < length; ++str, ++i) {
    hash ^= ((i & 1) == 0) ? ((hash << 7) ^ (*str) * (hash >> 3))
                           : (~((hash << 11) + ((*str) ^ (hash >> 5))));
  }

  return hash;
}

unsigned (*g_hash_funcs[])(const char* str, unsigned length) = {
    RSHash, JSHash, PJWHash, ELFHash, BKDRHash, DJBHash, DEKHash, APHash};

// bloom filter contants

#define FILTER_M 9996999
#define FILTER_K (sizeof(g_hash_funcs) / sizeof(g_hash_funcs[0]))

// global storage

typedef unsigned char Unit;
#define UNIT_BIT (sizeof(Unit) * 8)
Unit g_bit_set[FILTER_M / UNIT_BIT + 1];  // = { 0 }

// reference:
// https://drewdevault.com/2016/04/12/How-to-write-a-better-bloom-filter-in-C.html

void BloomFilterAdd(const char* str) {
  if (!str)
    return;

  for (size_t i = 0; i < FILTER_K; i++) {
    unsigned hash = g_hash_funcs[i](str, (unsigned)strlen(str)) % FILTER_M;
    Unit set_value =
        (Unit)(g_bit_set[hash / UNIT_BIT] | ((Unit)1 << (hash % UNIT_BIT)));
    g_bit_set[hash / UNIT_BIT] = set_value;
  }
}

unsigned char BloomFilterTest(const char* str) {
  if (!str)
    return 0;

  for (size_t i = 0; i < FILTER_K; i++) {
    unsigned hash = g_hash_funcs[i](str, (unsigned)strlen(str)) % FILTER_M;
    Unit test_value =
        (Unit)(g_bit_set[hash / UNIT_BIT] & ((Unit)1 << (hash % UNIT_BIT)));
    if (!test_value)
      return 0;
  }
  return 1;
}
