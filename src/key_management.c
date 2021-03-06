#include <sodium.h>
#include <stdlib.h>
#include <time.h>

#define OTRV4_KEY_MANAGEMENT_PRIVATE

#include "key_management.h"
#include "random.h"
#include "shake.h"

#include "debug.h"

tstatic void chain_link_free(chain_link_t *head) {
  chain_link_t *current = head;
  chain_link_t *next = NULL;
  while (current) {
    next = current->next;

    sodium_memzero(current->key, sizeof(chain_key_t));
    free(current);

    current = next;
  }
}

tstatic ratchet_t *ratchet_new() {
  ratchet_t *ratchet = malloc(sizeof(ratchet_t));
  if (!ratchet)
    return NULL;

  memset(ratchet->root_key, 0, sizeof(ratchet->root_key));

  ratchet->chain_a->id = 0;
  memset(ratchet->chain_a->key, 0, sizeof(ratchet->chain_a->key));
  ratchet->chain_a->next = NULL;

  ratchet->chain_b->id = 0;
  memset(ratchet->chain_b->key, 0, sizeof(ratchet->chain_b->key));
  ratchet->chain_b->next = NULL;

  return ratchet;
}

tstatic void ratchet_free(ratchet_t *ratchet) {
  if (!ratchet)
    return;

  sodium_memzero(ratchet->root_key, sizeof(root_key_t));

  chain_link_free(ratchet->chain_a->next);
  ratchet->chain_a->next = NULL;

  chain_link_free(ratchet->chain_b->next);
  ratchet->chain_b->next = NULL;

  free(ratchet);
  ratchet = NULL;
}

INTERNAL void
otrv4_key_manager_init(key_manager_t *manager) // make like ratchet_new?
{
  otrv4_ec_bzero(manager->our_ecdh->pub, ED448_POINT_BYTES);
  manager->our_dh->pub = NULL;
  manager->our_dh->priv = NULL;

  otrv4_ec_bzero(manager->their_ecdh, ED448_POINT_BYTES);
  manager->their_dh = NULL;

  otrv4_ec_bzero(manager->their_shared_prekey, ED448_POINT_BYTES);
  otrv4_ec_bzero(manager->our_shared_prekey, ED448_POINT_BYTES);

  manager->i = 0;
  manager->j = 0;

  manager->current = ratchet_new();

  memset(manager->brace_key, 0, sizeof(manager->brace_key));
  memset(manager->ssid, 0, sizeof(manager->ssid));
  manager->ssid_half = 0;
  memset(manager->extra_key, 0, sizeof(manager->extra_key));
  memset(manager->tmp_key, 0, sizeof(manager->tmp_key));

  manager->old_mac_keys = NULL;
}

INTERNAL void otrv4_key_manager_destroy(key_manager_t *manager) {
  otrv4_ecdh_keypair_destroy(manager->our_ecdh);
  otrv4_dh_keypair_destroy(manager->our_dh);

  otrv4_ec_point_destroy(manager->their_ecdh);

  gcry_mpi_release(manager->their_dh);
  manager->their_dh = NULL;

  ratchet_free(manager->current);
  manager->current = NULL;

  // TODO: once ake is finished should be wiped out
  sodium_memzero(manager->their_shared_prekey, ED448_POINT_BYTES);
  sodium_memzero(manager->our_shared_prekey, ED448_POINT_BYTES);

  sodium_memzero(manager->brace_key, sizeof(manager->brace_key));
  sodium_memzero(manager->ssid, sizeof(manager->ssid));
  manager->ssid_half = 0;
  sodium_memzero(manager->extra_key, sizeof(manager->extra_key));
  // TODO: once ake is finished should be wiped out
  sodium_memzero(manager->tmp_key, sizeof(manager->tmp_key));

  list_element_t *el;
  for (el = manager->old_mac_keys; el; el = el->next) {
    free((uint8_t *)el->data);
    el->data = NULL;
  }

  otrv4_list_free_full(manager->old_mac_keys);
  manager->old_mac_keys = NULL;
}

