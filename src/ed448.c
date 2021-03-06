#include <gcrypt.h>
#include <sodium.h>

#define OTRV4_ED448_PRIVATE

#include "ed448.h"

INTERNAL void otrv4_ec_bzero(void *data, size_t size) {
  decaf_bzero(data, size);
}

INTERNAL otrv4_bool_t otrv4_ec_scalar_eq(const ec_scalar_t a,
                                         const ec_scalar_t b) {
  if (decaf_448_scalar_eq(a, b) == DECAF_TRUE) {
    return otrv4_true;
  }

  return otrv4_false;
}

INTERNAL void otrv4_ec_point_copy(ec_point_t dst, const ec_point_t src) {
  decaf_448_point_copy(dst, src);
}

INTERNAL void otrv4_ec_point_destroy(ec_point_t dst) {
  decaf_448_point_destroy(dst);
}

INTERNAL void otrv4_ecdh_keypair_generate(ecdh_keypair_t *keypair,
                                          uint8_t sym[ED448_PRIVATE_BYTES]) {
  otrv4_ec_scalar_derive_from_secret(keypair->priv, sym);

  uint8_t pub[ED448_POINT_BYTES];
  otrv4_ec_derive_public_key(pub, sym);
  otrv4_ec_point_deserialize(keypair->pub, pub);

  decaf_bzero(sym, ED448_POINT_BYTES);
  decaf_bzero(pub, ED448_POINT_BYTES);
}

INTERNAL void otrv4_ecdh_keypair_destroy(ecdh_keypair_t *keypair) {
  otrv4_ec_scalar_destroy(keypair->priv);
  otrv4_ec_point_destroy(keypair->pub);
}

INTERNAL void
otrv4_ec_scalar_derive_from_secret(ec_scalar_t priv,
                                   uint8_t sym[ED448_PRIVATE_BYTES]) {
  /* Hash and clamp the secret into a scalar */
  decaf_ed448_derive_secret_scalar(priv, sym);
}

INTERNAL void
otrv4_ec_derive_public_key(uint8_t pub[ED448_POINT_BYTES],
                           const uint8_t sym[ED448_PRIVATE_BYTES]) {
  decaf_ed448_derive_public_key(pub, sym);
}

INTERNAL void otrv4_ecdh_shared_secret(uint8_t *shared,
                                       const ecdh_keypair_t *our_keypair,
                                       const ec_point_t their_pub) {
  decaf_448_point_t s;
  decaf_448_point_scalarmul(s, their_pub, our_keypair->priv);

  otrv4_ec_point_serialize(shared, s);
}

/* void ec_public_key_copy(ec_public_key_t dst, const ec_public_key_t src) { */
/*   memcpy(dst, src, sizeof(ec_public_key_t)); */
/* } */

INTERNAL otrv4_err_t otrv4_ec_scalar_serialize(uint8_t *dst, size_t dst_len,
                                               const ec_scalar_t scalar) {
  if (dst_len < ED448_SCALAR_BYTES)
    return ERROR;

  decaf_448_scalar_encode(dst, scalar);

  return SUCCESS;
}

INTERNAL void
otrv4_ec_scalar_deserialize(ec_scalar_t scalar,
                            const uint8_t serialized[ED448_SCALAR_BYTES]) {
  decaf_448_scalar_decode_long(scalar, serialized, ED448_SCALAR_BYTES);
}

INTERNAL void otrv4_ec_point_serialize(uint8_t *dst, const ec_point_t point) {
  decaf_448_point_mul_by_cofactor_and_encode_like_eddsa(dst, point);
}

INTERNAL otrv4_err_t otrv4_ec_point_deserialize(
    ec_point_t point, const uint8_t serialized[ED448_POINT_BYTES]) {
  decaf_448_point_t p;
  decaf_error_t err =
      decaf_448_point_decode_like_eddsa_and_ignore_cofactor(p, serialized);
  if (DECAF_SUCCESS != err)
    return ERROR;

  // The decoded point is equal to the original point * 2^2
  decaf_448_scalar_t r;
  decaf_448_scalar_copy(r, decaf_448_scalar_one);
  decaf_448_scalar_halve(r, r);
  decaf_448_scalar_halve(r, r);

  decaf_448_point_scalarmul(point, p, r);

  return SUCCESS;
}

static const char *ctx = "";

INTERNAL void otrv4_ec_sign(eddsa_signature_t dst,
                            uint8_t sym[ED448_PRIVATE_BYTES],
                            uint8_t pubkey[ED448_POINT_BYTES],
                            const uint8_t *msg, size_t msg_len) {
  decaf_ed448_sign(dst, sym, pubkey, msg, msg_len, 0, (uint8_t *)ctx,
                   strlen(ctx));
}

INTERNAL otrv4_bool_t otrv4_ec_verify(const uint8_t sig[ED448_SIGNATURE_BYTES],
                                      const uint8_t pubkey[ED448_POINT_BYTES],
                                      const uint8_t *msg, size_t msg_len) {
  if (decaf_ed448_verify(sig, pubkey, msg, msg_len, 0, (uint8_t *)ctx,
                         strlen(ctx)) == DECAF_TRUE)
    return otrv4_true;

  return otrv4_false;
}

INTERNAL void otrv4_ec_scalar_copy(ec_scalar_t dst, const ec_scalar_t src) {
  decaf_448_scalar_copy(dst, src);
}

INTERNAL void otrv4_ec_scalar_destroy(ec_scalar_t dst) {
  decaf_448_scalar_destroy(dst);
}

INTERNAL otrv4_bool_t otrv4_ec_point_valid(const ec_point_t point) {
  if (DECAF_TRUE == decaf_448_point_valid(point)) {
    return otrv4_true;
  }

  return otrv4_false;
}

INTERNAL otrv4_bool_t otrv4_ec_point_eq(const ec_point_t p,
                                        const ec_point_t q) {
  if (decaf_448_point_eq(p, q) == DECAF_TRUE)
    return otrv4_true;

  return otrv4_false;
}
