#ifndef OTRV4_INSTANCE_TAG_H
#define OTRV4_INSTANCE_TAG_H

#include <stdint.h>
#include <stdio.h>

#include "error.h"
#include "shared.h"

#define MIN_VALID_INSTAG 0x00000100

typedef struct {
  char *account;
  char *protocol;
  unsigned int value;
} otrv4_instag_t;

API otrv4_bool_t otrv4_instag_get(otrv4_instag_t *otrv4_instag,
                                  const char *account, const char *protocol,
                                  FILE *filename);

API void otrv4_instag_free(otrv4_instag_t *instag);

#ifdef OTRV4_INSTANCE_TAG_PRIVATE
#endif

#endif