INTERNAL otrv4_err_t
otrv4_key_manager_generate_ephemeral_keys(key_manager_t *manager) {
  time_t now;
  uint8_t sym[ED448_PRIVATE_BYTES];
  memset(sym, 0, sizeof(sym));
  random_bytes(sym, ED448_PRIVATE_BYTES);

  now = time(NULL);
  otrv4_ec_point_destroy(manager->our_ecdh->pub);
  otrv4_ecdh_keypair_generate(manager->our_ecdh, sym);
  manager->lastgenerated = now;

  if (manager->i % 3 == 0) {
    otrv4_dh_keypair_destroy(manager->our_dh);

    if (otrv4_dh_keypair_generate(manager->our_dh)) {
      return ERROR;
    }
  }

  return SUCCESS;
}

INTERNAL void otrv4_key_manager_set_their_keys(ec_point_t their_ecdh,
                                               dh_public_key_t their_dh,
                                               key_manager_t *manager) {
  otrv4_ec_point_destroy(manager->their_ecdh);
  otrv4_ec_point_copy(manager->their_ecdh, their_ecdh);
  otrv4_dh_mpi_release(manager->their_dh);
  manager->their_dh = otrv4_dh_mpi_copy(their_dh);
}

INTERNAL void otrv4_key_manager_prepare_to_ratchet(key_manager_t *manager) {
  manager->j = 0;
}

tstatic void derive_key_from_shared_secret(uint8_t *key, size_t keylen,
                                           const uint8_t magic[1],
                                           const shared_secret_t shared) {
  shake_256_kdf(key, keylen, magic, shared, sizeof(shared_secret_t));
}

tstatic void derive_root_key(root_key_t root_key,
                             const shared_secret_t shared) {
  uint8_t magic[1] = {0x1};
  derive_key_from_shared_secret(root_key, sizeof(root_key_t), magic, shared);
}

tstatic void derive_chain_key_a(chain_key_t chain_key,
                                const shared_secret_t shared) {
  uint8_t magic[1] = {0x2};
  derive_key_from_shared_secret(chain_key, sizeof(chain_key_t), magic, shared);
}

tstatic void derive_chain_key_b(chain_key_t chain_key,
                                const shared_secret_t shared) {
  uint8_t magic[1] = {0x3};
  derive_key_from_shared_secret(chain_key, sizeof(chain_key_t), magic, shared);
}

tstatic otrv4_err_t key_manager_new_ratchet(key_manager_t *manager,
                                            const shared_secret_t shared) {
  ratchet_t *ratchet = ratchet_new();
  if (ratchet == NULL) {
    return ERROR;
  }
  if (manager->i == 0) {
    derive_root_key(ratchet->root_key, shared);
    derive_chain_key_a(ratchet->chain_a->key, shared);
    derive_chain_key_b(ratchet->chain_b->key, shared);
  } else {
    shared_secret_t root_shared;
    shake_kkdf(root_shared, sizeof(shared_secret_t), manager->current->root_key,
               sizeof(root_key_t), shared, sizeof(shared_secret_t));
    derive_root_key(ratchet->root_key, root_shared);
    derive_chain_key_a(ratchet->chain_a->key, root_shared);
    derive_chain_key_b(ratchet->chain_b->key, root_shared);
  }

  ratchet_free(manager->current);
  manager->current = ratchet;

  return SUCCESS;
}

tstatic const chain_link_t *chain_get_last(const chain_link_t *head) {
  const chain_link_t *cursor = head;
  while (cursor->next)
    cursor = cursor->next;

  return cursor;
}

tstatic const chain_link_t *chain_get_by_id(int message_id,
                                            const chain_link_t *head) {
  const chain_link_t *cursor = head;
  while (cursor->next && cursor->id != message_id)
    cursor = cursor->next;

  if (cursor->id == message_id) {
    return cursor;
  }

  return NULL;
}

