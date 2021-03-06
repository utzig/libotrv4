#include <libotr/mem.h>

#define OTRV4_DATA_MESSAGE_PRIVATE

#include "data_message.h"
#include "deserialize.h"
#include "serialize.h"
#include "shake.h"

INTERNAL data_message_t *otrv4_data_message_new() {
  data_message_t *ret = malloc(sizeof(data_message_t));
  if (!ret)
    return NULL;

  ret->flags = 0;
  ret->enc_msg = NULL;
  ret->enc_msg_len = 0;

  otrv4_ec_bzero(ret->ecdh, ED448_POINT_BYTES);

  memset(ret->nonce, 0, sizeof ret->nonce);
  memset(ret->mac, 0, sizeof ret->mac);

  return ret;
}

tstatic void data_message_destroy(data_message_t *data_msg) {
  data_msg->flags = 0;

  otrv4_ec_point_destroy(data_msg->ecdh);
  otrv4_dh_mpi_release(data_msg->dh);
  data_msg->dh = NULL;

  sodium_memzero(data_msg->nonce, sizeof data_msg->nonce);
  data_msg->enc_msg_len = 0;
  // TODO: check if this free is always needed
  free(data_msg->enc_msg);
  data_msg->enc_msg = NULL;
  sodium_memzero(data_msg->mac, sizeof data_msg->mac);
}

INTERNAL void otrv4_data_message_free(data_message_t *data_msg) {
  if (!data_msg)
    return;

  data_message_destroy(data_msg);

  free(data_msg);
  data_msg = NULL;
}

INTERNAL otrv4_err_t otrv4_data_message_body_asprintf(
    uint8_t **body, size_t *bodylen, const data_message_t *data_msg) {
  size_t s = DATA_MESSAGE_MIN_BYTES + DH_MPI_BYTES + 4 + data_msg->enc_msg_len;
  uint8_t *dst = malloc(s);
  if (!dst)
    return ERROR;

  uint8_t *cursor = dst;
  cursor += otrv4_serialize_uint16(cursor, VERSION);
  cursor += otrv4_serialize_uint8(cursor, DATA_MSG_TYPE);
  cursor += otrv4_serialize_uint32(cursor, data_msg->sender_instance_tag);
  cursor += otrv4_serialize_uint32(cursor, data_msg->receiver_instance_tag);
  cursor += otrv4_serialize_uint8(cursor, data_msg->flags);
  cursor += otrv4_serialize_uint32(cursor, data_msg->message_id);
  cursor += otrv4_serialize_ec_point(cursor, data_msg->ecdh);

  // TODO: This could be NULL. We need to test.
  size_t len = 0;
  if (otrv4_serialize_dh_public_key(cursor, &len, data_msg->dh)) {
    free(dst);
    dst = NULL;
    return ERROR;
  }
  cursor += len;
  cursor += otrv4_serialize_bytes_array(cursor, data_msg->nonce,
                                        DATA_MSG_NONCE_BYTES);
  cursor +=
      otrv4_serialize_data(cursor, data_msg->enc_msg, data_msg->enc_msg_len);

  if (body)
    *body = dst;

  if (bodylen)
    *bodylen = cursor - dst;

  return SUCCESS;
}

INTERNAL otrv4_err_t otrv4_data_message_deserialize(data_message_t *dst,
                                                    const uint8_t *buff,
                                                    size_t bufflen,
                                                    size_t *nread) {
  const uint8_t *cursor = buff;
  int64_t len = bufflen;
  size_t read = 0;

  uint16_t protocol_version = 0;
  if (otrv4_deserialize_uint16(&protocol_version, cursor, len, &read))
    return ERROR;

  cursor += read;
  len -= read;

  if (protocol_version != VERSION)
    return ERROR;

  uint8_t message_type = 0;
  if (otrv4_deserialize_uint8(&message_type, cursor, len, &read))
    return ERROR;

  cursor += read;
  len -= read;

  if (message_type != DATA_MSG_TYPE)
    return ERROR;

  if (otrv4_deserialize_uint32(&dst->sender_instance_tag, cursor, len, &read))
    return ERROR;

  cursor += read;
  len -= read;

  if (otrv4_deserialize_uint32(&dst->receiver_instance_tag, cursor, len, &read))
    return ERROR;

  cursor += read;
  len -= read;

  if (otrv4_deserialize_uint8(&dst->flags, cursor, len, &read))
    return ERROR;

  cursor += read;
  len -= read;

  if (otrv4_deserialize_uint32(&dst->message_id, cursor, len, &read))
    return ERROR;

  cursor += read;
  len -= read;

  if (otrv4_deserialize_ec_point(dst->ecdh, cursor))
    return ERROR;

  cursor += ED448_POINT_BYTES;
  len -= ED448_POINT_BYTES;

  // TODO: This could be NULL. We need to test.

  otrv4_mpi_t b_mpi; // no need to free, because nothing is copied now
  if (otrv4_mpi_deserialize_no_copy(b_mpi, cursor, len, &read))
    return ERROR;

  cursor += read;
  len -= read;

  if (otrv4_dh_mpi_deserialize(&dst->dh, b_mpi->data, b_mpi->len, &read))
    return ERROR;

  cursor += read;
  len -= read;

  if (otrv4_deserialize_bytes_array(dst->nonce, DATA_MSG_NONCE_BYTES, cursor,
                                    len))
    return ERROR;

  cursor += DATA_MSG_NONCE_BYTES;
  len -= DATA_MSG_NONCE_BYTES;

  if (otrv4_deserialize_data(&dst->enc_msg, cursor, len, &read))
    return ERROR;

  dst->enc_msg_len = read - 4;
  cursor += read;
  len -= read;

  return otrv4_deserialize_bytes_array((uint8_t *)&dst->mac, DATA_MSG_MAC_BYTES,
                                       cursor, len);
}

INTERNAL otrv4_bool_t otrv4_valid_data_message(m_mac_key_t mac_key,
                                               const data_message_t *data_msg) {
  uint8_t *body = NULL;
  size_t bodylen = 0;

  if (otrv4_data_message_body_asprintf(&body, &bodylen, data_msg)) {
    return otrv4_false;
  }

  uint8_t mac_tag[DATA_MSG_MAC_BYTES];
  memset(mac_tag, 0, sizeof mac_tag);

  shake_256_mac(mac_tag, sizeof mac_tag, mac_key, sizeof(m_mac_key_t), body,
                bodylen);

  free(body);
  body = NULL;

  if (otrl_mem_differ(mac_tag, data_msg->mac, sizeof mac_tag) != 0) {
    sodium_memzero(mac_tag, sizeof mac_tag);
    return otrv4_false;
  }

  if (otrv4_ec_point_valid(data_msg->ecdh))
    return otrv4_false;

  return otrv4_dh_mpi_valid(data_msg->dh);
}
