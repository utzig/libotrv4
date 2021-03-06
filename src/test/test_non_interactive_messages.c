#include "../constants.h"
#include "../dake.h"
#include "../keys.h"

void test_dake_prekey_message_serializes(prekey_message_fixture_t *f,
                                         gconstpointer data) {
  OTRV4_INIT;

  ecdh_keypair_t ecdh[1];
  dh_keypair_t dh;

  uint8_t sym[ED448_PRIVATE_BYTES] = {0};
  otrv4_ecdh_keypair_generate(ecdh, sym);
  otrv4_assert(otrv4_dh_keypair_generate(dh) == SUCCESS);

  dake_prekey_message_t *prekey_message =
      otrv4_dake_prekey_message_new(f->profile);
  prekey_message->sender_instance_tag = 1;
  otrv4_ec_point_copy(prekey_message->Y, ecdh->pub);
  prekey_message->B = otrv4_dh_mpi_copy(dh->pub);

  uint8_t *serialized = NULL;
  otrv4_assert(otrv4_dake_prekey_message_asprintf(&serialized, NULL,
                                                  prekey_message) == SUCCESS);

  char expected[] = {
      0x0,
      0x04,             /* version */
      PRE_KEY_MSG_TYPE, /* message type */
      0x0,
      0x0,
      0x0,
      0x1, /* sender instance tag */
      0x0,
      0x0,
      0x0,
      0x0, /* receiver instance tag */
  };

  uint8_t *cursor = serialized;
  otrv4_assert_cmpmem(cursor, expected, 11); /* size of expected */
  ;
  cursor += 11;

  size_t user_profile_len = 0;
  uint8_t *user_profile_serialized = NULL;
  otrv4_assert(otrv4_user_profile_asprintf(&user_profile_serialized,
                                           &user_profile_len,
                                           prekey_message->profile) == SUCCESS);
  otrv4_assert_cmpmem(cursor, user_profile_serialized, user_profile_len);
  free(user_profile_serialized);
  user_profile_serialized = NULL;

  cursor += user_profile_len;

  uint8_t serialized_y[ED448_POINT_BYTES + 2] = {0};
  otrv4_ec_point_serialize(serialized_y, prekey_message->Y);
  otrv4_assert_cmpmem(cursor, serialized_y, sizeof(ec_public_key_t));
  cursor += sizeof(ec_public_key_t);

  uint8_t serialized_b[DH3072_MOD_LEN_BYTES] = {0};
  size_t mpi_len = 0;
  otrv4_err_t err = otrv4_dh_mpi_serialize(serialized_b, DH3072_MOD_LEN_BYTES,
                                           &mpi_len, prekey_message->B);
  otrv4_assert(!err);
  /* Skip first 4 because they are the size (mpi_len) */
  otrv4_assert_cmpmem(cursor + 4, serialized_b, mpi_len);

  free(serialized);
  serialized = NULL;
  otrv4_dh_keypair_destroy(dh);
  otrv4_ecdh_keypair_destroy(ecdh);
  otrv4_dake_prekey_message_free(prekey_message);

  OTRV4_FREE;
}

void test_otrv4_dake_prekey_message_deserializes(prekey_message_fixture_t *f,
                                                 gconstpointer data) {
  OTRV4_INIT;

  ecdh_keypair_t ecdh[1];
  dh_keypair_t dh;

  uint8_t sym[ED448_PRIVATE_BYTES] = {1};
  otrv4_ecdh_keypair_generate(ecdh, sym);
  otrv4_assert(otrv4_dh_keypair_generate(dh) == SUCCESS);

  dake_prekey_message_t *prekey_message =
      otrv4_dake_prekey_message_new(f->profile);
  otrv4_ec_point_copy(prekey_message->Y, ecdh->pub);
  prekey_message->B = otrv4_dh_mpi_copy(dh->pub);

  size_t serialized_len = 0;
  uint8_t *serialized = NULL;
  otrv4_assert(otrv4_dake_prekey_message_asprintf(&serialized, &serialized_len,
                                                  prekey_message) == SUCCESS);

  dake_prekey_message_t *deserialized = malloc(sizeof(dake_prekey_message_t));
  otrv4_assert(otrv4_dake_prekey_message_deserialize(
                   deserialized, serialized, serialized_len) == SUCCESS);

  g_assert_cmpuint(deserialized->sender_instance_tag, ==,
                   prekey_message->sender_instance_tag);
  g_assert_cmpuint(deserialized->receiver_instance_tag, ==,
                   prekey_message->receiver_instance_tag);
  otrv4_assert_user_profile_eq(deserialized->profile, prekey_message->profile);
  otrv4_assert_ec_public_key_eq(deserialized->Y, prekey_message->Y);
  otrv4_assert_dh_public_key_eq(deserialized->B, prekey_message->B);

  free(serialized);
  serialized = NULL;
  otrv4_dh_keypair_destroy(dh);
  otrv4_ecdh_keypair_destroy(ecdh);
  otrv4_dake_prekey_message_free(prekey_message);
  otrv4_dake_prekey_message_free(deserialized);

  OTRV4_FREE;
}

