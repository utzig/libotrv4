#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define OTRV4_DESERIALIZE_PRIVATE

#include "deserialize.h"
#include "serialize.h"
#include "user_profile.h"

tstatic user_profile_t *user_profile_new(const string_t versions) {
  if (!versions)
    return NULL;

  user_profile_t *profile = malloc(sizeof(user_profile_t));
  if (!profile)
    return NULL;

  otrv4_ec_bzero(profile->pub_key, ED448_POINT_BYTES);
  profile->expires = 0;
  profile->versions = otrv4_strdup(versions);
  otrv4_ec_bzero(profile->shared_prekey, ED448_POINT_BYTES);
  memset(profile->signature, 0, sizeof(profile->signature));
  otrv4_mpi_init(profile->transitional_signature);

  return profile;
}

INTERNAL void otrv4_user_profile_copy(user_profile_t *dst,
                                      const user_profile_t *src) {
  // TODO should we set dst to a valid (but empty) profile?
  if (!src)
    return;

  otrv4_ec_point_copy(dst->pub_key, src->pub_key);
  dst->versions = otrv4_strdup(src->versions);
  dst->expires = src->expires;
  otrv4_ec_point_copy(dst->shared_prekey, src->shared_prekey);

  memcpy(dst->signature, src->signature, sizeof(eddsa_signature_t));
  otrv4_mpi_copy(dst->transitional_signature, src->transitional_signature);
}

INTERNAL void otrv4_user_profile_destroy(user_profile_t *profile) {
  if (!profile)
    return;

  otrv4_ec_point_destroy(profile->pub_key);
  free(profile->versions);
  profile->versions = NULL;
  sodium_memzero(profile->signature, ED448_SIGNATURE_BYTES);
  otrv4_ec_point_destroy(profile->shared_prekey);
  otrv4_mpi_free(profile->transitional_signature);
}

INTERNAL void otrv4_user_profile_free(user_profile_t *profile) {
  otrv4_user_profile_destroy(profile);
  free(profile);
  profile = NULL;
}

tstatic int user_profile_body_serialize(uint8_t *dst,
                                        const user_profile_t *profile) {
  uint8_t *target = dst;

  target += otrv4_serialize_otrv4_public_key(target, profile->pub_key);
  target += otrv4_serialize_data(target, (uint8_t *)profile->versions,
                                 strlen(profile->versions) + 1);
  target += otrv4_serialize_uint64(target, profile->expires);
  target += otrv4_serialize_otrv4_shared_prekey(target, profile->shared_prekey);

  return target - dst;
}

tstatic otrv4_err_t user_profile_body_asprintf(uint8_t **dst, size_t *nbytes,
                                               const user_profile_t *profile) {
  size_t s = ED448_PUBKEY_BYTES + strlen(profile->versions) +
             ED448_SHARED_PREKEY_BYTES + 1 + 4 + 8;

  uint8_t *buff = malloc(s);
  if (!buff)
    return ERROR;

  user_profile_body_serialize(buff, profile);

  *dst = buff;
  if (nbytes)
    *nbytes = s;

  return SUCCESS;
}

INTERNAL otrv4_err_t otrv4_user_profile_asprintf(
    uint8_t **dst, size_t *nbytes, const user_profile_t *profile) {
  // TODO: should it checked here for signature?
  if (!(profile->signature > 0))
    return ERROR;

  uint8_t *buff = NULL;
  size_t body_len = 0;
  uint8_t *body = NULL;
  if (user_profile_body_asprintf(&body, &body_len, profile))
    return ERROR;

  size_t s = body_len + 4 + sizeof(eddsa_signature_t) +
             profile->transitional_signature->len;
  buff = malloc(s);
  if (!buff) {
    free(body);
    body = NULL;
    return ERROR;
  }

  uint8_t *cursor = buff;
  cursor += otrv4_serialize_bytes_array(cursor, body, body_len);
  cursor += otrv4_serialize_bytes_array(cursor, profile->signature,
                                        sizeof(eddsa_signature_t));
  cursor += otrv4_serialize_mpi(cursor, profile->transitional_signature);

  *dst = buff;
  if (nbytes)
    *nbytes = s;

  free(body);
  body = NULL;

  return SUCCESS;
}

