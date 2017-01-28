#include <gcrypt.h>

#ifndef RANDOM_H
#define RANDOM_H

static void inline
random_bytes(void * const buf, const size_t size) {
  gcry_randomize(buf, size, GCRY_STRONG_RANDOM);
}

#endif