void test_dake_prekey_message_valid(prekey_message_fixture_t *f,
                                    gconstpointer data) {
  OTRV4_INIT;

  ecdh_keypair_t ecdh[1];
  dh_keypair_t dh;

  uint8_t sym[ED448_PRIVATE_BYTES] = {1};
  otrv4_ecdh_keypair_generate(ecdh, sym);
  otrv4_assert(otrv4_dh_keypair_generate(dh) == SUCCESS);

  dake_prekey_message_t *prekey_message =
      otrv4_dake_prekey_message_new(f->profile);
  otrv4_assert(prekey_message != NULL);

  otrv4_ec_point_copy(prekey_message->Y, ecdh->pub);
  prekey_message->B = otrv4_dh_mpi_copy(dh->pub);

  otrv4_assert(otrv4_valid_received_values(prekey_message->Y, prekey_message->B,
                                           prekey_message->profile) ==
               otrv4_true);

  otrv4_ecdh_keypair_destroy(ecdh);
  otrv4_dh_keypair_destroy(dh);
  otrv4_dake_prekey_message_free(prekey_message);

  ecdh_keypair_t invalid_ecdh[1];
  dh_keypair_t invalid_dh;

  uint8_t invalid_sym[ED448_PRIVATE_BYTES] = {1};
  otrv4_ecdh_keypair_generate(invalid_ecdh, invalid_sym);
  otrv4_assert(otrv4_dh_keypair_generate(invalid_dh) == SUCCESS);
  otrv4_shared_prekey_pair_t *shared_prekey = otrv4_shared_prekey_pair_new();
  otrv4_shared_prekey_pair_generate(shared_prekey, invalid_sym);
  otrv4_assert(otrv4_ec_point_valid(shared_prekey->pub) == SUCCESS);

  user_profile_t *invalid_profile = user_profile_new("2");
  otrv4_ec_point_copy(invalid_profile->pub_key, invalid_ecdh->pub);
  otrv4_ec_point_copy(invalid_profile->shared_prekey, shared_prekey->pub);

  dake_prekey_message_t *invalid_prekey_message =
      otrv4_dake_prekey_message_new(invalid_profile);

  otrv4_ec_point_copy(invalid_prekey_message->Y, invalid_ecdh->pub);
  invalid_prekey_message->B = otrv4_dh_mpi_copy(invalid_dh->pub);

  otrv4_assert(otrv4_valid_received_values(
                   invalid_prekey_message->Y, invalid_prekey_message->B,
                   invalid_prekey_message->profile) == otrv4_false);

  otrv4_user_profile_free(invalid_profile);
  otrv4_ecdh_keypair_destroy(invalid_ecdh);
  otrv4_dh_keypair_destroy(invalid_dh);
  otrv4_shared_prekey_pair_free(shared_prekey);
  otrv4_dake_prekey_message_free(invalid_prekey_message);

  OTRV4_FREE;
}