tstatic message_chain_t *decide_between_chain_keys(const ratchet_t *ratchet,
                                                   const ec_point_t our,
                                                   const ec_point_t their) {
  message_chain_t *ret = malloc(sizeof(message_chain_t));
  if (ret == NULL)
    return NULL;

  ret->sending = NULL;
  ret->receiving = NULL;

  // TODO: this conversion from point to mpi might be checked.
  gcry_mpi_t our_mpi = NULL;
  gcry_mpi_t their_mpi = NULL;
  if (gcry_mpi_scan(&our_mpi, GCRYMPI_FMT_USG, our, sizeof(ec_public_key_t),
                    NULL)) {
    gcry_mpi_release(our_mpi);
    gcry_mpi_release(their_mpi);
    return NULL;
  }

  if (gcry_mpi_scan(&their_mpi, GCRYMPI_FMT_USG, their, sizeof(ec_public_key_t),
                    NULL)) {
    gcry_mpi_release(our_mpi);
    gcry_mpi_release(their_mpi);
    return NULL;
  }

  int cmp = gcry_mpi_cmp(our_mpi, their_mpi);
  if (cmp > 0) {
    ret->sending = ratchet->chain_a;
    ret->receiving = ratchet->chain_b;
  } else if (cmp < 0) {
    ret->sending = ratchet->chain_b;
    ret->receiving = ratchet->chain_a;
  }

  gcry_mpi_release(our_mpi);
  gcry_mpi_release(their_mpi);

  return ret;
}

tstatic int key_manager_get_sending_chain_key(chain_key_t sending,
                                              const key_manager_t *manager) {
  message_chain_t *chain = decide_between_chain_keys(
      manager->current, manager->our_ecdh->pub, manager->their_ecdh);
  const chain_link_t *last = chain_get_last(chain->sending);
  memcpy(sending, last->key, sizeof(chain_key_t));
  free(chain);
  chain = NULL;

  return last->id;
}

tstatic chain_link_t *chain_link_new() {
  chain_link_t *l = malloc(sizeof(chain_link_t));
  if (l == NULL)
    return NULL;

  l->id = 0;
  l->next = NULL;

  return l;
}

tstatic chain_link_t *derive_next_chain_link(chain_link_t *previous) {
  chain_link_t *l = chain_link_new();
  if (l == NULL)
    return NULL;

  hash_hash(l->key, sizeof(chain_key_t), previous->key, sizeof(chain_key_t));

  // TODO: the previous is still needed for the MK
  sodium_memzero(previous->key, CHAIN_KEY_BYTES);

  l->id = previous->id + 1;
  previous->next = l;
  return l;
}

tstatic otrv4_err_t rebuild_chain_keys_up_to(int message_id,
                                             const chain_link_t *head) {
  chain_link_t *last = (chain_link_t *)chain_get_last(head);

  int j = 0;
  for (j = last->id; j < message_id; j++) {
    last = derive_next_chain_link(last);
    if (last == NULL)
      return ERROR;
  }

  return SUCCESS;
}

tstatic otrv4_err_t key_manager_get_receiving_chain_key(
    chain_key_t receiving, int message_id, const key_manager_t *manager) {

  message_chain_t *chain = decide_between_chain_keys(
      manager->current, manager->our_ecdh->pub, manager->their_ecdh);
  if (rebuild_chain_keys_up_to(message_id, chain->receiving) == ERROR) {
    free(chain);
    chain = NULL;
    return ERROR;
  }

  const chain_link_t *link = chain_get_by_id(message_id, chain->receiving);
  free(chain);
  chain = NULL;

  if (link == NULL)
    return ERROR; /* message id not found. Should have been generated at
                        rebuild_chain_keys_up_to */

  memcpy(receiving, link->key, sizeof(chain_key_t));

  return SUCCESS;
}

INTERNAL void
otrv4_ecdh_shared_secret_from_prekey(uint8_t *shared,
                                     otrv4_shared_prekey_pair_t *shared_prekey,
                                     const ec_point_t their_pub) {
  decaf_448_point_t s;
  decaf_448_point_scalarmul(s, their_pub, shared_prekey->priv);

  otrv4_ec_point_serialize(shared, s);
}