INTERNAL otrv4_err_t otrv4_user_profile_deserialize(user_profile_t *target,
                                                    const uint8_t *buffer,
                                                    size_t buflen,
                                                    size_t *nread) {
  size_t read = 0;
  int walked = 0;

  if (!target)
    return ERROR;

  otrv4_err_t ok = ERROR;
  do {
    if (otrv4_deserialize_otrv4_public_key(target->pub_key, buffer, buflen,
                                           &read))
      continue;

    walked += read;

    if (otrv4_deserialize_data((uint8_t **)&target->versions, buffer + walked,
                               buflen - walked, &read))
      continue;

    walked += read;

    if (otrv4_deserialize_uint64(&target->expires, buffer + walked,
                                 buflen - walked, &read))
      continue;

    walked += read;

    if (otrv4_deserialize_otrv4_shared_prekey(
            target->shared_prekey, buffer + walked, buflen - walked, &read))
      continue;

    walked += read;

    // TODO: check the len
    if (buflen - walked < sizeof(eddsa_signature_t))
      continue;

    memcpy(target->signature, buffer + walked, sizeof(eddsa_signature_t));

    walked += sizeof(eddsa_signature_t);

    if (otrv4_mpi_deserialize(target->transitional_signature, buffer + walked,
                              buflen - walked, &read))
      continue;

    walked += read;

    ok = SUCCESS;
  } while (0);

  if (nread)
    *nread = walked;

  return ok;
}

tstatic otrv4_err_t user_profile_sign(user_profile_t *profile,
                                      const otrv4_keypair_t *keypair) {
  uint8_t *body = NULL;
  size_t bodylen = 0;

  otrv4_ec_point_copy(profile->pub_key, keypair->pub);
  if (user_profile_body_asprintf(&body, &bodylen, profile))
    return ERROR;

  uint8_t pubkey[ED448_POINT_BYTES];
  otrv4_ec_point_serialize(pubkey, keypair->pub);

  // maybe otrv4_ec_derive_public_key again?
  otrv4_ec_sign(profile->signature, (uint8_t *)keypair->sym, pubkey, body,
                bodylen);

  free(body);
  body = NULL;
  return SUCCESS;
}

// TODO: I dont think this needs the data structure. Could verify from the
// deserialized bytes.
INTERNAL otrv4_bool_t
otrv4_user_profile_verify_signature(const user_profile_t *profile) {
  uint8_t *body = NULL;
  size_t bodylen = 0;

  if (!(profile->signature > 0))
    return otrv4_false;

  if (otrv4_ec_point_valid(profile->shared_prekey) == ERROR)
    return otrv4_false;

  if (user_profile_body_asprintf(&body, &bodylen, profile))
    return otrv4_false;

  uint8_t pubkey[ED448_POINT_BYTES];
  otrv4_ec_point_serialize(pubkey, profile->pub_key);

  otrv4_bool_t valid =
      otrv4_ec_verify(profile->signature, pubkey, body, bodylen);

  free(body);
  body = NULL;

  return valid;
}

INTERNAL user_profile_t *
otrv4_user_profile_build(const string_t versions, otrv4_keypair_t *keypair,
                         otrv4_shared_prekey_pair_t *shared_prekey_pair) {
  user_profile_t *profile = user_profile_new(versions);
  if (!profile)
    return NULL;

#define PROFILE_EXPIRATION_SECONDS 2 * 7 * 24 * 60 * 60; /* 2 weeks */
  time_t expires = time(NULL);
  profile->expires = expires + PROFILE_EXPIRATION_SECONDS;

  memcpy(profile->shared_prekey, shared_prekey_pair->pub,
         sizeof(otrv4_shared_prekey_pub_t));

  if (user_profile_sign(profile, keypair)) {
    otrv4_user_profile_free(profile);
    return NULL;
  }

  return profile;
}