void test_dake_non_interactive_auth_message_serializes(
    non_interactive_auth_message_fixture_t *f, gconstpointer data) {
  OTRV4_INIT;

  ecdh_keypair_t ecdh[1];
  dh_keypair_t dh;

  uint8_t sym[ED448_PRIVATE_BYTES] = {0};
  otrv4_ecdh_keypair_generate(ecdh, sym);
  otrv4_assert(otrv4_dh_keypair_generate(dh) == SUCCESS);

  dake_non_interactive_auth_message_t msg[1];

  msg->sender_instance_tag = 1;
  msg->receiver_instance_tag = 1;
  otrv4_user_profile_copy(msg->profile, f->profile);
  otrv4_ec_point_copy(msg->X, ecdh->pub);
  msg->A = otrv4_dh_mpi_copy(dh->pub);
  memset(msg->nonce, 0, sizeof(msg->nonce));
  msg->enc_msg = NULL;
  msg->enc_msg_len = 0;
  memset(msg->auth_mac, 0, sizeof(msg->auth_mac));

  uint8_t secret[1] = {0x01};
  shake_256_hash(msg->auth_mac, HASH_BYTES, secret, 1);

  unsigned char *t = NULL;
  size_t t_len = 0;
  otrv4_snizkpk_authenticate(msg->sigma, f->keypair, f->profile->pub_key,
                             msg->X, t, t_len);

  uint8_t *serialized = NULL;
  size_t len = 0;

  otrv4_assert(otrv4_dake_non_interactive_auth_message_asprintf(
                   &serialized, &len, msg) == SUCCESS);

  char expected[] = {
      0x0,
      0x04,                  /* version */
      NON_INT_AUTH_MSG_TYPE, /* message type */
      0x0,
      0x0,
      0x0,
      0x1, /* sender instance tag */
      0x0,
      0x0,
      0x0,
      0x1, /* receiver instance tag */
  };

  uint8_t *cursor = serialized;
  otrv4_assert_cmpmem(cursor, expected, 11); /* size of expected */
  cursor += 11;

  size_t user_profile_len = 0;
  uint8_t *user_profile_serialized = NULL;
  otrv4_assert(otrv4_user_profile_asprintf(&user_profile_serialized,
                                           &user_profile_len,
                                           msg->profile) == SUCCESS);
  otrv4_assert_cmpmem(cursor, user_profile_serialized, user_profile_len);
  free(user_profile_serialized);
  user_profile_serialized = NULL;

  cursor += user_profile_len;

  uint8_t serialized_x[ED448_POINT_BYTES + 2] = {};
  otrv4_ec_point_serialize(serialized_x, msg->X);
  otrv4_assert_cmpmem(cursor, serialized_x, sizeof(ec_public_key_t));
  cursor += sizeof(ec_public_key_t);

  uint8_t serialized_a[DH3072_MOD_LEN_BYTES] = {};
  size_t mpi_len = 0;
  otrv4_err_t err = otrv4_dh_mpi_serialize(serialized_a, DH3072_MOD_LEN_BYTES,
                                           &mpi_len, msg->A);
  otrv4_assert(!err);

  /* Skip first 4 because they are the size (mpi_len) */
  otrv4_assert_cmpmem(cursor + 4, serialized_a, mpi_len);

  cursor += 4 + mpi_len;

  uint8_t serialized_snizkpk[SNIZKPK_BYTES] = {};
  otrv4_serialize_snizkpk_proof(serialized_snizkpk, msg->sigma);

  otrv4_assert_cmpmem(cursor, serialized_snizkpk, SNIZKPK_BYTES);

  cursor += SNIZKPK_BYTES;

  uint8_t serialized_mac[HASH_BYTES] = {};
  cursor += otrv4_serialize_bytes_array(cursor, msg->auth_mac, HASH_BYTES);

  otrv4_assert_cmpmem(cursor, serialized_mac, HASH_BYTES);

  free(t);
  t = NULL;
  free(serialized);
  serialized = NULL;

  otrv4_dh_keypair_destroy(dh);
  otrv4_ecdh_keypair_destroy(ecdh);
  otrv4_dake_non_interactive_auth_message_destroy(msg);

  OTRV4_FREE;
}

void test_otrv4_dake_non_interactive_auth_message_deserializes(
    prekey_message_fixture_t *f, gconstpointer data) {
  OTRV4_INIT;

  ecdh_keypair_t ecdh[1];
  dh_keypair_t dh;

  uint8_t sym[ED448_PRIVATE_BYTES] = {0};
  otrv4_ecdh_keypair_generate(ecdh, sym);
  otrv4_assert(otrv4_dh_keypair_generate(dh) == SUCCESS);

  dake_non_interactive_auth_message_t msg[1];

  msg->sender_instance_tag = 1;
  msg->receiver_instance_tag = 1;
  otrv4_user_profile_copy(msg->profile, f->profile);
  otrv4_ec_point_copy(msg->X, ecdh->pub);
  msg->A = otrv4_dh_mpi_copy(dh->pub);
  memset(msg->nonce, 0, sizeof(msg->nonce));
  msg->enc_msg = NULL;
  msg->enc_msg_len = 0;
  memset(msg->auth_mac, 0, sizeof(msg->auth_mac));

  uint8_t secret[1] = {0x01};
  shake_256_hash(msg->auth_mac, HASH_BYTES, secret, 1);

  unsigned char *t = NULL;
  size_t t_len = 0;
  otrv4_snizkpk_authenticate(msg->sigma, f->keypair, f->profile->pub_key,
                             msg->X, t, t_len);

  uint8_t *serialized = NULL;
  size_t len = 0;
  otrv4_assert(otrv4_dake_non_interactive_auth_message_asprintf(
                   &serialized, &len, msg) == SUCCESS);

  free(t);
  t = NULL;
  otrv4_dh_keypair_destroy(dh);
  otrv4_ecdh_keypair_destroy(ecdh);

  dake_non_interactive_auth_message_t deserialized[1];
  otrv4_assert(otrv4_dake_non_interactive_auth_message_deserialize(
                   deserialized, serialized, len) == SUCCESS);

  g_assert_cmpuint(deserialized->sender_instance_tag, ==,
                   msg->sender_instance_tag);
  g_assert_cmpuint(deserialized->receiver_instance_tag, ==,
                   msg->receiver_instance_tag);
  otrv4_assert_user_profile_eq(deserialized->profile, msg->profile);
  otrv4_assert_ec_public_key_eq(deserialized->X, msg->X);
  otrv4_assert_dh_public_key_eq(deserialized->A, msg->A);
  otrv4_assert(memcmp(deserialized->auth_mac, msg->auth_mac, HASH_BYTES));
  otrv4_assert(memcmp(deserialized->sigma, msg->sigma, SNIZKPK_BYTES));

  serialized = NULL;
  free(serialized);
  otrv4_dake_non_interactive_auth_message_destroy(msg);
  otrv4_dake_non_interactive_auth_message_destroy(deserialized);

  OTRV4_FREE;
}