INTERNAL void
otrv4_ecdh_shared_secret_from_keypair(uint8_t *shared, otrv4_keypair_t *keypair,
                                      const ec_point_t their_pub) {
  decaf_448_point_t s;
  decaf_448_point_scalarmul(s, their_pub, keypair->priv);

  otrv4_ec_point_serialize(shared, s);
}

tstatic void calculate_shared_secret(shared_secret_t dst, const k_ecdh_t k_ecdh,
                                     const brace_key_t brace_key) {
  decaf_shake256_ctx_t hd;

  hash_init_with_dom(hd);
  hash_update(hd, k_ecdh, sizeof(k_ecdh_t));
  hash_update(hd, brace_key, sizeof(brace_key_t));

  hash_final(hd, dst, sizeof(shared_secret_t));
  hash_destroy(hd);
}

tstatic void calculate_shared_secret_from_tmp_key(shared_secret_t dst,
                                                  const uint8_t *tmp_k) {
  decaf_shake256_ctx_t hd;

  hash_init_with_dom(hd);
  hash_update(hd, tmp_k, HASH_BYTES);

  hash_final(hd, dst, sizeof(shared_secret_t));
  hash_destroy(hd);
}

tstatic otrv4_err_t derive_sending_chain_key(key_manager_t *manager) {
  message_chain_t *chain = decide_between_chain_keys(
      manager->current, manager->our_ecdh->pub, manager->their_ecdh);
  chain_link_t *last = (chain_link_t *)chain_get_last(chain->sending);
  free(chain);
  chain = NULL;
  (void)last;

  // TODO: seems to be wrong
  chain_link_t *l = derive_next_chain_link(last);
  if (l == NULL)
    return ERROR;

  // TODO: assert l->id == manager->j
  return SUCCESS;
}

tstatic void calculate_ssid(key_manager_t *manager,
                            const shared_secret_t shared) {
  uint8_t magic[1] = {0x00};
  uint8_t ssid_buff[32];

  shake_256_kdf(ssid_buff, sizeof ssid_buff, magic, shared,
                sizeof(shared_secret_t));

  memcpy(manager->ssid, ssid_buff, sizeof manager->ssid);
}

tstatic void calculate_extra_key(key_manager_t *manager,
                                 const chain_key_t chain_key) {
  uint8_t magic[1] = {0xFF};
  uint8_t extra_key_buff[HASH_BYTES];

  shake_256_kdf(extra_key_buff, HASH_BYTES, magic, chain_key,
                sizeof(chain_key_t));

  memcpy(manager->extra_key, extra_key_buff, sizeof manager->extra_key);
}

tstatic otrv4_err_t calculate_brace_key(key_manager_t *manager) {
  k_dh_t k_dh;

  if (manager->i % 3 == 0) {
    if (otrv4_dh_shared_secret(k_dh, sizeof(k_dh_t), manager->our_dh->priv,
                               manager->their_dh) == ERROR)
      return ERROR;

    hash_hash(manager->brace_key, sizeof(brace_key_t), k_dh, sizeof(k_dh_t));

  } else {
    hash_hash(manager->brace_key, sizeof(brace_key_t), manager->brace_key,
              sizeof(brace_key_t));
  }

  return SUCCESS;
}

tstatic otrv4_err_t enter_new_ratchet(key_manager_t *manager) {
  k_ecdh_t k_ecdh;
  shared_secret_t shared;

  otrv4_ecdh_shared_secret(k_ecdh, manager->our_ecdh, manager->their_ecdh);

  if (calculate_brace_key(manager) == ERROR)
    return ERROR;

  calculate_shared_secret(shared, k_ecdh, manager->brace_key);

#ifdef DEBUG
  printf("ENTERING NEW RATCHET\n");
  printf("K_ecdh = ");
  otrv4_memdump(k_ecdh, sizeof(k_ecdh_t));
  printf("brace_key = ");
  otrv4_memdump(manager->brace_key, sizeof(brace_key_t));
#endif

  if (key_manager_new_ratchet(manager, shared) == ERROR) {
    sodium_memzero(shared, SHARED_SECRET_BYTES);
    sodium_memzero(manager->ssid, sizeof(manager->ssid));
    sodium_memzero(manager->extra_key, sizeof(manager->extra_key));
    return ERROR;
  }

  sodium_memzero(shared, SHARED_SECRET_BYTES);
  return SUCCESS;
}

