#ifndef OTRV4_MPI_H
#define OTRV4_MPI_H

#include <stdint.h>
#include <stdlib.h>

#include "shared.h"
#include "error.h"


typedef struct {
  uint32_t len;
  uint8_t *data;
} otr_mpi_t[1];

INTERNAL void otr_mpi_init(otr_mpi_t mpi);

INTERNAL void otr_mpi_free(otr_mpi_t mpi);

INTERNAL void otr_mpi_set(otr_mpi_t mpi, const uint8_t *src, size_t len);

INTERNAL void otr_mpi_copy(otr_mpi_t dst, const otr_mpi_t src);

INTERNAL otrv4_err_t otr_mpi_deserialize(otr_mpi_t dst, const uint8_t *src,
                                size_t src_len, size_t *read);

INTERNAL otrv4_err_t otr_mpi_deserialize_no_copy(otr_mpi_t dst, const uint8_t *src,
                                        size_t src_len, size_t *read);

INTERNAL size_t otr_mpi_memcpy(uint8_t *dst, const otr_mpi_t mpi);


#ifdef OTRV4_MPI_PRIVATE
#endif

#endif