tstatic otrv4_err_t init_ratchet(key_manager_t *manager, bool interactive) {
  k_ecdh_t k_ecdh;
  shared_secret_t shared;

  otrv4_ecdh_shared_secret(k_ecdh, manager->our_ecdh, manager->their_ecdh);

  if (calculate_brace_key(manager) == ERROR)
    return ERROR;

  if (interactive)
    calculate_shared_secret(shared, k_ecdh, manager->brace_key);
  else
    calculate_shared_secret_from_tmp_key(shared, manager->tmp_key);

#ifdef DEBUG
  printf("ENTERING NEW RATCHET\n");
  printf("K_ecdh = ");
  otrv4_memdump(k_ecdh, sizeof(k_ecdh_t));
  printf("mixed_key = ");
  otrv4_memdump(manager->brace_key, sizeof(brace_key_t));
#endif

  calculate_ssid(manager, shared);
  if (gcry_mpi_cmp(manager->our_dh->pub, manager->their_dh) > 0) {
    manager->ssid_half = SESSION_ID_SECOND_HALF_BOLD;
  } else {
    manager->ssid_half = SESSION_ID_FIRST_HALF_BOLD;
  }

#ifdef DEBUG
  printf("THE SECURE SESSION ID\n");
  printf("ssid: \n");
  if (manager->ssid_half == SESSION_ID_FIRST_HALF_BOLD) {
    printf("the first 32 = ");
    for (unsigned int i = 0; i < 4; i++) {
      printf("0x%08x ", manager->ssid[i]);
    }
  } else {
    printf("\n");
    printf("the last 32 = ");
    for (unsigned int i = 4; i < 8; i++) {
      printf("0x%08x ", manager->ssid[i]);
    }
    printf("\n");
    printf("the 32 = ");
  }
#endif

  if (key_manager_new_ratchet(manager, shared) == ERROR) {
    sodium_memzero(shared, SHARED_SECRET_BYTES);
    sodium_memzero(manager->ssid, sizeof(manager->ssid));
    sodium_memzero(manager->extra_key, sizeof(manager->extra_key));
    sodium_memzero(manager->tmp_key, sizeof(manager->tmp_key));
    return ERROR;
  }

  sodium_memzero(shared, SHARED_SECRET_BYTES);
  /* tmp_k is no longer needed */
  sodium_memzero(manager->tmp_key, HASH_BYTES);

  return SUCCESS;
}

INTERNAL otrv4_err_t otrv4_key_manager_ratcheting_init(int j, bool interactive,
                                                       key_manager_t *manager) {
  if (init_ratchet(manager, interactive))
    return ERROR;

  manager->i = 0;
  manager->j = j;
  return SUCCESS;
}

tstatic otrv4_err_t rotate_keys(key_manager_t *manager) {
  manager->i++;
  manager->j = 0;

  if (otrv4_key_manager_generate_ephemeral_keys(manager))
    return ERROR;

  return enter_new_ratchet(manager);
}

INTERNAL otrv4_err_t
otrv4_key_manager_ensure_on_ratchet(key_manager_t *manager) {
  if (manager->j == 0)
    return SUCCESS;

  manager->i++;
  if (enter_new_ratchet(manager))
    return ERROR;

  // Securely delete priv keys as no longer needed
  otrv4_ec_scalar_destroy(manager->our_ecdh->priv);
  if (manager->i % 3 == 0) {
    otrv4_dh_priv_key_destroy(manager->our_dh);
  }

  return SUCCESS;
}

tstatic void derive_encryption_and_mac_keys(m_enc_key_t enc_key,
                                            m_mac_key_t mac_key,
                                            const chain_key_t chain_key) {
  uint8_t magic1[1] = {0x1};
  uint8_t magic2[1] = {0x2};

  shake_256_kdf(enc_key, sizeof(m_enc_key_t), magic1, chain_key,
                sizeof(chain_key_t));
  shake_256_kdf(mac_key, sizeof(m_mac_key_t), magic2, enc_key,
                sizeof(m_enc_key_t));
}

INTERNAL otrv4_err_t otrv4_key_manager_retrieve_receiving_message_keys(
    m_enc_key_t enc_key, m_mac_key_t mac_key, int message_id,
    key_manager_t *manager) {
  chain_key_t receiving;

  if (key_manager_get_receiving_chain_key(receiving, message_id, manager) ==
      ERROR)
    return ERROR;

  derive_encryption_and_mac_keys(enc_key, mac_key, receiving);
  calculate_extra_key(manager, receiving);

#ifdef DEBUG
  printf("GOT SENDING KEYS:\n");
  printf("receiving enc_key = ");
  otrv4_memdump(enc_key, sizeof(m_enc_key_t));
  printf("receiving mac_key = ");
  otrv4_memdump(mac_key, sizeof(m_mac_key_t));
#endif

  return SUCCESS;
}

tstatic otrv4_bool_t should_ratchet(const key_manager_t *manager) {
  if (manager->j == 0)
    return otrv4_true;

  return otrv4_false;
}

INTERNAL otrv4_err_t
otrv4_key_manager_prepare_next_chain_key(key_manager_t *manager) {
  if (should_ratchet(manager) == otrv4_true) {
    return rotate_keys(manager);
  }

  return derive_sending_chain_key(manager);
}

INTERNAL otrv4_err_t otrv4_key_manager_retrieve_sending_message_keys(
    m_enc_key_t enc_key, m_mac_key_t mac_key, key_manager_t *manager) {
  chain_key_t sending;
  int message_id = key_manager_get_sending_chain_key(sending, manager);

  derive_encryption_and_mac_keys(enc_key, mac_key, sending);
  calculate_extra_key(manager, sending);

#ifdef DEBUG
  printf("GOT SENDING KEYS:\n");
  printf("sending enc_key = ");
  otrv4_memdump(enc_key, sizeof(m_enc_key_t));
  printf("sending mac_key = ");
  otrv4_memdump(mac_key, sizeof(m_mac_key_t));
#endif

  if (message_id == manager->j) {
    return SUCCESS;
  }

  sodium_memzero(enc_key, sizeof(m_enc_key_t));
  sodium_memzero(mac_key, sizeof(m_mac_key_t));
  return ERROR;
}

INTERNAL uint8_t *
otrv4_key_manager_old_mac_keys_serialize(list_element_t *old_mac_keys) {
  uint num_mac_keys = otrv4_list_len(old_mac_keys);
  size_t serlen = num_mac_keys * MAC_KEY_BYTES;
  if (serlen == 0) {
    return NULL;
  }

  uint8_t *ser_mac_keys = malloc(serlen);
  if (!ser_mac_keys) {
    return NULL;
  }

  for (unsigned int i = 0; i < num_mac_keys; i++) {
    list_element_t *last = otrv4_list_get_last(old_mac_keys);
    memcpy(ser_mac_keys + i * MAC_KEY_BYTES, last->data, MAC_KEY_BYTES);
    old_mac_keys = otrv4_list_remove_element(last, old_mac_keys);
    otrv4_list_free_full(last);
  }

  otrv4_list_free_nodes(old_mac_keys);

  return ser_mac_keys;
}

INTERNAL void otrv4_key_manager_set_their_ecdh(ec_point_t their,
                                               key_manager_t *manager) {
  otrv4_ec_point_copy(manager->their_ecdh, their);
}

INTERNAL void otrv4_key_manager_set_their_dh(dh_public_key_t their,
                                             key_manager_t *manager) {
  otrv4_dh_mpi_release(manager->their_dh);
  manager->their_dh = otrv4_dh_mpi_copy(their);
}
